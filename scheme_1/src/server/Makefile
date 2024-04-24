CC ?= gcc
CFLAGS ?= -Wall -Wextra -g
MAKE_CD_OPTIONS ?= 
MAIN_DIR ?= 
INC_DIR ?= 
OBJ_DIR ?= 
BIN_DIR ?= 

SERVER_SRC_DIR := $(MAIN_DIR)/src/server
SERVER_INC_DIR := $(INC_DIR)/server
SERVER_OBJ_DIR := $(OBJ_DIR)/server
SERVER_BIN_DIR := $(BIN_DIR)

TARGET := $(SERVER_BIN_DIR)/server

.PHONY: all clean

$(SERVER_OBJ_DIR): 
	mkdir -p $(SERVER_OBJ_DIR)

all: $(TARGET)

$(SERVER_OBJ_DIR)/lists.o:  $(SERVER_SRC_DIR)/lists.c $(SERVER_INC_DIR)/lists.h | $(SERVER_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/servthreads.o:  $(SERVER_SRC_DIR)/servthreads.c \
	$(SERVER_INC_DIR)/servthreads.h \
	$(SERVER_INC_DIR)/servpar.h \
	$(SERVER_INC_DIR)/lists.h | $(SERVER_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVER_OBJ_DIR)/server.o:  $(SERVER_SRC_DIR)/server.c \
	$(SERVER_INC_DIR)/servthreads.h \
	$(SERVER_INC_DIR)/servpar.h \
	$(SERVER_INC_DIR)/lists.h | $(SERVER_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): \
	$(SERVER_OBJ_DIR)/lists.o \
	$(SERVER_OBJ_DIR)/servthreads.o \
	$(SERVER_OBJ_DIR)/server.o | $(SERVER_OBJ_DIR)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

clean:
	rm -f $(SERVER_OBJ_DIR)/lists.o
	rm -f $(SERVER_OBJ_DIR)/servthreads.o
	rm -f $(SERVER_OBJ_DIR)/server.o
	rm -f $(SERVER_BIN_DIR)/server