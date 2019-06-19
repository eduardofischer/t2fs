CC=gcc
LIB_DIR=../lib
SRC_DIR=../src

all: readMBR formatPart testFAT testDirEnt

readMBR: readMBR.c $(LIB_DIR)/libt2fs.a
	$(CC) readMBR.c -o readMBR -L$(LIB_DIR) -lt2fs -Wall

formatPart: formatPart.c $(LIB_DIR)/libt2fs.a
	$(CC) formatPart.c -o formatPart -L$(LIB_DIR) -lt2fs -Wall

testFAT: testFAT.c $(LIB_DIR)/libt2fs.a
	$(CC) testFAT.c -o testFAT -L$(LIB_DIR) -lt2fs -Wall

testDirEnt: testDirEnt.c $(LIB_DIR)/libt2fs.a
	$(CC) testDirEnt.c -o testDirEnt -L$(LIB_DIR) -lt2fs -Wall

clean:
	rm -rf readMBR formatPart testFAT testDirEnt


