#ifndef CBZ_HANDLER_H
#define CBZ_HANDLER_H

#include <stddef.h> // for size_t
#include <zip.h>

typedef struct {
  char **filenames;  // Array of page filenames
  int count;         // Total pages
  int current_index; // Current page number (0-based)
  zip_t *archive;    // Pointer to the open zip
} MangaBook;

// Initialize the book structure and open the zip
// Returns 0 on success, error code otherwise
int open_cbz(const char *path, MangaBook *book);

// Free memory and close zip
void close_cbz(MangaBook *book);

// Extract a file into a raw memory buffer.
// Returns buffer pointer, sets 'size' output variable.
// Returns NULL on failure.
char *get_image_data(MangaBook *book, size_t *size);

// Navigation helpers
void next_page(MangaBook *book);
void prev_page(MangaBook *book);

#endif
