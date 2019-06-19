CC=gcc
LIB_DIR=../lib
SRC_DIR=../src

all: t2shell

t2shell: t2shell.c $(LIB_DIR)/libt2fs.a
	$(CC) t2shell.c -o t2shell -L$(LIB_DIR) -lt2fs -Wall

clean:
	rm -rf t2shell

