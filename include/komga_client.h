#ifndef KOMGA_CLIENT_H
#define KOMGA_CLIENT_H

#include <curl/curl.h>
#include <stddef.h>

typedef struct {
  char base_url[512];
  char api_key[256];
  char username[128];
  char password[128];
  CURL *curl;
  int connected;
} KomgaClient;

typedef struct {
  char id[64];
  char name[256];
} KomgaLibrary;

typedef struct {
  char id[64];
  char name[256];
  char library_id[64];
  int books_count;
} KomgaSeries;

typedef struct {
  char id[64];
  char name[256];
  char series_id[64];
  int number;
  int pages_count;
  int read_progress_page;
  int read_progress_completed;
} KomgaBook;

typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} HttpBuffer;

// Lifecycle
int komga_init(KomgaClient *client, const char *base_url,
               const char *api_key, const char *username,
               const char *password);
void komga_cleanup(KomgaClient *client);
int komga_test_connection(KomgaClient *client);

// Libraries
int komga_get_libraries(KomgaClient *client, KomgaLibrary **out, int *count);
void komga_free_libraries(KomgaLibrary *libs);

// Series (paginated)
int komga_get_series(KomgaClient *client, const char *library_id, int page,
                     int page_size, KomgaSeries **out, int *count,
                     int *total_pages);
void komga_free_series(KomgaSeries *series);

// Books (paginated)
int komga_get_books(KomgaClient *client, const char *series_id, int page,
                    int page_size, KomgaBook **out, int *count,
                    int *total_pages);
void komga_free_books(KomgaBook *books);

// Images — caller must free() returned data
char *komga_get_series_thumbnail(KomgaClient *client, const char *series_id,
                                 size_t *out_size);
char *komga_get_book_thumbnail(KomgaClient *client, const char *book_id,
                               size_t *out_size);
char *komga_get_page(KomgaClient *client, const char *book_id, int page_num,
                     size_t *out_size);

// Download full CBZ to disk. Returns 0 on success.
int komga_download_book(KomgaClient *client, const char *book_id,
                        const char *save_path);

// Book navigation
int komga_get_next_book(KomgaClient *client, const char *book_id,
                        KomgaBook *out);
int komga_get_prev_book(KomgaClient *client, const char *book_id,
                        KomgaBook *out);

// Read progress
int komga_update_read_progress(KomgaClient *client, const char *book_id,
                               int page, int completed);
int komga_get_book_details(KomgaClient *client, const char *book_id,
                           KomgaBook *out);

#endif
