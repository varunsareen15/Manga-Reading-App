#ifndef BOOKMARK_MANAGER_H
#define BOOKMARK_MANAGER_H

// Initialize the SQLite database (create tables if needed)
int init_bookmarks_db();

// Checks if a bookmark exists for this file.
int load_bookmark(const char *filepath);

// Saves the current page index for the given file.
void save_bookmark(const char *filepath, int page_index);

// Close the database connection (call at app exit)
void close_bookmarks_db();

#endif
