CC=gcc
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src

all: fs_aux t2fs
	mkdir -p bin
	ar crs $(LIB_DIR)/libt2fs.a $(BIN_DIR)/t2fs.o $(BIN_DIR)/fs_aux.o $(LIB_DIR)/apidisk.o

fs_aux:
	$(CC) -c $(SRC_DIR)/fs_aux.c -o $(BIN_DIR)/fs_aux.o -Wall

t2fs:
	$(CC) -c $(SRC_DIR)/t2fs.c -o $(BIN_DIR)/t2fs.o -Wall

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~


