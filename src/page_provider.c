#include "page_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cache_clear(PageProvider *p) {
  for (int i = 0; i < PAGE_CACHE_SIZE; i++) {
    if (p->cache[i].data) {
      free(p->cache[i].data);
      p->cache[i].data = NULL;
    }
    p->cache[i].index = -1;
    p->cache[i].size = 0;
  }
}

static CachedPage *cache_find(PageProvider *p, int index) {
  for (int i = 0; i < PAGE_CACHE_SIZE; i++) {
    if (p->cache[i].index == index && p->cache[i].data)
      return &p->cache[i];
  }
  return NULL;
}

static void cache_store(PageProvider *p, int index, const char *data,
                        size_t size) {
  // Find an empty slot or the slot furthest from current_index
  int best = 0;
  int best_dist = -1;

  for (int i = 0; i < PAGE_CACHE_SIZE; i++) {
    if (!p->cache[i].data) {
      best = i;
      break;
    }
    int dist = abs(p->cache[i].index - p->current_index);
    if (dist > best_dist) {
      best_dist = dist;
      best = i;
    }
  }

  if (p->cache[best].data)
    free(p->cache[best].data);

  p->cache[best].index = index;
  p->cache[best].size = size;
  p->cache[best].data = malloc(size);
  if (p->cache[best].data)
    memcpy(p->cache[best].data, data, size);
}

int provider_open_local(PageProvider *p, const char *cbz_path) {
  memset(p, 0, sizeof(PageProvider));
  p->type = SOURCE_LOCAL_CBZ;

  for (int i = 0; i < PAGE_CACHE_SIZE; i++)
    p->cache[i].index = -1;

  if (open_cbz(cbz_path, &p->local_book) != 0)
    return -1;

  p->count = p->local_book.count;
  p->current_index = p->local_book.current_index;
  p->read_mode = p->local_book.mode;
  return 0;
}

int provider_open_komga(PageProvider *p, KomgaClient *client,
                        const char *book_id, ReadMode mode) {
  memset(p, 0, sizeof(PageProvider));
  p->type = SOURCE_KOMGA_STREAM;
  p->client = client;
  strncpy(p->book_id, book_id, sizeof(p->book_id) - 1);
  p->read_mode = mode;

  for (int i = 0; i < PAGE_CACHE_SIZE; i++)
    p->cache[i].index = -1;

  // Fetch book details to get page count and read progress
  KomgaBook details;
  if (komga_get_book_details(client, book_id, &details) != 0) {
    fprintf(stderr, "Failed to get book details for %s\n", book_id);
    return -1;
  }

  p->count = details.pages_count;
  p->current_index = 0;

  // Resume from Komga read progress if available
  if (details.read_progress_page > 0 &&
      details.read_progress_page < p->count) {
    p->current_index = details.read_progress_page;
  }

  return 0;
}

char *provider_get_page(PageProvider *p, int index, size_t *out_size) {
  if (index < 0 || index >= p->count) {
    *out_size = 0;
    return NULL;
  }

  if (p->type == SOURCE_LOCAL_CBZ) {
    int saved = p->local_book.current_index;
    p->local_book.current_index = index;
    char *data = get_image_data(&p->local_book, out_size);
    p->local_book.current_index = saved;
    return data;
  }

  // SOURCE_KOMGA_STREAM — check cache first
  CachedPage *cached = cache_find(p, index);
  if (cached) {
    // Return a copy so caller can free() it
    char *copy = malloc(cached->size);
    if (copy) {
      memcpy(copy, cached->data, cached->size);
      *out_size = cached->size;
    }
    return copy;
  }

  // Cache miss — fetch from Komga (1-based page numbers)
  char *data = komga_get_page(p->client, p->book_id, index + 1, out_size);
  if (data && *out_size > 0)
    cache_store(p, index, data, *out_size);

  return data;
}

void provider_close(PageProvider *p) {
  cache_clear(p);

  if (p->type == SOURCE_LOCAL_CBZ)
    close_cbz(&p->local_book);

  memset(p, 0, sizeof(PageProvider));
}
