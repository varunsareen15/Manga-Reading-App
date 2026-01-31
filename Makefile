CC = gcc
# 1. Run pkg-config to get the specific include paths (CFLAGS)
#    This will grab -I/opt/homebrew/... etc.
PKG_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_image libzip)

# 2. Run pkg-config to get the specific library paths (LIBS)
PKG_LIBS = $(shell pkg-config --libs sdl2 SDL2_image libzip)

# Combine our local include folder with the pkg-config flags
CFLAGS = -Wall -g -Iinclude $(PKG_CFLAGS)
LIBS = $(PKG_LIBS)

# Define paths
SRC_DIR = src
OBJ_DIR = build

# Automatically find all .c files in src/
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Convert .c filenames to .o filenames for the build folder
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Output binary name
TARGET = manga_reader

# Default rule
all: create_dirs $(TARGET)

# Link the object files into the binary
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

# Compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create the build directory if it doesn't exist
create_dirs:
	mkdir -p $(OBJ_DIR)

# Clean up build artifacts
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean create_dirs
