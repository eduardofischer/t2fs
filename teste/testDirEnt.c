#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/fs_aux.h"

int main(int argc, char const *argv[]){
    if(initFS() != 0){
        printf("Error at initFS\n");
        return -1;
    }

    printf("Disco inicializado com sucesso\n");


    DIRENTRY *test = malloc(sizeof(DIRENTRY));
    strcpy((char *)&(test->name), "Diretorio 9");
    test->fileType = 0x02;
    test->fileSize = 100;
    test->next = -1;

    WORD addr = writeDirEnt(test);

    printf("Writing dirEnt at %x\n", addr);

    free(test);

    printf("Reading dirEnt at %x\n", addr);
    DIRENTRY readDire = readDirEnt(addr);

    printf("Reading dirEnt name: %s\n", readDire.name);

    printf("Teste do Root\n");

    DIRENTRY *root = readData(0, sizeof(DIRENTRY));
    printf("name: %s\n", root->name);
    printf("fileType: %x\n", root->fileType);
    printf("fileSize: %u\n", root->fileSize);
    printf("firstBlock: %x\n", root->firstBlock);
    printf("next: %x\n", root->next);

    return 0;
}

