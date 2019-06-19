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
    newSB.entDirAreaSize = (MAX_NUMBER_FILES * sizeof(DIRENT2))/SECTOR_SIZE;

    // O espaço restante é então ocupado por blocos de dados
    newSB.dataBlocksAreaStart = newSB.entDirAreaStart + newSB.entDirAreaSize;
    newSB.dataBlocksAreaSize = getPartitionSize(WORKING_PART) - 1 - newSB.fatSize - newSB.entDirAreaSize;
    newSB.numberOfDataBlocks = newSB.dataBlocksAreaSize / newSB.sectorsPerBlock;

    BYTE buffer[SECTOR_SIZE] = {0};
    memcpy(buffer, &newSB, sizeof(SUPERBLOCK));

    return write_sector(diskMBR.partition[WORKING_PART].startAddress, buffer);
}

int initFS(){
    BYTE buffer[SECTOR_SIZE] = {0};

    diskInit();

    if(read_sector(diskMBR.partition[WORKING_PART].startAddress, buffer) != 0)
        return -1;

    memcpy(&superBlock, buffer, sizeof(SUPERBLOCK));

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
    int entriesPerSector = SECTOR_SIZE/sizeof(DWORD);

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
    int entriesPerSector = SECTOR_SIZE/sizeof(DIRENT2);

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
    int entriesPerSector = SECTOR_SIZE/sizeof(DIRENT2);
    int offset = (BYTE)(addr) * entriesPerSector + (BYTE)(addr >> 8);

    if(read_sector(sector, buffer) != 0)
        return -1;

    memcpy(buffer + offset, &value, sizeof(BYTE));

    if(write_sector(sector, buffer) != 0)
        return -1;

    free(buffer);

    return 0;
}

DIRENT2 readDirEnt(WORD addr){
    WORD sector = superBlock.entDirAreaStart + 1 + (BYTE)(addr);
    WORD offset = (BYTE)(addr >> 8)*sizeof(DIRENT2);
    BYTE *buffer = malloc(SECTOR_SIZE);
    DIRENT2 ent;

    read_sector((unsigned int)sector, buffer);
    memcpy(&ent, buffer + offset, sizeof(DIRENT2));

    free(buffer);

    return ent;
}

WORD writeDirEnt(DIRENT2 *ent){
    WORD freeAddr = getDirEntFreeAddr();
    BYTE *buffer = malloc(SECTOR_SIZE);

    if(freeAddr < 0){
        printf("\nO limite de arquivos em disco foi atingido.\n");
        return -1;
    }

    int sector = superBlock.entDirAreaStart + 1 + (BYTE)(freeAddr);
    int offset = (BYTE)(freeAddr >> 8)*sizeof(DIRENT2);

    if(read_sector(sector, buffer) != 0)
        return -2;

    memcpy(buffer + offset, ent, sizeof(DIRENT2));

    if(write_sector(sector, buffer) != 0)
        return -2;

    if(setDirEntAddr(freeAddr, BITMAP_TAKEN_CHAR) != 0)
        return -3;

    free(buffer);

    return freeAddr;
}

int writeData(WORD block, void *data, int size){
    int firstSector = superBlock.dataBlocksAreaStart + block / superBlock.sectorsPerBlock;
    BYTE *buffer = malloc(SECTOR_SIZE * superBlock.sectorsPerBlock);
    memset(buffer, 0, SECTOR_SIZE * superBlock.sectorsPerBlock);

    memcpy(buffer, data, size);

    int i;
    for(i=0; i<superBlock.sectorsPerBlock; i++){
        if(write_sector(firstSector + i, buffer + SECTOR_SIZE*i) != 0)
            return -1;
        setFAT(block, FAT_LAST_BLOCK_CHAR);
    }

    free(buffer);

    return 0;
}

void* readData(WORD block, int size){
    int firstSector = superBlock.dataBlocksAreaStart + block / superBlock.sectorsPerBlock;
    BYTE *buffer = malloc(SECTOR_SIZE * superBlock.sectorsPerBlock);
    BYTE *data = malloc(size);

    int i;
    for(i=0; i<superBlock.sectorsPerBlock; i++){
        if(read_sector(firstSector + i, buffer + SECTOR_SIZE*i) != 0)
            return -1;
    }

    memcpy(data, buffer, size);

    free(buffer);

    return data;
}

int createRootDir(){
    DIRENT2 table[DIR_HASHTABLE_SIZE];
    DIRENT2 this;
    
    // Entrada .
    strcpy((char *)&this.name, "/");
    this.fileType = DIRECTORY_FT;
    this.fileSize = sizeof(table);
    this.firstBlock = 0;
    this.next = INVALID_PTR;

    table[0] = this;

    if(writeData(0, &table, sizeof(table)) != 0)
        return -1;

    return 0;
}



