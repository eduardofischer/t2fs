#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/fs_aux.h"
#include "../include/apidisk.h"
#include "../include/t2fs.h"
#include "../include/hashtable.h"

int hasInit = -1;

WORD bufferToWord(BYTE* buffer){
    return (WORD) ((WORD)buffer[0] | (WORD)buffer[1] << 8);
}

DWORD bufferToDword(BYTE* buffer) {
    return (DWORD) ((DWORD)buffer[0] | (DWORD)buffer[1] << 8 |(DWORD)buffer[2] << 16 |(DWORD)buffer[3] << 24 );
}

int diskInit(){
    BYTE buffer[SECTOR_SIZE] = {0};

    if(read_sector(0, buffer) != 0)
        return -1;

    memcpy(&diskMBR, buffer, sizeof(MBR));
    
    return 0;
}

DWORD getPartitionSize(int part){
    return diskMBR.partition[part].endAddress - diskMBR.partition[part].startAddress + 1;
};

int createSuperblock(int sectorsPerBlock){
    SUPERBLOCK newSB;

    newSB.sectorsPerBlock = sectorsPerBlock;

    // O primeiro bloco da partição é reservado ao superbloco [0]
    // Portanto a FAT inicia no bloco [1]
    newSB.fatStart = diskMBR.partition[WORKING_PART].startAddress + 1;
    newSB.fatSize = 64/sectorsPerBlock;

    // Logo após a FAT, se inicia uma área reservada ao armazenamento de entradas de diretório
    // O primeiro setor dessa área é utilizado como bitmap (bytemap nesse caso)
    newSB.entDirAreaStart = newSB.fatStart + newSB.fatSize;
    newSB.entDirAreaSize = (MAX_NUMBER_FILES * sizeof(DIRENTRY))/SECTOR_SIZE;

    // O espaço restante é então ocupado por blocos de dados
    newSB.dataBlocksAreaStart = newSB.entDirAreaStart + newSB.entDirAreaSize;
    newSB.dataBlocksAreaSize = getPartitionSize(WORKING_PART) - 1 - newSB.fatSize - newSB.entDirAreaSize;
    newSB.numberOfDataBlocks = newSB.dataBlocksAreaSize / newSB.sectorsPerBlock;

    newSB.hashTableSize = sectorsPerBlock * SECTOR_SIZE / sizeof(DIRENTRY);

    //printf("Hash Table Size: %d\n", newSB.hashTableSize);

    // Diretório corrente padrão
    newSB.cwdHandle = 0;
    strcpy(newSB.cwdPath, "/");

    BYTE buffer[SECTOR_SIZE] = {0};
    memcpy(buffer, &newSB, sizeof(SUPERBLOCK));

    return write_sector(diskMBR.partition[WORKING_PART].startAddress, buffer);
}

int initOpenStructs(){
    int f, d;
    int nOpenFiles = sizeof(openFiles) / sizeof(OFILE);
    int nOpenDirs = sizeof(openDirs) / sizeof(ODIR);

    OFILE invalidFile;
    invalidFile.pointer = INVALID_PTR;
    invalidFile.firstBlock = INVALID_PTR;
    invalidFile.size = INVALID_PTR;
    invalidFile.dirBlock = INVALID_PTR;

    ODIR invalidDir;
    invalidDir.readCount = INVALID_PTR;
    invalidDir.block = INVALID_PTR;

    for(f=0; f<nOpenFiles; f++)
        openFiles[f] = invalidFile;

    for(d=1; d<nOpenDirs; d++)
        openDirs[d] = invalidDir;

    ODIR root;
    root.readCount = 0;
    root.block = 0;
    openDirs[0] = root;

    return 0;
}

int initFS(){
    BYTE buffer[SECTOR_SIZE] = {0};

    diskInit();

    if(read_sector(diskMBR.partition[WORKING_PART].startAddress, buffer) != 0)
        return -1;

    memcpy(&superBlock, buffer, sizeof(SUPERBLOCK));

    superBlock.cwdHandle = 0;
    strcpy(superBlock.cwdPath, "/");

    initOpenStructs();

    hasInit = 1;

    return 0;
}

int createFAT(){
    BYTE *newFAT = malloc(SECTOR_SIZE);
    memset(newFAT, FAT_FREE_CHAR, SECTOR_SIZE);

    int i;
    int firstSector = superBlock.fatStart;
    for(i=0; i<superBlock.fatSize; i++)
        if(write_sector(firstSector + i, newFAT) != 0)
            return -1;

    free(newFAT);

    return 0;
}

