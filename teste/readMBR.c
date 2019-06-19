#include <stdio.h>
#include <stdlib.h>
#include "../include/fs_aux.h"

int main(int argc, char const *argv[]){
    if(diskInit() != 0){
        printf("Error at diskInit\n");
        return -1;
    }    

    printf("\nDisk version: %d\n", diskMBR.diskVersion);
    printf("Sector size: %d\n", diskMBR.sectorSize);
    printf("Partition table first byte: %d\n", diskMBR.partitionTableStart);
    printf("Number of partitions: %d\n\n", diskMBR.numberPartitions);

    int i;
    for(i=0; i<4; i++){
        printf("Partition %d name: %s\n", i, diskMBR.partition[i].name);
        printf("Partition %d size: %u\n", i, getPartitionSize(i));
        printf("Partition %d first sector: %u\n\n", i, diskMBR.partition[i].startAddress);
    }

    
    return 0;
}

