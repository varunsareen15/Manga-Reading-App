#include "bookmark_manager.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DB_FILE "library.db"
#define MAX_PATH 2048

static sqlite3 *db = NULL;

// Helper: Normalize path to be relative to "library/"
void get_unique_key(const char *path, char *out_buffer, size_t size) {
  char clean_path[MAX_PATH];
  strncpy(clean_path, path, MAX_PATH);

  // Normalize slashes
  for (int i = 0; clean_path[i]; i++) {
    if (clean_path[i] == '\\')
      clean_path[i] = '/';
  }

  const char *marker = "/library/";
  char *found = strstr(clean_path, marker);

  if (found) {
    strncpy(out_buffer, found + strlen(marker), size);
  } else {
    strncpy(out_buffer, clean_path, size);
  }
}

int init_bookmarks_db() {
  int rc = sqlite3_open(DB_FILE, &db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  // Create table if not exists
  const char *sql = "CREATE TABLE IF NOT EXISTS bookmarks ("
                    "path TEXT PRIMARY KEY, "
                    "page INTEGER, "
                    "last_read TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                    ");";

  char *err_msg = 0;
  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return -1;
  }

  // Komga progress table
  const char *sql2 = "CREATE TABLE IF NOT EXISTS komga_progress ("
                     "book_id TEXT PRIMARY KEY, "
                     "page INTEGER, "
                     "completed INTEGER DEFAULT 0, "
                     "last_synced TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                     ");";
  rc = sqlite3_exec(db, sql2, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return -1;
  }

  return 0;
}

void close_bookmarks_db() {
  if (db)
    sqlite3_close(db);
}

int load_bookmark(const char *filepath) {
  if (!db)
    return 0;

  char key[MAX_PATH];
  get_unique_key(filepath, key, sizeof(key));

  const char *sql = "SELECT page FROM bookmarks WHERE path = ?;";
  sqlite3_stmt *stmt;

  int page = 0;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
      page = sqlite3_column_int(stmt, 0);
    }
  }
  sqlite3_finalize(stmt);
  return page;
}

void save_bookmark(const char *filepath, int page_index) {
  if (!db)
    return;

  char key[MAX_PATH];
  get_unique_key(filepath, key, sizeof(key));

  // INSERT OR REPLACE updates the row if it exists, or creates it if it
  // doesn't.
  const char *sql = "INSERT OR REPLACE INTO bookmarks (path, page, last_read) "
                    "VALUES (?, ?, CURRENT_TIMESTAMP);";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, page_index);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      fprintf(stderr, "Failed to save bookmark: %s\n", sqlite3_errmsg(db));
    } else {
      printf("Saved (DB): [%s] at page %d\n", key, page_index + 1);
    }
  }
  sqlite3_finalize(stmt);
}

void save_komga_progress(const char *book_id, int page, int completed) {
  if (!db)
    return;

  const char *sql =
      "INSERT OR REPLACE INTO komga_progress (book_id, page, completed, "
      "last_synced) VALUES (?, ?, ?, CURRENT_TIMESTAMP);";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, book_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, page);
    sqlite3_bind_int(stmt, 3, completed);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);
}

int load_komga_progress(const char *book_id, int *out_page,
                        int *out_completed) {
  if (!db)
    return -1;

  const char *sql =
      "SELECT page, completed FROM komga_progress WHERE book_id = ?;";
  sqlite3_stmt *stmt;
  int found = -1;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, book_id, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      *out_page = sqlite3_column_int(stmt, 0);
      *out_completed = sqlite3_column_int(stmt, 1);
      found = 0;
    }
  }
  sqlite3_finalize(stmt);
  return found;
}
