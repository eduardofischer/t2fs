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

    char str[] = "../teste/eduardo/file.txt";

    char **pathArray = decodePath(str);
    int arraySize = sizeof(pathArray)/sizeof(char);

    printf("Array size %d\n", arraySize);

    int i;
    for(i=0; i<arraySize; i++){
        printf("Item %d: %s\n", i, pathArray[i]);
    }

    return 0;
}

