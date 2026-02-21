#ifndef PAGE_PROVIDER_H
#define PAGE_PROVIDER_H

#include "cbz_handler.h"
#include "komga_client.h"
#include <pthread.h>
#include <stddef.h>

typedef enum {
  SOURCE_LOCAL_CBZ,
  SOURCE_KOMGA_STREAM,
} PageSourceType;

#define PAGE_CACHE_SIZE 20
#define PREFETCH_AHEAD 5

typedef struct {
  int index;
  char *data;
  size_t size;
} CachedPage;

typedef struct {
  PageSourceType type;

  // For SOURCE_LOCAL_CBZ:
  MangaBook local_book;

  // For SOURCE_KOMGA_STREAM:
  KomgaClient *client; // borrowed, not owned (main thread only)
  char book_id[64];

  // Unified fields
  int current_index;
  int count;
  ReadMode read_mode;

  // Page cache for streaming
  CachedPage cache[PAGE_CACHE_SIZE];

  // Prefetch thread state (Komga only)
  pthread_t prefetch_thread;
  pthread_mutex_t cache_mutex;
  pthread_cond_t prefetch_cond;
  KomgaClient prefetch_client; // separate curl handle for prefetch thread
  int prefetch_running;
} PageProvider;

// Open from local CBZ file
int provider_open_local(PageProvider *p, const char *cbz_path);

// Open from Komga book (fetches book details for page count)
int provider_open_komga(PageProvider *p, KomgaClient *client,
                        const char *book_id, ReadMode mode);

// Get page image data at index. Caller must free() the returned buffer.
char *provider_get_page(PageProvider *p, int index, size_t *out_size);

// Signal the prefetch thread that current_index changed
void provider_notify_prefetch(PageProvider *p);

// Close and free resources
void provider_close(PageProvider *p);

#endif
