#include "file_utils.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

// Helper to check extension
int is_cbz(const char *filename) {
  const char *dot = strrchr(filename, '.');
  return (dot && strcmp(dot, ".cbz") == 0);
}

// Qsort comparator
int compare_strings(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

void get_parent_dir(const char *path, char *parent, char *filename) {
  const char *last_sep = strrchr(path, PATH_SEP);
  if (!last_sep)
    last_sep = strrchr(path, '/'); // Fallback

  if (last_sep) {
    int dir_len = last_sep - path;
    strncpy(parent, path, dir_len);
    parent[dir_len] = '\0';
    strcpy(filename, last_sep + 1);
  } else {
    strcpy(parent, ".");
    strcpy(filename, path);
  }
}

int get_neighbor_file(const char *current_full_path, int direction,
                      char *out_path, size_t max_len) {
  char parent_dir[1024];
  char current_filename[256];
  get_parent_dir(current_full_path, parent_dir, current_filename);

  DIR *d = opendir(parent_dir);
  if (!d)
    return 0;

  struct dirent *dir;
  char **file_list = NULL;
  int count = 0;

  // 1. Collect all CBZ files
  while ((dir = readdir(d)) != NULL) {
    if (is_cbz(dir->d_name)) {
      file_list = realloc(file_list, sizeof(char *) * (count + 1));
      file_list[count] = strdup(dir->d_name);
      count++;
    }
  }
  closedir(d);

  if (count == 0)
    return 0;

  // 2. Sort them alphabetically
  qsort(file_list, count, sizeof(char *), compare_strings);

  // 3. Find current and pick neighbor
  int found_idx = -1;
  for (int i = 0; i < count; i++) {
    if (strcmp(file_list[i], current_filename) == 0) {
      found_idx = i;
      break;
    }
  }

  int success = 0;
  if (found_idx != -1) {
    int target_idx = found_idx + direction;
    if (target_idx >= 0 && target_idx < count) {
      snprintf(out_path, max_len, "%s%c%s", parent_dir, PATH_SEP,
               file_list[target_idx]);
      success = 1;
    }
  }

  // Cleanup
  for (int i = 0; i < count; i++)
    free(file_list[i]);
  free(file_list);

  return success;
}
