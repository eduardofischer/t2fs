#ifndef __HASHTABLE__
#define __HASHTABLE__

#include "../include/t2fs.h"
#include "../include/fs_aux.h"

unsigned int hash(const char *str, int tablesize);

DIRENTRY* findHashEntry(DIRENTRY *table, const char *key);

int insertHashEntry(DIRENTRY *table, DIRENTRY *file);

int updateHashEntry(DIRENTRY *table, DIRENTRY *file);

int removeHashEntry(DIRENTRY *table, DIRENTRY *file);

DIRENTRY *getNthEntry(DIRENTRY *table, int n);

#endif
