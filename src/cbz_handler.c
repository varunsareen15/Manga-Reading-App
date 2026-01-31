#include "cbz_handler.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper: Case-insensitive string comparison for extensions
static int str_ends_with_ignore_case(const char *str, const char *suffix) {
  if (!str || !suffix)
    return 0;
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix > lenstr)
    return 0;

  const char *ptr = str + lenstr - lensuffix;
  while (*ptr && *suffix) {
    if (tolower((unsigned char)*ptr) != tolower((unsigned char)*suffix)) {
      return 0;
    }
    ptr++;
    suffix++;
  }
  return 1;
}

// Helper: Check if file is an image (PNG, JPG, JPEG)
static int is_image_file(const char *filename) {
  return str_ends_with_ignore_case(filename, ".png") ||
         str_ends_with_ignore_case(filename, ".jpg") ||
         str_ends_with_ignore_case(filename, ".jpeg");
}

// Helper: Comparator for qsort
static int compare_strings(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

int open_cbz(const char *path, MangaBook *book) {
  int err = 0;
  book->archive = zip_open(path, 0, &err);
  if (!book->archive)
    return err;

  book->current_index = 0;
  book->count = 0;

  // Count valid images
  int num_entries = zip_get_num_entries(book->archive, 0);
  for (int i = 0; i < num_entries; i++) {
    const char *name = zip_get_name(book->archive, i, 0);
    // Ignore __MACOSX folders and hidden files often found in zips
    if (name[0] != '.' && strstr(name, "__MACOSX") == 0 &&
        is_image_file(name)) {
      book->count++;
    }
  }

  if (book->count == 0)
    return -1;

  // Allocate filename array
  book->filenames = malloc(sizeof(char *) * book->count);
  int head = 0;
  for (int i = 0; i < num_entries; i++) {
    const char *name = zip_get_name(book->archive, i, 0);
    if (name[0] != '.' && strstr(name, "__MACOSX") == 0 &&
        is_image_file(name)) {
      book->filenames[head++] = strdup(name);
    }
  }

  // Sort alphabetically (Essential for manga page order)
  qsort(book->filenames, book->count, sizeof(char *), compare_strings);

  return 0;
}

void close_cbz(MangaBook *book) {
  if (book->archive)
    zip_close(book->archive);
  if (book->filenames) {
    for (int i = 0; i < book->count; i++)
      free(book->filenames[i]);
    free(book->filenames);
  }
}

char *get_image_data(MangaBook *book, size_t *out_size) {
  if (book->current_index < 0 || book->current_index >= book->count)
    return NULL;

  const char *fname = book->filenames[book->current_index];
  struct zip_stat st;
  zip_stat_init(&st);
  zip_stat(book->archive, fname, 0, &st);

  char *contents = malloc(st.size);
  if (!contents)
    return NULL;

  zip_file_t *f = zip_fopen(book->archive, fname, 0);
  if (!f) {
    free(contents);
    return NULL;
  }

  zip_fread(f, contents, st.size);
  zip_fclose(f);

  if (out_size)
    *out_size = st.size;
  return contents;
}

void next_page(MangaBook *book) {
  if (book->current_index < book->count - 1)
    book->current_index++;
}

void prev_page(MangaBook *book) {
  if (book->current_index > 0)
    book->current_index--;
}
