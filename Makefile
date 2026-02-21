CC = gcc
PKG_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf libzip sqlite3 libcurl)
PKG_LIBS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf libzip sqlite3 libcurl)

CFLAGS = -Wall -g -Iinclude -Ivendor/cJSON $(PKG_CFLAGS) -pthread
LIBS = $(PKG_LIBS) -pthread

SRC_DIR = src
VENDOR_DIR = vendor/cJSON
OBJ_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
VENDOR_SRCS = $(VENDOR_DIR)/cJSON.c
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
VENDOR_OBJS = $(OBJ_DIR)/cJSON.o

TARGET = manga_reader

all: create_dirs $(TARGET)

$(TARGET): $(OBJS) $(VENDOR_OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(VENDOR_OBJS) -o $(TARGET) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/cJSON.o: $(VENDOR_DIR)/cJSON.c
	$(CC) $(CFLAGS) -c $< -o $@

create_dirs:
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean create_dirs