int setFAT(int block, DWORD value){
    BYTE *buffer = malloc(SECTOR_SIZE);

    int firstSector = superBlock.fatStart;
    int entriesPerSector = SECTOR_SIZE/sizeof(DWORD);
    int entrySector = firstSector + (block / entriesPerSector);
    
    if(read_sector(entrySector, buffer) != 0)
        return -1;

    memcpy(buffer + (block % entriesPerSector)*sizeof(value), &value, sizeof(value));

    if(write_sector(entrySector, buffer) != 0)
        return -2;

    free(buffer);

    return 0;
}

DWORD getFAT(int block){
    BYTE *buffer = malloc(SECTOR_SIZE);

    int firstSector = superBlock.fatStart;
    int entriesPerSector = SECTOR_SIZE/sizeof(DWORD);
    int entrySector = firstSector + (block / entriesPerSector);
    
    if(read_sector(entrySector, buffer) != 0)
        return -1;

    DWORD value;
    memcpy(&value, buffer + (block % entriesPerSector)*sizeof(value), sizeof(value));

    free(buffer);

    return value;
}

WORD getFATFreeAddr(){
    BYTE *buffer = malloc(SECTOR_SIZE);
    int i=0, j;
    int sector = superBlock.fatStart; 

    while(i < superBlock.numberOfDataBlocks){
        j=0;
        read_sector(sector + i, buffer);
        while(j < SECTOR_SIZE){
            if(bufferToDword(buffer + j) == FAT_FREE_CHAR){
                free(buffer);
                return j/4;
            }          

            j += 4;
        }
        i++;
    }
    
    free(buffer);

    return -1;
}

int createDirEntBitMap(){
    BYTE *empty = malloc(SECTOR_SIZE);
    memset(empty, BITMAP_FREE_CHAR, SECTOR_SIZE);

    unsigned int sector = superBlock.entDirAreaStart; 
    if(write_sector(sector, empty) != 0)
        return -1;

    free(empty);

    return 0;
}

WORD getDirEntFreeAddr(){
    BYTE *buffer = malloc(SECTOR_SIZE);
    int j=0;
    int sector = superBlock.entDirAreaStart; 
    int entriesPerSector = SECTOR_SIZE/sizeof(DIRENTRY);

    BYTE dirEntSector;
    BYTE dirEntOffset;

    read_sector(sector, buffer);
    while(j < SECTOR_SIZE){
        if(buffer[j] == BITMAP_FREE_CHAR){
            free(buffer);
            dirEntSector = j / entriesPerSector;
            dirEntOffset = j % entriesPerSector;
            return (WORD)dirEntOffset << 8 | (WORD)dirEntSector;
        }
        j++;
    }

    free(buffer);

    return -1;
}

int setDirEntAddr(WORD addr, BYTE value){
    BYTE *buffer = malloc(SECTOR_SIZE);
    int sector = superBlock.entDirAreaStart; 
    int entriesPerSector = SECTOR_SIZE/sizeof(DIRENTRY);
    int offset = (BYTE)(addr) * entriesPerSector + (BYTE)(addr >> 8);

    if(read_sector(sector, buffer) != 0)
        return -1;

    memcpy(buffer + offset, &value, sizeof(BYTE));

    if(write_sector(sector, buffer) != 0)
        return -1;

    free(buffer);

    return 0;
}

DIRENTRY *readDirEnt(WORD addr){
    WORD sector = superBlock.entDirAreaStart + 1 + (BYTE)(addr);
    WORD offset = (BYTE)(addr >> 8)*sizeof(DIRENTRY);
    BYTE *buffer = malloc(SECTOR_SIZE);

    read_sector((unsigned int)sector, buffer);

    return (DIRENTRY*)(buffer + offset);
}

WORD writeDirEnt(DIRENTRY *ent){
    WORD freeAddr = getDirEntFreeAddr();
    BYTE *buffer = malloc(SECTOR_SIZE);

    if(freeAddr < 0){
        printf("\nO limite de arquivos em disco foi atingido.\n");
        return -1;
    }

    int sector = superBlock.entDirAreaStart + 1 + (BYTE)(freeAddr);
    int offset = (BYTE)(freeAddr >> 8)*sizeof(DIRENTRY);

    if(read_sector(sector, buffer) != 0)
        return -2;

    memcpy(buffer + offset, ent, sizeof(DIRENTRY));

    if(write_sector(sector, buffer) != 0)
        return -2;

    if(setDirEntAddr(freeAddr, BITMAP_TAKEN_CHAR) != 0)
        return -3;

    free(buffer);

    return freeAddr;
}

