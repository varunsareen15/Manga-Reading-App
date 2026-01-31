#include "bookmark_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_FILE "bookmarks.txt"
#define TEMP_FILE "bookmarks.tmp"
#define MAX_LINE 1024

// Helper: Extract filename from full path (e.g. "books/manga.cbz" ->
// "manga.cbz") This ensures that if you move the file to a different folder,
// the bookmark still works.
const char *get_filename(const char *path) {
  const char *last_slash = strrchr(path, '/');
#ifdef _WIN32
  const char *last_backslash = strrchr(path, '\\');
  if (last_backslash && (!last_slash || last_backslash > last_slash)) {
    last_slash = last_backslash;
  }
#endif
  return last_slash ? last_slash + 1 : path;
}

int load_bookmark(const char *filepath) {
  FILE *f = fopen(DB_FILE, "r");
  if (!f)
    return 0; // No database yet, start at page 0

  const char *target_name = get_filename(filepath);
  char line[MAX_LINE];
  int saved_page = 0;

  while (fgets(line, sizeof(line), f)) {
    // Format is: filename=page
    char *eq = strchr(line, '=');
    if (eq) {
      *eq = '\0'; // Split string
      char *name = line;
      int page = atoi(eq + 1);

      // Remove newline if present
      name[strcspn(name, "\r\n")] = 0;

      if (strcmp(name, target_name) == 0) {
        saved_page = page;
        break;
      }
    }
  }

  fclose(f);
  return saved_page;
}

void save_bookmark(const char *filepath, int page_index) {
  const char *target_name = get_filename(filepath);

  FILE *in = fopen(DB_FILE, "r");
  FILE *out = fopen(TEMP_FILE, "w");
  if (!out)
    return;

  int found = 0;
  char line[MAX_LINE];

  // Copy existing bookmarks, updating ours if found
  if (in) {
    while (fgets(line, sizeof(line), in)) {
      char line_copy[MAX_LINE];
      strcpy(line_copy, line);

      char *eq = strchr(line_copy, '=');
      if (eq) {
        *eq = '\0';
        char *name = line_copy;
        name[strcspn(name, "\r\n")] = 0;

        if (strcmp(name, target_name) == 0) {
          fprintf(out, "%s=%d\n", target_name, page_index);
          found = 1;
        } else {
          fprintf(out, "%s", line); // Copy old line
        }
      } else {
        fprintf(out, "%s", line); // Copy malformed lines
      }
    }
    fclose(in);
  }

  // If new entry, append it
  if (!found) {
    fprintf(out, "%s=%d\n", target_name, page_index);
  }

  fclose(out);

  // Atomic replace
  remove(DB_FILE);
  rename(TEMP_FILE, DB_FILE);

  printf("Saved progress: %s at page %d\n", target_name, page_index + 1);
}
