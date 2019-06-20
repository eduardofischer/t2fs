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

    int count;
    char str[] = "teste/eduardo/file.txt";

    char **pathArray = decodePath(str, &count);

    printf("Array size %d\n", count);

    int i;
    for(i=0; i<count; i++){
        printf("Item %d: %s\n", i, pathArray[i]);
    }

    return 0;
}