WORD writeDirEntAtAddr(DIRENTRY *ent, WORD addr){
    WORD freeAddr = addr;
    BYTE *buffer = malloc(SECTOR_SIZE);

    if(freeAddr < 0){
        printf("\nO limite de arquivos em disco foi atingido.\n");
        return -1;
    }

    int sector = superBlock.entDirAreaStart + 1 + (BYTE)(freeAddr);
    int offset = (BYTE)(freeAddr >> 8)*sizeof(DIRENTRY);

    if(read_sector(sector, buffer) != 0)
        return -2;

    memcpy(buffer + offset, ent, sizeof(DIRENTRY));

    if(write_sector(sector, buffer) != 0)
        return -2;

    if(setDirEntAddr(freeAddr, BITMAP_TAKEN_CHAR) != 0)
        return -3;

    free(buffer);

    return freeAddr;
}

int writeData(WORD block, void *data, int size){
    int firstSector = superBlock.dataBlocksAreaStart + block * superBlock.sectorsPerBlock;
    BYTE *buffer = malloc(SECTOR_SIZE * superBlock.sectorsPerBlock);
    memset(buffer, -1, SECTOR_SIZE * superBlock.sectorsPerBlock);

    memcpy(buffer, data, size);

    int i;
    for(i=0; i<superBlock.sectorsPerBlock; i++)
        if(write_sector(firstSector + i, buffer + SECTOR_SIZE*i) != 0)
            return -1;      
    

    setFAT(block, FAT_LAST_BLOCK_CHAR);

    free(buffer);

    return 0;
}

void* readData(WORD block, int size){
    int firstSector = superBlock.dataBlocksAreaStart + block * superBlock.sectorsPerBlock;
    BYTE *buffer = malloc(SECTOR_SIZE * superBlock.sectorsPerBlock);
    BYTE *data = malloc(size);

    int i;
    for(i=0; i<superBlock.sectorsPerBlock; i++)
        if(read_sector(firstSector + i, buffer + SECTOR_SIZE*i) != 0)
            return NULL;
    
    memcpy(data, buffer, size);

    free(buffer);

    return (void*)data;
}

DIRENTRY* createDirTable(){
    DIRENTRY *table = malloc(superBlock.hashTableSize*sizeof(DIRENTRY));
    int i;
    DIRENTRY empty;
    memset(empty.name, 0, MAX_FILE_NAME_SIZE+1);
    empty.fileType = INVALID_PTR;
    empty.fileSize = INVALID_PTR;
    empty.firstBlock = INVALID_PTR;
    empty.next = INVALID_PTR;
    for(i=0; i<superBlock.hashTableSize; i++){
        table[i] = empty;
    }

    return table;
}

