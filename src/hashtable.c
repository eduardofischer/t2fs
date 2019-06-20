#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/t2fs.h"
#include "../include/fs_aux.h"


// Hash Function
int hash(const char *str, int tablesize){
    int value = 0;

    int i;
    for(i=0; i< strlen(str); i++)
        value += str[i]/3;

    return value % tablesize;
}

DIRENTRY* findHashEntry(DIRENTRY *table, const char *key){
    int index = hash(key, superBlock.hashTableSize);
    DIRENTRY *it = malloc(sizeof(DIRENTRY));
    memcpy(it, &(table[index]), sizeof(DIRENTRY));

    if(it->fileType == (BYTE)INVALID_PTR)
        return NULL; 

    if(strcmp(it->name, key) == 0)
        return it;

    while(it->next != (BYTE)INVALID_PTR) {
        DIRENTRY next = *readDirEnt(it->next);
        memcpy(it, &next, sizeof(DIRENTRY));
        if(strcmp(it->name, key) == 0)
            return it;    
    };

    return NULL;
}

int insertHashEntry(DIRENTRY *table, DIRENTRY *file){
    DIRENTRY *hashTable = table;
    if(findHashEntry(hashTable, file->name) == NULL) {
        int index = hash(file->name, superBlock.hashTableSize);
        if(table[index].fileType == (BYTE)INVALID_PTR){
            file->next = (BYTE)INVALID_PTR;
            table[index] = *file;
        }else{
            file->next = table[index].next;
            table[index].next = writeDirEnt(file);
        }
    } else{
        return -1; // Entrada já existe na tabela
    }
    return 0;
}

int updateHashEntry(DIRENTRY *table, DIRENTRY *file){
    int index = hash(file->name, superBlock.hashTableSize);
    DIRENTRY *it = malloc(sizeof(DIRENTRY));
    memcpy(it, &(table[index]), sizeof(DIRENTRY));

    if(it->fileType == (BYTE)INVALID_PTR)
        return -2; 

    // Caso a entrada esteja armazenada na tabela
    if(strcmp(it->name, file->name) == 0){
        memcpy(&(table[index]), file, sizeof(DIRENTRY));
        return 0;
    }

    WORD saveAddr;

    // Caso esteja armazenada em Linked List
    while(it->next != (BYTE)INVALID_PTR) {
        printf("Preso aqui\n");
        DIRENTRY *next = readDirEnt(it->next);
        saveAddr = it->next;
        memcpy(it, next, sizeof(DIRENTRY));
        if(strcmp(it->name, file->name) == 0)
            writeDirEntAtAddr(file, saveAddr);         
    };

    return 0;
}

int removeHashEntry(DIRENTRY *table, DIRENTRY *file){
    int index = hash(file->name, superBlock.hashTableSize);
    DIRENTRY *it = malloc(sizeof(DIRENTRY));
    memcpy(it, &(table[index]), sizeof(DIRENTRY));

    DIRENTRY empty;
    memset(empty.name, 0, MAX_FILE_NAME_SIZE+1);
    empty.fileType = INVALID_PTR;
    empty.fileSize = INVALID_PTR;
    empty.firstBlock = INVALID_PTR;
    empty.next = INVALID_PTR;

    if(it->fileType == (BYTE)INVALID_PTR)
        return -2; 

    // Caso a entrada esteja armazenada na tabela
    if(strcmp(it->name, file->name) == 0){
        memcpy(&(table[index]), &empty, sizeof(DIRENTRY));
        return 0;
    }

    DIRENTRY *prev = &(table[index]);
    int prevInTable = 1;
    WORD prevAddr, savePrev;

    // Caso esteja armazenada em Linked List
    while(it->next != (BYTE)INVALID_PTR) {
        DIRENTRY *next = readDirEnt(it->next);
        savePrev = it->next;
        memcpy(it, next, sizeof(DIRENTRY));
        if(strcmp(it->name, file->name) == 0){
            if(prevInTable == 0){
                prev = readDirEnt(prevAddr);
                prev->next = it->next;
                writeDirEntAtAddr(prev, prevAddr);
            }               
            prev->next = it->next;
            
            setDirEntAddr(prevAddr, BITMAP_FREE_CHAR);
        }
        prevAddr = savePrev;
        prevInTable = 0;
    };

    return 0;
}

DIRENTRY *getNthEntry(DIRENTRY *table, int n){
    int i=0, found=0;
    DIRENTRY *it = malloc(sizeof(DIRENTRY));

    while(i < superBlock.hashTableSize){
        memcpy(it, &(table[i]), sizeof(DIRENTRY));
        if(it->fileType == (BYTE)INVALID_PTR){
            i++;
        }else{
            found++;
            if(found == n+1)
                return it;

            while(it->next != (BYTE)INVALID_PTR) {
                DIRENTRY next = *readDirEnt(it->next);
                memcpy(it, &next, sizeof(DIRENTRY));
                found++;
                if(found == n+1)
                    return it;  
            };
            i++;
        }
    }

    return NULL;
}