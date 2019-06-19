#include <stdio.h>
#include <stdlib.h>
#include "../include/fs_aux.h"

int main(int argc, char const *argv[]){
    if(initFS() != 0){
        printf("Error at initFS\n");
        return -1;
    }

    printf("Disco inicializado com sucesso\n");

    printf("256 Before write %x\n", getFAT(256));

    printf("Get free addr: %i\n", getFATFreeAddr());

    printf("Set 256 to 0x33333333\n");
    if(setFAT(0, 0x33333333) != 0){
        printf("Error write FAT\n");
        return -1;
    }

    printf("256 After write %x\n", getFAT(256));

    printf("Get free addr: %i\n", getFATFreeAddr());

    printf("Set FAT 0 to 50\n");
    int i;
    for(i=0; i<=50; i++){
        setFAT(i, i);
    }

    printf("Get free addr: %i\n", getFATFreeAddr());

    

    return 0;
}

