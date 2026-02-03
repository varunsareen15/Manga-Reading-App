#include "bookmark_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_FILE "bookmarks.txt"
#define TEMP_FILE "bookmarks.tmp"
#define MAX_LINE 2048

// Helper: Generate a unique key based on Library structure
// If path contains "library/", it returns everything after it.
// If not, it falls back to the full path.
void get_unique_key(const char *path, char *out_buffer, size_t size) {
  // Normalize path separators (turn backslash into forward slash for Windows)
  char clean_path[MAX_LINE];
  strncpy(clean_path, path, MAX_LINE);

  for (int i = 0; clean_path[i]; i++) {
    if (clean_path[i] == '\\')
      clean_path[i] = '/';
  }

  // Look for "library/" marker
  const char *marker = "/library/";
  char *found = strstr(clean_path, marker);

  if (found) {
    // Return "manga/Series/vol.cbz"
    strncpy(out_buffer, found + strlen(marker), size);
  } else {
    // Fallback: Use the full path if "library" folder isn't found
    strncpy(out_buffer, clean_path, size);
  }
}

int load_bookmark(const char *filepath) {
  FILE *f = fopen(DB_FILE, "r");
  if (!f)
    return 0;

  char target_key[MAX_LINE];
  get_unique_key(filepath, target_key, sizeof(target_key));

  char line[MAX_LINE];
  int saved_page = 0;

  while (fgets(line, sizeof(line), f)) {
    char *eq = strchr(line, '=');
    if (eq) {
      *eq = '\0';
      char *key = line;
      int page = atoi(eq + 1);
      key[strcspn(key, "\r\n")] = 0;

      if (strcmp(key, target_key) == 0) {
        saved_page = page;
        break;
      }
    }
  }

  fclose(f);
  return saved_page;
}

void save_bookmark(const char *filepath, int page_index) {
  char target_key[MAX_LINE];
  get_unique_key(filepath, target_key, sizeof(target_key));

  FILE *in = fopen(DB_FILE, "r");
  FILE *out = fopen(TEMP_FILE, "w");
  if (!out)
    return;

  int found = 0;
  char line[MAX_LINE];

  if (in) {
    while (fgets(line, sizeof(line), in)) {
      char line_copy[MAX_LINE];
      strcpy(line_copy, line);

      char *eq = strchr(line_copy, '=');
      if (eq) {
        *eq = '\0';
        char *key = line_copy;
        key[strcspn(key, "\r\n")] = 0;

        if (strcmp(key, target_key) == 0) {
          fprintf(out, "%s=%d\n", target_key, page_index);
          found = 1;
        } else {
          fprintf(out, "%s", line);
        }
      } else {
        fprintf(out, "%s", line);
      }
    }
    fclose(in);
  }

  if (!found) {
    fprintf(out, "%s=%d\n", target_key, page_index);
  }

  fclose(out);
  remove(DB_FILE);
  rename(TEMP_FILE, DB_FILE);

  printf("Saved: [%s] at page %d\n", target_key, page_index + 1);
}
