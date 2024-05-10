CC := gcc
CFLAGS := -Wall -Wextra -g

MAIN_DIR := $(CURDIR)
INC_DIR := $(CURDIR)/include
SRC_DIR := $(CURDIR)/src
OBJ_DIR := $(CURDIR)/obj
BIN_DIR := $(CURDIR)/bin

.PHONY: all server client clean

$(OBJ_DIR):

subdirs:
	mkdir -p $(OBJ_DIR)/server
	mkdir -p $(OBJ_DIR)/client
	mkdir -p $(BIN_DIR)

all: server client_stream client_dgram

server: | subdirs
	$(MAKE) --no-print-directory -C $(SRC_DIR)/server CC=$(CC) \
	CFLAGS="$(CFLAGS)" MAIN_DIR=$(MAIN_DIR) INC_DIR=$(INC_DIR) OBJ_DIR=$(OBJ_DIR) BIN_DIR=$(BIN_DIR) all

clean_server:
	$(MAKE) --no-print-directory -C $(SRC_DIR)/server CC=$(CC) \
	CFLAGS="$(CFLAGS)" MAIN_DIR=$(MAIN_DIR) INC_DIR=$(INC_DIR) OBJ_DIR=$(OBJ_DIR) BIN_DIR=$(BIN_DIR) clean

client_stream: | subdirs
	$(MAKE) --no-print-directory -C $(SRC_DIR)/client_stream CC=$(CC) \
	CFLAGS="$(CFLAGS)" MAIN_DIR=$(MAIN_DIR) INC_DIR=$(INC_DIR) OBJ_DIR=$(OBJ_DIR) BIN_DIR=$(BIN_DIR) all

client_stream_server:
	$(MAKE) --no-print-directory -C $(SRC_DIR)/client_stream CC=$(CC) \
	CFLAGS="$(CFLAGS)" MAIN_DIR=$(MAIN_DIR) INC_DIR=$(INC_DIR) OBJ_DIR=$(OBJ_DIR) BIN_DIR=$(BIN_DIR) clean

client_dgram: | subdirs
	$(MAKE) --no-print-directory -C $(SRC_DIR)/client_dgram CC=$(CC) \
	CFLAGS="$(CFLAGS)" MAIN_DIR=$(MAIN_DIR) INC_DIR=$(INC_DIR) OBJ_DIR=$(OBJ_DIR) BIN_DIR=$(BIN_DIR) all

client_dgram_server:
	$(MAKE) --no-print-directory -C $(SRC_DIR)/client_dgram CC=$(CC) \
	CFLAGS="$(CFLAGS)" MAIN_DIR=$(MAIN_DIR) INC_DIR=$(INC_DIR) OBJ_DIR=$(OBJ_DIR) BIN_DIR=$(BIN_DIR) clean

clean: clean_server client_stream client_dgram