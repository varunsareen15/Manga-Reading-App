CC = gcc
PKG_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf libzip sqlite3)
PKG_LIBS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf libzip sqlite3)

CFLAGS = -Wall -g -Iinclude $(PKG_CFLAGS)
LIBS = $(PKG_LIBS)

SRC_DIR = src
OBJ_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

TARGET = manga_reader

all: create_dirs $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

create_dirs:
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean create_dirs
