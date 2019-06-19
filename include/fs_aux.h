#ifndef __FSAUX__
#define __FSAUX__

#include "../include/apidisk.h"
#include "../include/t2fs.h"

int hasInit;

// 31 7E -> 7E 31

// Carrega os dados do MBR
int diskInit();

// Retorna o tamanho da partição em setores
DWORD getPartitionSize();

// Cria o superbloco na partição
int createSuperblock(int sectorsPerBlock); 

// Carrega os dados do superbloco
int initFS();

// Cria uma nova tabela FAT na partição com todos os campos iguais a -1
int createFAT();

// Escreve em uma entrada da FAT
int setFAT(int block, DWORD value);

// Lê uma entrada da FAT
DWORD getFAT(int block);

// Retorna um bloco livre
WORD getFATFreeAddr();

// Cria o bitmap das entradas livres no espaço reservado aos DIRENT2
int createDirEntBitMap();

// Retorna um endereço livre no bitmap
WORD getDirEntFreeAddr();

// Seta um byte do bitmap
int setDirEntAddr(WORD addr, BYTE value);

// Lê uma entrada de diretório no disco
DIRENT2 readDirEnt(WORD addr);

// Salva uma entrada de diretório no disco e retorna o endereço
WORD writeDirEnt(DIRENT2 *ent);

// Escreve em um bloco de dados do disco
int writeData(WORD block, void *data, int size);

// Lê de um bloco de dados do disco
void* readData(WORD block, int size);

// Cria o diretorio raiz no primeiro bloco de dados [BLOCO 0]
int createRootDir();

// Quebra uma pathstring em uma sequencia de caminhos relativos
char** decodePath(char* path);

FILE2 getFreeFileHandle();

char* getCWD();

FILE2 createFile(char *name);

#endif