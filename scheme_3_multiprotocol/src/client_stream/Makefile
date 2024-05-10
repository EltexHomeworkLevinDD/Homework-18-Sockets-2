CC ?= gcc
CFLAGS ?= -Wall -Wextra -g
MAKE_CD_OPTIONS ?= 
MAIN_DIR ?= 
INC_DIR ?= 
OBJ_DIR ?= 
BIN_DIR ?= 

CLIENT_SRC_DIR := $(MAIN_DIR)/src/client_stream
CLIENT_INC_DIR := $(INC_DIR)/client
CLIENT_OBJ_DIR := $(OBJ_DIR)/client
CLIENT_BIN_DIR := $(BIN_DIR)

TARGET := $(CLIENT_BIN_DIR)/client_stream

.PHONY: all clean

all: $(TARGET)

$(CLIENT_OBJ_DIR): 
	mkdir -p $(CLIENT_OBJ_DIR)

$(CLIENT_OBJ_DIR)/client_stream.o:  $(CLIENT_SRC_DIR)/client_stream.c $(INC_DIR)/server/servpar.h | $(CLIENT_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(CLIENT_OBJ_DIR)/client_stream.o | $(CLIENT_OBJ_DIR)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

clean:
	rm -f $(CLIENT_OBJ_DIR)/client_stream.o
	rm -f $(TARGET)