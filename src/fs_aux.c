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

    printf("Hash Table Size: %d\n", newSB.hashTableSize);

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
    invalidFile.block = INVALID_PTR;

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

DIRENTRY readDirEnt(WORD addr){
    WORD sector = superBlock.entDirAreaStart + 1 + (BYTE)(addr);
    WORD offset = (BYTE)(addr >> 8)*sizeof(DIRENTRY);
    BYTE *buffer = malloc(SECTOR_SIZE);
    DIRENTRY ent;

    read_sector((unsigned int)sector, buffer);
    memcpy(&ent, buffer + offset, sizeof(DIRENTRY));

    free(buffer);

    return ent;
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
    
    // Entrada .
    strcpy((char *)this, ".");
    this->fileType = DIRECTORY_FT;
    this->fileSize = sizeof(DIRENTRY) * superBlock.hashTableSize;
    this->firstBlock = 0;
    this->next = INVALID_PTR;

    insertHashEntry(table, this);

    if(writeData(0, table, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
        return -1;

    return 0;
}

char** decodePath(char* path, int *count){
    (*count) = 0;
    char** pathArray = malloc(2*sizeof(char*));
    char *token;
    const char s[2] = "/";
    int i=1, j=1;

    pathArray[0] = malloc(2*sizeof(char*));
    if(path[0] == '/'){
        strcpy(pathArray[0], "/");
        (*count)++;
    }else if(path[0] == '.' && path[1] == '.' && path[2] == '/'){
        i=0;
    }else if(path[0] == '.' && path[1] == '/'){
        i=0;
    }else{ 
        strcpy(pathArray[0], ".");
        (*count)++;
    }

    token = strtok(path, s);

    while(token != NULL){
        pathArray = realloc(pathArray, (j+1)*sizeof(char*));
        pathArray[i] = malloc(strlen(token)+1);
        strcpy(pathArray[i], token);
        token = strtok(NULL, (char *)"/");
        (*count)++;
        i++;
        j++;
    }
    
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

FILE2 createFile(char *name){
    OFILE newFile;
    WORD addr = getFATFreeAddr();
    FILE2 handle = getFreeFileHandle();
    BYTE cwdHandle = superBlock.cwdHandle;
    DIRENTRY *dirTable = readData(openDirs[cwdHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);

    if((strcmp(name, "/") | strcmp(name, ".") | strcmp(name, "..")) == 0)
        return -1;

    newFile.pointer = 0;
    newFile.block = addr;
    newFile.firstBlock = addr;

    DIRENTRY *dirEntry = malloc(sizeof(DIRENTRY));
    strcpy((char*)&(dirEntry->name), name);
    dirEntry->fileType = REGULAR_FT;
    dirEntry->fileSize = 0;
    dirEntry->firstBlock = addr;
    dirEntry->next = INVALID_PTR;

    if(insertHashEntry(dirTable, dirEntry) != 0)
        return -2; // Caso o arquivo já exista [FALTA IMPLEMENTAR]

    if(writeData(openDirs[cwdHandle].block, dirTable, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
        return -3;

    openFiles[handle] = newFile;

    return handle;
}

DIR2 createDir(char *pathname){
    ODIR newDir;
    WORD addr = getFATFreeAddr();
    FILE2 handle = getFreeDirHandle();
    BYTE cwdHandle = superBlock.cwdHandle;
    DIRENTRY *dirTable = readData(openDirs[cwdHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
    DIRENTRY *newDirTable = createDirTable();

    if((strcmp(pathname, "/") | strcmp(pathname, ".") | strcmp(pathname, "..")) == 0)
        return -1;

    newDir.readCount = 0;
    newDir.block = addr;

    DIRENTRY *dirEntry = malloc(sizeof(DIRENTRY));
    strcpy((char*)&(dirEntry->name), pathname);
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

    if(writeData(openDirs[cwdHandle].block, dirTable, sizeof(DIRENTRY) * superBlock.hashTableSize) != 0)
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

int loadDirHandle(DIRENTRY dir){
    ODIR newDir;
    int handle = getFreeDirHandle();

    if(handle < 0)
        return -1;

    newDir.readCount = 0;
    newDir.block = dir.firstBlock;

    openDirs[handle] = newDir;

    return handle;
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
    char *pathcpy = malloc(strlen(pathname));
    strcpy(pathcpy, pathname);
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
        strcpy(newPath, toAbsolutePath(pathcpy));
    }
    
    for(i=j; i<branchLength; i++){
        if(strcmp(branch[i], ".") != 0){
            dir = findHashEntry(dirTable, branch[i]);
            if(dir == NULL)
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
    char *pathcpy = malloc(strlen(pathname));
    strcpy(pathcpy, pathname);
    char **branch = decodePath(pathname, &branchLength);
    DIRENTRY *dirTable, *dir;
    DIR2 dirHandle;
    char *newPath = malloc(128);
    int i, j=0;

    if(strcmp(branch[0], "/") == 0){
        dirTable = readData(openDirs[0].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        strcpy(newPath, pathcpy);
        strcpy(superBlock.cwdPath, newPath);
        superBlock.cwdHandle = 0;
        j++;
    } else {
        dirTable = readData(openDirs[superBlock.cwdHandle].block, sizeof(DIRENTRY) * superBlock.hashTableSize);
        strcpy(newPath, toAbsolutePath(pathcpy));
    }
    
    for(i=j; i<branchLength; i++){
        if(strcmp(branch[i], ".") != 0){
            dir = findHashEntry(dirTable, branch[i]);
            if(dir == NULL)
                return -1;

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

    return 0;
}



