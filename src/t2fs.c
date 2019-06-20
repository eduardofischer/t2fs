#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/t2fs.h"
#include "../include/apidisk.h"
#include "../include/fs_aux.h"

/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {
	strncpy (name, "Eduardo Spitzer Fischer    | 00290399\nMaria Flavia Borrajo Tondo | 00278892\nRodrigo Paranhos Bastos    | 00261162\0", size);
	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente o disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho 
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2 (int sectors_per_block) {
	if(diskInit() != 0) 
		return -1;

	if(createSuperblock(sectors_per_block) != 0)
		return -2;

	if(initFS() != 0)
		return -3;

	if(createFAT() != 0)
		return -4;

	if(createDirEntBitMap() != 0)
		return -5;

	if(createRootDir() != 0)
		return -6;

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um 
		arquivo já existente, o mesmo terá seu conteúdo removido e 
		assumirá um tamanho de zero bytes.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return createFile(filename);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco. 
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para truncar um arquivo. Remove do arquivo 
		todos os bytes a partir da posição atual do contador de posição
		(current pointer), inclusive, até o seu final.
-----------------------------------------------------------------------------*/
int truncate2 (FILE2 handle) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Altera o contador de posição (current pointer) do arquivo.
-----------------------------------------------------------------------------*/
int seek2 (FILE2 handle, DWORD offset) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo diretório.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return createDir(pathname);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um diretório do disco.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para alterar o CP (current path)
-----------------------------------------------------------------------------*/
int chdir2 (char *pathname) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return changeDirectory(pathname);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para obter o caminho do diretório corrente.
-----------------------------------------------------------------------------*/
int getcwd2 (char *pathname, int size) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	strcpy(pathname, getCWD());

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return openDirectory(pathname);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return readDir(handle, dentry);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink) com
		o nome dado por linkname (relativo ou absoluto) para um 
		arquivo ou diretório fornecido por filename.
-----------------------------------------------------------------------------*/
int ln2 (char *linkname, char *filename) {
	// Inicializa o FS caso ainda não tenha sido inicializado
	if(hasInit < 0)
		if(initFS() != 0) return -1;

	return -1;
}


