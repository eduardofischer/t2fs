#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/t2fs.h"
#include "../include/fs_aux.h"


// Hash Function
unsigned int hash(const char *str, int tablesize){
    int value = 0;

    int i;
    for(i=0; i< strlen(str); i++)
        value += toupper(str[i]) - 'A';

    return value % tablesize;
}

DIRENT2* findHashEntry(DIRENT2 *table[], const char *key){
    unsigned int index = hash(key, DIR_HASHTABLE_SIZE);
    DIRENT2 *it = table[index];
    DIRENT2 disk;

    // Try to find if a matching key in the list exists
    do{
        if(strcmp(it->name, key) == 0)
            return it;
        disk = readDirEnt(it->next);
        it = &disk;
    } while(it->next != INVALID_PTR);

    return INVALID_PTR;
}

int insertHashEntry(DIRENT2 *table[], DIRENT2 *file){
    if(findEntry(table, file->name) == INVALID_PTR) {
        // Find the desired linked list
        unsigned int index = hash(file->name, DIR_HASHTABLE_SIZE);
        DIRENT2 *newEnt = malloc(sizeof(DIRENT2*));

        memcpy(newEnt, file, sizeof(DIRENT2));

        // Add the new key and link to the front of the list
        newEnt->next = table[index];
        table[index] = newEnt;
    } else{
        return -1; // Entrada já existe na tabela
    }
    return 0;
}