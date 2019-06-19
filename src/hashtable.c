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
        value += str[i];

    return value % tablesize;
}

DIRENT2* findHashEntry(DIRENT2 *table, const char *key){
    int index = hash(key, superBlock.hashTableSize);
    DIRENT2 *it = malloc(sizeof(DIRENT2));
    memcpy(it, &(table[index]), sizeof(DIRENT2));

    if(it->fileType == (BYTE)INVALID_PTR)
        return NULL; 

    if(strcmp(it->name, key) == 0)
        return it;

    while(it->next != (BYTE)INVALID_PTR) {
        DIRENT2 next = readDirEnt(it->next);
        memcpy(it, &next, sizeof(DIRENT2));
        if(strcmp(it->name, key) == 0)
            return it;    
    };

    return NULL;
}

int insertHashEntry(DIRENT2 *table, DIRENT2 *file){
    DIRENT2 *hashTable = table;
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