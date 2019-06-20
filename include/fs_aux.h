#ifndef __FSAUX__
#define __FSAUX__

#include "../include/apidisk.h"
#include "../include/t2fs.h"

int hasInit;

#define WORKING_PART 0 // Partição utilizada pelo FS
#define FILE_NAME_MAX_LENGTH 31+1
#define MAX_NUMBER_FILES 64
#define MAX_NUMBER_OPEN_FILES 20

#define FAT_FREE_CHAR 0xFFFFFFFF
#define FAT_LAST_BLOCK_CHAR 0xDDDDDDDD
#define BITMAP_FREE_CHAR 0xEE
#define BITMAP_TAKEN_CHAR 0xAA

#define REGULAR_FT 0x01
#define DIRECTORY_FT 0x02

// Open Files
typedef struct {
	char name[FILE_NAME_MAX_LENGTH+1];
	int pointer;
	int firstBlock;
	int size;
	WORD dirBlock;
} OFILE;

OFILE openFiles[MAX_NUMBER_OPEN_FILES];

// Open Directories
typedef struct {
	int readCount;
	int block;
} ODIR;

ODIR openDirs[20];

// Partition Table Entry
typedef struct {
	DWORD	startAddress;
	DWORD	endAddress;
	BYTE 	name[24];
} PARTTE;

// Master Boot Record data
typedef struct {
    WORD	diskVersion;
    WORD	sectorSize;
    WORD	partitionTableStart;
    WORD	numberPartitions;
	PARTTE	partition[4];
} MBR;

MBR diskMBR;

// Superblock
typedef struct {
    WORD	sectorsPerBlock;
    WORD	fatStart;
    WORD	fatSize;
    WORD    entDirAreaStart;
    WORD    entDirAreaSize; // Número de setores reservados para armazenamento de ENTDIR2 (Entradas de diretório)
	WORD	dataBlocksAreaStart; // Início da área de blocos de dados
	WORD 	dataBlocksAreaSize;
	WORD 	numberOfDataBlocks;
	WORD	hashTableSize;
	BYTE	cwdHandle;
	char 	cwdPath[128];
} SUPERBLOCK;

SUPERBLOCK superBlock;

typedef struct {
    char    name[MAX_FILE_NAME_SIZE+1]; /* Nome do arquivo cuja entrada foi lida do disco      */
    BYTE    fileType;                   /* Tipo do arquivo: regular (0x01) ou diret�rio (0x02) */
    DWORD   fileSize;                   /* Numero de bytes do arquivo                      */

	WORD	firstBlock; // Primeiro bloco de dados do arquivo [regular] ou bloco do diretório
	WORD   	next;		// Ponteiro para a próxima entrada (Linked List) ENDEREÇO NO DISCO
} DIRENTRY;

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

// Cria o bitmap das entradas livres no espaço reservado aos DIRENTRY
int createDirEntBitMap();

// Retorna um endereço livre no bitmap
WORD getDirEntFreeAddr();

// Seta um byte do bitmap
int setDirEntAddr(WORD addr, BYTE value);

// Lê uma entrada de diretório no disco
DIRENTRY *readDirEnt(WORD addr);

// Salva uma entrada de diretório no disco e retorna o endereço
WORD writeDirEnt(DIRENTRY *ent);

WORD writeDirEntAtAddr(DIRENTRY *ent, WORD addr);

// Escreve em um bloco de dados do disco
int writeData(WORD block, void *data, int size);

// Lê de um bloco de dados do disco
void* readData(WORD block, int size);

// Cria o diretorio raiz no primeiro bloco de dados [BLOCO 0]
int createRootDir();

// Quebra uma pathstring em uma sequencia de caminhos relativos
char** decodePath(char* path, int *count);

FILE2 getFreeFileHandle();

char* getCWD();

DWORD getDirTableBlock(char *pathname, int parent, char *filename);

FILE2 createFile(char *pathname);

DIR2 createDir(char *pathname);

int readDir(DIR2 handle, DIRENT2 *dentry);

DIR2 openDirectory(char *pathname);

int changeDirectory(char *pathname);

char *toAbsolutePath(char *path);

int removeDirectory(char *pathname);

FILE2 openFile(char *pathname);

int writeToFile(FILE2 handle, char *buffer, int size);

int readFile(FILE2 handle, char *buffer, int size);

int seek(FILE2 handle, DWORD offset);

int truncateFile(FILE2 handle);

int closeFile(FILE2 handle);

int freeAllFileBlocks(DWORD firstAddr);

int deleteFile(char *pathname);

#endif