#ifndef BOOKMARK_MANAGER_H
#define BOOKMARK_MANAGER_H

// Checks if bookmark exists for this file
// Returns the page index (0-based) or 0 if not found
int load_bookmark(const char *filepath);

// Saves the current page index for the given file
void save_bookmark(const char *filepath, int page_index);

#endif
