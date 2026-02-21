#include "page_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Cache helpers (caller must hold cache_mutex for Komga sources) ---

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

// --- Prefetch thread ---

static void *prefetch_thread_func(void *arg) {
  PageProvider *p = (PageProvider *)arg;

  pthread_mutex_lock(&p->cache_mutex);
  while (p->prefetch_running) {
    int base = p->current_index;

    // Find the next uncached page ahead of current
    int target = -1;
    for (int i = 1; i <= PREFETCH_AHEAD; i++) {
      int idx = base + i;
      if (idx >= p->count)
        break;
      if (!cache_find(p, idx)) {
        target = idx;
        break;
      }
    }

    if (target < 0) {
      // Everything ahead is cached, wait for signal
      pthread_cond_wait(&p->prefetch_cond, &p->cache_mutex);
      continue;
    }

    // Release lock during network fetch
    pthread_mutex_unlock(&p->cache_mutex);

    size_t size = 0;
    char *data =
        komga_get_page(&p->prefetch_client, p->book_id, target + 1, &size);

    pthread_mutex_lock(&p->cache_mutex);

    // Store only if still relevant (user hasn't jumped far away)
    if (data && size > 0) {
      if (abs(target - p->current_index) <= PREFETCH_AHEAD + 2 &&
          !cache_find(p, target)) {
        cache_store(p, target, data, size);
      }
    }
    if (data)
      free(data);
  }
  pthread_mutex_unlock(&p->cache_mutex);
  return NULL;
}

// --- Public API ---

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

  // Initialize synchronization primitives
  pthread_mutex_init(&p->cache_mutex, NULL);
  pthread_cond_init(&p->prefetch_cond, NULL);
  p->prefetch_running = 0;

  // Create a separate KomgaClient for the prefetch thread (own curl handle)
  if (komga_init(&p->prefetch_client, client->base_url, client->api_key,
                 client->username, client->password) == 0) {
    p->prefetch_running = 1;
    pthread_create(&p->prefetch_thread, NULL, prefetch_thread_func, p);
  } else {
    fprintf(stderr, "Warning: prefetch thread disabled (curl init failed)\n");
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

  // SOURCE_KOMGA_STREAM — check cache first (under lock)
  pthread_mutex_lock(&p->cache_mutex);
  CachedPage *cached = cache_find(p, index);
  if (cached) {
    char *copy = malloc(cached->size);
    if (copy) {
      memcpy(copy, cached->data, cached->size);
      *out_size = cached->size;
    }
    pthread_mutex_unlock(&p->cache_mutex);
    return copy;
  }
  pthread_mutex_unlock(&p->cache_mutex);

  // Cache miss — fetch from Komga (blocking, uses main thread's client)
  char *data = komga_get_page(p->client, p->book_id, index + 1, out_size);
  if (data && *out_size > 0) {
    pthread_mutex_lock(&p->cache_mutex);
    if (!cache_find(p, index))
      cache_store(p, index, data, *out_size);
    pthread_mutex_unlock(&p->cache_mutex);
  }

  return data;
}

void provider_notify_prefetch(PageProvider *p) {
  if (p->type != SOURCE_KOMGA_STREAM || !p->prefetch_running)
    return;
  pthread_cond_signal(&p->prefetch_cond);
}

void provider_close(PageProvider *p) {
  // Stop prefetch thread if running
  if (p->type == SOURCE_KOMGA_STREAM) {
    if (p->prefetch_running) {
      pthread_mutex_lock(&p->cache_mutex);
      p->prefetch_running = 0;
      pthread_cond_signal(&p->prefetch_cond);
      pthread_mutex_unlock(&p->cache_mutex);
      pthread_join(p->prefetch_thread, NULL);
      komga_cleanup(&p->prefetch_client);
    }
    pthread_mutex_destroy(&p->cache_mutex);
    pthread_cond_destroy(&p->prefetch_cond);
  }

  cache_clear(p);

  if (p->type == SOURCE_LOCAL_CBZ)
    close_cbz(&p->local_book);

  memset(p, 0, sizeof(PageProvider));
}
