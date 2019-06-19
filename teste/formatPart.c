#include <stdio.h>
#include <stdlib.h>
#include "../include/fs_aux.h"

int main(int argc, char const *argv[]){
    if(format2(1) != 0){
        printf("Error at format2\n");
        return -1;
    }

    printf("Particao formatada com sucesso!\n");

    return 0;
}

