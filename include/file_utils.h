#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stddef.h>

// Fills out_path with the next/prev .cbz file in the same directory.
// direction: +1 for Next, -1 for Previous.
// Returns 1 if found, 0 if not found.
int get_neighbor_file(const char *current_full_path, int direction, char *out_path, size_t max_len);

#endif
