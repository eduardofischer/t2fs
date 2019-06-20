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

DIRENTRY* findHashEntry(DIRENTRY *table, const char *key){
    int index = hash(key, superBlock.hashTableSize);
    DIRENTRY *it = malloc(sizeof(DIRENTRY));
    memcpy(it, &(table[index]), sizeof(DIRENTRY));

    if(it->fileType == (BYTE)INVALID_PTR)
        return NULL; 

    if(strcmp(it->name, key) == 0)
        return it;

    while(it->next != (BYTE)INVALID_PTR) {
        DIRENTRY next = readDirEnt(it->next);
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
                DIRENTRY next = readDirEnt(it->next);
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