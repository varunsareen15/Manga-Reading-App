#ifndef CBZ_HANDLER_H
#define CBZ_HANDLER_H

#include <stddef.h>
#include <zip.h>

// Reading Modes
typedef enum {
  MODE_MANGA,  // Right-to-Left (Default)
  MODE_COMIC,  // Left-to-Right
  MODE_MANHUA, // Left-to-Right
  MODE_MANHWA, // Vertical (Placeholder for future)
} ReadMode;

typedef struct {
  char **filenames;
  int count;
  int current_index;
  zip_t *archive;
  ReadMode mode;
} MangaBook;

int open_cbz(const char *path, MangaBook *book);
void close_cbz(MangaBook *book);
char *get_image_data(MangaBook *book, size_t *size);
void next_page(MangaBook *book);
void prev_page(MangaBook *book);

#endif