int createRootDir(){
    DIRENTRY *table = createDirTable();
    DIRENTRY *this = malloc(sizeof(DIRENTRY));
    
    // Entrada ..
    strcpy((char *)this, "..");
    this->fileType = DIRECTORY_FT;
    this->fileSize = sizeof(DIRENTRY) * superBlock.hashTableSize;
    this->firstBlock = 0;
    this->next = INVALID_PTR;

    insertHashEntry(table, this);

    // Entrada .
    strcpy((char *)this, ".");
    insertHashEntry(table, this);

    if(writeData(0, table, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
        return -1;

    return 0;
}

char** decodePath(char* path, int *count){
    char** pathArray = malloc(2*sizeof(char*));
    char *pathcpy = malloc(strlen(path) + 1);
    strcpy(pathcpy, path);
    char *token;
    const char s[2] = "/";
    (*count) = 0;
    int i=1, j=1;

    pathArray[0] = malloc(2*sizeof(char*));
    if(pathcpy[0] == '/'){
        strcpy(pathArray[0], "/");
        (*count)++;
    }else if(pathcpy[0] == '.' && pathcpy[1] == '.' && pathcpy[2] == '/'){
        i=0;
    }else if(pathcpy[0] == '.' && pathcpy[1] == '/'){
        i=0;
    }else if(pathcpy[0] == '.'){
        i=0;
    }else{ 
        strcpy(pathArray[0], ".");
        (*count)++;
    }

    token = strtok(pathcpy, s);

    while(token != NULL){
        pathArray = realloc(pathArray, (j+1)*sizeof(char*));
        pathArray[i] = malloc(strlen(token)+1);
        strcpy(pathArray[i], token);
        token = strtok(NULL, (char *)"/");
        (*count)++;
        i++;
        j++;
    }

    free(pathcpy);
    
    return pathArray;
}

FILE2 getFreeFileHandle(){
    int i;
    int nHandles = sizeof(openFiles) / sizeof(OFILE);

    for(i=0; i<nHandles; i++)
        if(openFiles[i].pointer == INVALID_PTR)
            return i;

    return -1;
}

FILE2 getFreeDirHandle(){
    int i;
    int nHandles = sizeof(openDirs) / sizeof(ODIR);

    for(i=0; i<nHandles; i++)
        if(openDirs[i].readCount == INVALID_PTR)
            return i;

    return -1;
}

DIRENTRY getCWDDirEnt(){
    BYTE cwdHandle = superBlock.cwdHandle;
    DIRENTRY *dirTable = readData(openDirs[cwdHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);

    return *dirTable;
}

char* getCWD(){
    return superBlock.cwdPath;
}

FILE2 createFile(char *pathname){
    OFILE newFile;
    WORD addr = getFATFreeAddr();
    FILE2 handle = getFreeFileHandle();
    char *name = malloc(FILE_NAME_MAX_LENGTH+1);
    DWORD dirTableAddr = getDirTableBlock(pathname, 1, name);
    DIRENTRY *dirTable = readData(dirTableAddr, sizeof(DIRENTRY) * superBlock.hashTableSize);

    if((int)dirTableAddr < 0)
        return -1;

    if((strcmp(pathname, "/") | strcmp(pathname, ".") | strcmp(pathname, "..")) == 0)
        return -4;

    strcpy((char*)&(newFile.name), name);
    newFile.pointer = 0;
    newFile.firstBlock = addr;
    newFile.size = 0;
    newFile.dirBlock = dirTableAddr;

    DIRENTRY *dirEntry = malloc(sizeof(DIRENTRY));
    strcpy((char*)&(dirEntry->name), name);
    dirEntry->fileType = REGULAR_FT;
    dirEntry->fileSize = 0;
    dirEntry->firstBlock = addr;
    dirEntry->next = INVALID_PTR;

    setFAT(addr, FAT_LAST_BLOCK_CHAR);

    if(insertHashEntry(dirTable, dirEntry) != 0){      
        if(deleteFile(pathname) == 0)
            return createFile(pathname);         
        else
            return -2;
    }

    if(writeData(dirTableAddr, dirTable, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
        return -3;

    openFiles[handle] = newFile;

    return handle;
}

DIR2 createDir(char *pathname){
    ODIR newDir;
    WORD addr = getFATFreeAddr();
    FILE2 handle = getFreeDirHandle();
    BYTE cwdHandle = superBlock.cwdHandle;
    char *name = malloc(FILE_NAME_MAX_LENGTH+1);
    DWORD dirTableAddr = getDirTableBlock(pathname, 1, name);
    DIRENTRY *newDirTable = createDirTable();
    DIRENTRY *dirTable = readData(dirTableAddr, sizeof(DIRENTRY) * superBlock.hashTableSize);

    if((int)dirTableAddr < 0)
        return -1;

    if((strcmp(pathname, "/") | strcmp(pathname, ".") | strcmp(pathname, "..")) == 0)
        return -1;

    newDir.readCount = 0;
    newDir.block = addr;

    DIRENTRY *dirEntry = malloc(sizeof(DIRENTRY));
    strcpy((char*)&(dirEntry->name), name);
    dirEntry->fileType = DIRECTORY_FT;
    dirEntry->fileSize = superBlock.sectorsPerBlock * SECTOR_SIZE;
    dirEntry->firstBlock = addr;
    dirEntry->next = INVALID_PTR;

    if(insertHashEntry(dirTable, dirEntry) != 0)
        return -2; // Diretório já existe

    DIRENTRY this, parent;
    // .
    strcpy((char *)&(this.name), ".");
    this.fileType = DIRECTORY_FT;
    this.fileSize = superBlock.sectorsPerBlock * SECTOR_SIZE;
    this.firstBlock = addr;
    this.next = INVALID_PTR;
    if(insertHashEntry(newDirTable, &this) != 0)
        return -2;
    // ..
    strcpy((char *)&(parent.name), "..");
    parent.fileType = DIRECTORY_FT;
    parent.fileSize = superBlock.sectorsPerBlock * SECTOR_SIZE;
    parent.firstBlock = openDirs[cwdHandle].block;
    parent.next = INVALID_PTR;
    if(insertHashEntry(newDirTable, &parent) != 0)
        return -2;

    if(writeData(addr, newDirTable, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
        return -3;
    
    setFAT(addr, FAT_LAST_BLOCK_CHAR);

    if(writeData(dirTableAddr, dirTable, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
        return -3;

    openDirs[handle] = newDir;

    return handle;
}

int readDir(DIR2 handle, DIRENT2 *dentry){
    DIRENTRY *dirTable = readData(openDirs[handle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
    DIRENTRY *found = getNthEntry(dirTable, openDirs[handle].readCount);

    if(found == NULL){
        openDirs[handle].readCount = 0;
        return -1;
    }
        
    openDirs[handle].readCount++;
    
    dentry->fileType = found->fileType;
    dentry->fileSize = found->fileSize;
    strcpy((char *)&(dentry->name), (char *)&(found->name));

    return 0;
}

int isFileOpen(DIRENTRY file){
    int f;
    int nOpenFiles = sizeof(openFiles) / sizeof(OFILE);
    for(f=0; f<nOpenFiles; f++)
        if(openFiles[f].firstBlock == file.firstBlock)
            return f;

    return -1;
}

int isDirOpen(DIRENTRY dir){
    int d;
    int nOpenDirs = sizeof(openDirs) / sizeof(ODIR);
    for(d=0; d<nOpenDirs; d++)
        if(openDirs[d].block == dir.firstBlock)
            return d;
    
    return -1;
}

int loadDirHandle(DIRENTRY dir){
    ODIR newDir;
    int handle = isDirOpen(dir);

    if(handle >= 0)
        return handle;

    handle = getFreeDirHandle();

    if(handle < 0)
        return -1;

    newDir.readCount = 0;
    newDir.block = dir.firstBlock;

    openDirs[handle] = newDir;

    return handle;
}

int loadFileHandle(DIRENTRY file, WORD dirBlock){
    OFILE newFile;
    int handle = isFileOpen(file);

    if(handle >= 0)
        return handle;

    handle = getFreeFileHandle();

    if(handle < 0)
        return -1;

    strcpy((char*)&(newFile.name), file.name);
    newFile.pointer = 0;
    newFile.firstBlock = file.firstBlock;
    newFile.size = file.fileSize;
    newFile.dirBlock = dirBlock;

    openFiles[handle] = newFile;

    return handle;
}

int closeDirectory(DIR2 handle){
    ODIR invalidDir;
    invalidDir.readCount = INVALID_PTR;
    invalidDir.block = INVALID_PTR;

    if(handle < sizeof(openDirs)/sizeof(ODIR))
        openDirs[handle] = invalidDir;
    else
        return -1;   

    return 0;
}

int closeDir(DIR2 handle){
    return 0;
}

int closeAllDirectories(){
    int d;
    int nOpenDirs = sizeof(openDirs) / sizeof(ODIR);

    for(d=1; d<nOpenDirs; d++)
        if(superBlock.cwdHandle != d)
            closeDirectory(d);    

    return 0;
}

DWORD getDirTableBlock(char *pathname, int parent, char *filename){
    int branchLength = 0;
    char **branch = decodePath(pathname, &branchLength);
    DIRENTRY *dir, *dirTable;
    DIR2 dirHandle = superBlock.cwdHandle;
    int i;

    // Testa se o path é absoluto ou relativo e carrega a tabela de diretório correspondente
    if(strcmp(branch[0], "/") == 0)
        dirTable = readData(openDirs[0].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
    else
        dirTable = readData(openDirs[superBlock.cwdHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
    
    for(i=0; i<branchLength - parent; i++){
        if(strcmp(branch[i], ".") != 0){
            if(strcmp(branch[i], "/") == 0)
                strcpy(branch[i], ".");

            dir = findHashEntry(dirTable, branch[i]);

            // Caso o diretório seja inválido
            if(dir == NULL || dir->fileType != DIRECTORY_FT)
                return -1;             

            dirHandle = loadDirHandle(*dir);
            if(dirHandle < 0)
                return -2;    

            if(openDirs[dirHandle].block == INVALID_PTR)
                return -3;

            dirTable = (DIRENTRY*)readData(openDirs[dirHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        }  
    }

    strcpy(filename, branch[branchLength - 1]);

    return openDirs[dirHandle].block;
}

int openPathDirs(){
    int branchLength = 0;
    char **branch = decodePath(superBlock.cwdPath, &branchLength);
    DIRENTRY *dirTable, *dir;
    DIR2 dirHandle;
    int i;

    dirTable = readData(openDirs[0].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
    
    // Percorre o path
    for(i=1; i<branchLength; i++){
        dir = findHashEntry(dirTable, branch[i]);

        // Caso o diretório seja inválido
        if(dir == NULL || dir->fileType != DIRECTORY_FT)
            return -1;

        dirHandle = loadDirHandle(*dir);
        if(dirHandle < 0)
            return -2;    

        dirTable = (DIRENTRY*)readData(openDirs[dirHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
    }

    return 0;
}

char *toAbsolutePath(char *path){
    int countCwd = 0, countPath = 0;
    char **tokensCwd = decodePath(superBlock.cwdPath, &countCwd);
    char **tokensPath = decodePath(path, &countPath);
    char *absolute = malloc(128);
    strcpy(absolute, "/");

    int i;
    int j=0;
    int ptr = 0;

    // Corrige os deslocamentos .. e .
    for(i=0; i<countPath; i++){
        if(strcmp(tokensPath[i], "..") == 0){
            countCwd--;
            j++;
        }
        if(strcmp(tokensPath[i], ".") == 0){
            j++;
        }
        
    }

    // Monta a string final
    for(i=1; i<countCwd; i++){
        strcpy(absolute + ptr, "/");
        ptr++;
        strcpy(absolute + ptr, tokensCwd[i]);
        ptr += strlen(tokensCwd[i]);
    }

    

    for(i=j; i<countPath; i++){
        strcpy(absolute + ptr, "/");
        ptr++;
        strcpy(absolute + ptr, tokensPath[i]);
        ptr += strlen(tokensPath[i]);
    }
    
    return absolute;
}

DIR2 openDirectory(char *pathname){
    int branchLength = 0;
    char **branch = decodePath(pathname, &branchLength);
    DIRENTRY *dirTable, *dir;
    DIR2 dirHandle;
    char *newPath = malloc(128);
    int i, j=0;

    if(strcmp(branch[0], "/") == 0){
        dirTable = readData(openDirs[0].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        strcpy(newPath, pathname);
        j++;
    } else {
        dirTable = readData(openDirs[superBlock.cwdHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        strcpy(newPath, toAbsolutePath(pathname));
    }
    
    for(i=j; i<branchLength; i++){
        if(strcmp(branch[i], ".") != 0){
            dir = findHashEntry(dirTable, branch[i]);
            if(dir == NULL || dir->fileType != DIRECTORY_FT)
                return -1;

            dirHandle = loadDirHandle(*dir);
            if(dirHandle < 0)
                return -2;    

            if(openDirs[dirHandle].block == INVALID_PTR)
                return -3;

            dirTable = (DIRENTRY*)readData(openDirs[dirHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        } else{
            dirHandle = superBlock.cwdHandle;
        }
    }

    return dirHandle;
}

int changeDirectory(char *pathname){
    int branchLength = 0;
    char **branch = decodePath(pathname, &branchLength);
    DIRENTRY *dirTable, *dir;
    DIR2 dirHandle;
    char *newPath = malloc(128);
    int i;

    // Salva o diretório inicial
    char *cwdPath = malloc(strlen(superBlock.cwdPath)+1);
    strcpy(cwdPath, superBlock.cwdPath);
    int cwdHandle = superBlock.cwdHandle;

    // Testa se o path é absoluto ou relativo e carrega a tabela de diretório correspondente
    if(strcmp(branch[0], "/") == 0){
        dirTable = readData(openDirs[0].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        strcpy(newPath, pathname);
    } else {
        dirTable = readData(openDirs[superBlock.cwdHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        strcpy(newPath, toAbsolutePath(pathname));
    }
    
    for(i=0; i<branchLength; i++){
        if(strcmp(branch[i], ".") != 0){
            if(strcmp(branch[i], "/") == 0)
                strcpy(branch[i], ".");

            dir = findHashEntry(dirTable, branch[i]);

            // Caso o diretório seja inválido, restaura o diretório inicial
            if(dir == NULL || dir->fileType != DIRECTORY_FT){
                strcpy(superBlock.cwdPath, cwdPath);
                superBlock.cwdHandle = cwdHandle;
                return -1;
            }       

            dirHandle = loadDirHandle(*dir);
            if(dirHandle < 0)
                return -2;    

            if(openDirs[dirHandle].block == INVALID_PTR)
                return -3;

            dirTable = (DIRENTRY*)readData(openDirs[dirHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);

            strcpy(superBlock.cwdPath, newPath);
            superBlock.cwdHandle = dirHandle;
        }  
    }

    closeAllDirectories();
    openPathDirs();

    return 0;
}

int removeDirectory(char *pathname){
    int branchLength = 0;
    char **branch = decodePath(pathname, &branchLength);
    DIRENTRY *dirTable, *dir;
    DIR2 dirHandle;
    WORD parentDir = openDirs[superBlock.cwdHandle].block;
    int i;

    if(branchLength == 1 && (strcmp(branch[0], "/") && strcmp(branch[0], ".") && strcmp(branch[0], "..")) == 0)
        return -1;

    // Testa se o path é absoluto ou relativo e carrega a tabela de diretório correspondente
    if(strcmp(branch[0], "/") == 0){
        dirTable = readData(openDirs[0].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
    } else {
        dirTable = readData(openDirs[superBlock.cwdHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
    }
    
    // Percorre o path até chegar no diretório pai
    for(i=0; i<branchLength-1; i++){
        if(strcmp(branch[i], ".") != 0){
            if(strcmp(branch[i], "/") == 0)
                strcpy(branch[i], ".");

            dir = findHashEntry(dirTable, branch[i]);

            // Caso o diretório seja inválido
            if(dir == NULL || dir->fileType != DIRECTORY_FT)
                return -6;

            dirHandle = loadDirHandle(*dir);
            if(dirHandle < 0)
                return -7;    

            if(openDirs[dirHandle].block == INVALID_PTR)
                return -8;

            dirTable = (DIRENTRY*)readData(openDirs[dirHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
            parentDir = openDirs[dirHandle].block;
        }  
    }

    closeAllDirectories();
    openPathDirs();

    // Tratamento do diretório a ser removido

    dir = findHashEntry(dirTable, branch[i]);

    // Caso o diretório seja inválido
    if(dir == NULL || dir->fileType != DIRECTORY_FT)
        return -1;

    // Verifica se o diretório está vazio
    if(getNthEntry(dirTable, 3) != NULL)
        return -2;

    if(isDirOpen(*dir) == 0)
        return -3; // Erro: diretório aberto

    if(removeHashEntry(dirTable, dir) != 0)
        return -4;

    if(writeData(parentDir, dirTable, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
        return -5;

    setFAT(dir->firstBlock, FAT_FREE_CHAR);

    return 0;
}

FILE2 openFile(char *pathname){
    int branchLength = 0;
    char **branch = decodePath(pathname, &branchLength);
    DIRENTRY *dirTable, *dir, *file;
    DIR2 dirHandle;
    int i, j=0;

    if(strcmp(branch[0], "/") == 0){
        dirTable = readData(openDirs[0].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        j++;
    } else {
        dirTable = readData(openDirs[superBlock.cwdHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
    }
    
    // Navega até o diretório pai
    for(i=j; i<branchLength-1; i++){
        if(strcmp(branch[i], ".") != 0){
            dir = findHashEntry(dirTable, branch[i]);
            if(dir == NULL || dir->fileType != DIRECTORY_FT)
                return -1;

            dirHandle = loadDirHandle(*dir);
            if(dirHandle < 0)
                return -2;    

            if(openDirs[dirHandle].block == INVALID_PTR)
                return -3;

            dirTable = (DIRENTRY*)readData(openDirs[dirHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        } else{
            dirHandle = superBlock.cwdHandle;
        }
    }

    closeAllDirectories();
    openPathDirs();

    file = findHashEntry(dirTable, branch[i]);

    if(file == NULL || file->fileType != REGULAR_FT)
        return -1;

    return loadFileHandle(*file, openDirs[dirHandle].block);
}

int updateFileDirEntry(FILE2 handle){
    OFILE file = openFiles[handle];
    DIRENTRY *dirTable = readData(file.dirBlock, sizeof(DIRENTRY) * superBlock.hashTableSize);
    DIRENTRY *fileEntry = findHashEntry(dirTable, file.name);

    fileEntry->fileSize = file.size;
    fileEntry->firstBlock = file.firstBlock;

    if(updateHashEntry(dirTable, fileEntry) != 0)
        return -1;

    if(writeData(file.dirBlock, dirTable, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
        return -2;
    
    return 0;
}

WORD getFileNthBlock(FILE2 handle, int n){
    DWORD addr;
    DWORD next;

    if(openFiles[handle].firstBlock == INVALID_PTR){
        openFiles[handle].firstBlock = getFATFreeAddr();
        setFAT(openFiles[handle].firstBlock, FAT_LAST_BLOCK_CHAR);
    }

    addr = openFiles[handle].firstBlock;

    int i;
    for(i=1; i<n; i++){
        if(getFAT(addr) == FAT_LAST_BLOCK_CHAR){
            next = getFATFreeAddr();
            setFAT(addr, next);
        }
        addr = next;
    }
    setFAT(addr, FAT_LAST_BLOCK_CHAR);
        
    return addr;
}

int writeToFile(FILE2 handle, char *buffer, int size){
    OFILE *file = &(openFiles[handle]);

    if(file->pointer == INVALID_PTR)
        return -1;

    int bytesToWrite = size;
    int startPointer = file->pointer;
    int blockSize = superBlock.sectorsPerBlock * SECTOR_SIZE;
    int nBlocks = 1 + ((file->pointer % blockSize) + size) / blockSize;
    int fileBlock, diskBlock, offset;
    
    BYTE *blockBuffer;

    int i, writeSize;
    for(i=1; i <= nBlocks; i++){
        fileBlock = file->pointer / blockSize;
        offset = file->pointer % blockSize;

        diskBlock = getFileNthBlock(handle, fileBlock + i);

        blockBuffer = readData(diskBlock, blockSize);

        if(bytesToWrite < (blockSize - offset))
            writeSize = bytesToWrite;
        else
            writeSize = blockSize - offset;
        
        memcpy(blockBuffer + offset, buffer, writeSize);

        if(writeData(diskBlock, blockBuffer, blockSize) != 0)
            return -1;

        file->pointer += writeSize;
        file->size += writeSize;
        bytesToWrite -= writeSize;
    }

    return file->pointer - startPointer;
}

int readFile(FILE2 handle, char *buffer, int size){
    OFILE *file = &(openFiles[handle]);

    if(file->pointer == INVALID_PTR)
        return -1;

    if(file->pointer >= file->size)
        return 0;

    int bytesToRead = size;
    int startPointer = file->pointer;
    int blockSize = superBlock.sectorsPerBlock * SECTOR_SIZE;
    int nBlocks = 1 + ((file->pointer % blockSize) + size) / blockSize;
    int fileBlock, diskBlock, offset;

    BYTE *blockBuffer;



    int i, readSize;
    for(i=1; i <= nBlocks; i++){
        fileBlock = file->pointer / blockSize;
        offset = file->pointer % blockSize;

        diskBlock = getFileNthBlock(handle, fileBlock + i);

        blockBuffer = readData(diskBlock, blockSize);

        if(bytesToRead < (blockSize - offset))
            readSize = bytesToRead;
        else
            readSize = blockSize - offset;

        if((file->pointer - startPointer) + readSize > file->size - file->pointer)
            readSize = file->size - file->pointer;
        
        memcpy(buffer, blockBuffer + offset, readSize);

        file->pointer += readSize;
        bytesToRead -= readSize;
    }

    return file->pointer - startPointer;
}

int seek(FILE2 handle, DWORD offset){
    OFILE *file = &openFiles[handle];

    if(handle < 0 || handle >= MAX_NUMBER_OPEN_FILES)
        return -1;

    if(offset == (DWORD)INVALID_PTR)
        file->pointer = file->size;
    else
        file->pointer = offset;
    
    return 0;
}

int closeFile(FILE2 handle){
    if(handle < 0 || handle >= sizeof(openFiles)/sizeof(OFILE))
        return -1;

    if(updateFileDirEntry(handle) != 0)
        return -1;

    OFILE invalidFile;
    invalidFile.dirBlock = INVALID_PTR;
    invalidFile.firstBlock = INVALID_PTR;
    invalidFile.pointer = INVALID_PTR;
    invalidFile.size = INVALID_PTR;

    openFiles[handle] = invalidFile;

    return 0;
}

int truncateFile(FILE2 handle){
    OFILE *file = &openFiles[handle];

    if(handle < 0 || handle >= MAX_NUMBER_OPEN_FILES)
        return -1;

    file->size = file->pointer;
    
    return 0;
}

int freeAllFileBlocks(DWORD firstAddr){
    DWORD next = getFAT(firstAddr);
        
    setFAT(firstAddr, FAT_FREE_CHAR);

    while(next != FAT_LAST_BLOCK_CHAR){ 
        setFAT(next, FAT_FREE_CHAR);
        next = getFAT(next);
    }
    
    return 0;
}

int deleteFile(char *pathname){
    char *name = malloc(FILE_NAME_MAX_LENGTH+1);
    DWORD dirTableAddr = getDirTableBlock(pathname, 1, name);
    DIRENTRY *dirTable = readData(dirTableAddr, sizeof(DIRENTRY) * superBlock.hashTableSize);
    DIRENTRY *file = findHashEntry(dirTable, name);

    if(file == NULL)
        return -1;
    if(file->fileType != REGULAR_FT)
        return -2;
    if((int)dirTableAddr < 0)
        return -3;
    if(isFileOpen(*file) >= 0)
        return -4;
    if(freeAllFileBlocks(file->firstBlock) != 0)
        return -5;
    if(removeHashEntry(dirTable, file) != 0)
        return -6;
    if(writeData(dirTableAddr, dirTable, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
        return -7;
    return 0;
}





