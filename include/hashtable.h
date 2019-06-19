#ifndef __HASHTABLE__
#define __HASHTABLE__

#include "../include/t2fs.h"
#include "../include/fs_aux.h"

unsigned int hash(const char *str, int tablesize);

DIRENT2* findHashEntry(DIRENT2 *table[], const char *key);

int insertHashEntry(DIRENT2 *table[], DIRENT2 *file);

#endif
