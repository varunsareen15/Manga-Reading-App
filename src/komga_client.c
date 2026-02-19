#include "komga_client.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- HTTP Buffer helpers ---

static void httpbuf_init(HttpBuffer *buf) {
  buf->capacity = 4096;
  buf->size = 0;
  buf->data = malloc(buf->capacity);
}

static void httpbuf_free(HttpBuffer *buf) {
  free(buf->data);
  buf->data = NULL;
  buf->size = 0;
  buf->capacity = 0;
}

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t total = size * nmemb;
  HttpBuffer *buf = (HttpBuffer *)userp;

  while (buf->size + total >= buf->capacity) {
    buf->capacity *= 2;
    buf->data = realloc(buf->data, buf->capacity);
    if (!buf->data)
      return 0;
  }

  memcpy(buf->data + buf->size, contents, total);
  buf->size += total;
  return total;
}

// --- Internal helpers ---

static void setup_auth(KomgaClient *client) {
  if (client->api_key[0]) {
    struct curl_slist *headers = NULL;
    char header[300];
    snprintf(header, sizeof(header), "X-API-Key: %s", client->api_key);
    headers = curl_slist_append(headers, header);
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);
  } else if (client->username[0]) {
    char userpwd[300];
    snprintf(userpwd, sizeof(userpwd), "%s:%s", client->username,
             client->password);
    curl_easy_setopt(client->curl, CURLOPT_USERPWD, userpwd);
  }
}

// Perform a GET request. Returns 0 on success with data in buf.
// Caller must call httpbuf_free(buf) when done.
static int do_get(KomgaClient *client, const char *url, HttpBuffer *buf) {
  httpbuf_init(buf);

  curl_easy_reset(client->curl);
  curl_easy_setopt(client->curl, CURLOPT_URL, url);
  curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, buf);
  curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(client->curl, CURLOPT_CONNECTTIMEOUT, 10L);
  setup_auth(client);

  CURLcode res = curl_easy_perform(client->curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "GET %s failed: %s\n", url, curl_easy_strerror(res));
    httpbuf_free(buf);
    return -1;
  }

  long http_code = 0;
  curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code < 200 || http_code >= 300) {
    fprintf(stderr, "GET %s returned HTTP %ld\n", url, http_code);
    httpbuf_free(buf);
    return -1;
  }

  return 0;
}

// Perform a GET that returns binary data. Returns malloc'd buffer.
static char *do_get_binary(KomgaClient *client, const char *url,
                           size_t *out_size) {
  HttpBuffer buf;
  if (do_get(client, url, &buf) != 0)
    return NULL;

  *out_size = buf.size;
  return buf.data; // caller frees
}

// Perform a PATCH with JSON body. Returns 0 on success.
static int do_patch_json(KomgaClient *client, const char *url,
                         const char *json_body) {
  HttpBuffer buf;
  httpbuf_init(&buf);

  curl_easy_reset(client->curl);
  curl_easy_setopt(client->curl, CURLOPT_URL, url);
  curl_easy_setopt(client->curl, CURLOPT_CUSTOMREQUEST, "PATCH");
  curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, json_body);
  curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &buf);
  curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 15L);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  if (client->api_key[0]) {
    char auth_header[300];
    snprintf(auth_header, sizeof(auth_header), "X-API-Key: %s",
             client->api_key);
    headers = curl_slist_append(headers, auth_header);
  }
  curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);

  if (!client->api_key[0] && client->username[0]) {
    char userpwd[300];
    snprintf(userpwd, sizeof(userpwd), "%s:%s", client->username,
             client->password);
    curl_easy_setopt(client->curl, CURLOPT_USERPWD, userpwd);
  }

  CURLcode res = curl_easy_perform(client->curl);
  curl_slist_free_all(headers);
  httpbuf_free(&buf);

  if (res != CURLE_OK) {
    fprintf(stderr, "PATCH %s failed: %s\n", url, curl_easy_strerror(res));
    return -1;
  }

  long http_code = 0;
  curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code < 200 || http_code >= 300) {
    fprintf(stderr, "PATCH %s returned HTTP %ld\n", url, http_code);
    return -1;
  }

  return 0;
}

// Parse a KomgaBook from a cJSON object
static void parse_book(const cJSON *item, KomgaBook *book) {
  memset(book, 0, sizeof(KomgaBook));

  const cJSON *id = cJSON_GetObjectItem(item, "id");
  const cJSON *name = cJSON_GetObjectItem(item, "name");
  const cJSON *series_id = cJSON_GetObjectItem(item, "seriesId");
  const cJSON *number = cJSON_GetObjectItem(item, "number");

  if (id && cJSON_IsString(id))
    strncpy(book->id, id->valuestring, sizeof(book->id) - 1);
  if (name && cJSON_IsString(name))
    strncpy(book->name, name->valuestring, sizeof(book->name) - 1);
  if (series_id && cJSON_IsString(series_id))
    strncpy(book->series_id, series_id->valuestring,
            sizeof(book->series_id) - 1);
  if (number && cJSON_IsNumber(number))
    book->number = number->valueint;

  const cJSON *media = cJSON_GetObjectItem(item, "media");
  if (media) {
    const cJSON *pages_count = cJSON_GetObjectItem(media, "pagesCount");
    if (pages_count && cJSON_IsNumber(pages_count))
      book->pages_count = pages_count->valueint;
  }

  const cJSON *rp = cJSON_GetObjectItem(item, "readProgress");
  if (rp && cJSON_IsObject(rp)) {
    const cJSON *page = cJSON_GetObjectItem(rp, "page");
    const cJSON *completed = cJSON_GetObjectItem(rp, "completed");
    if (page && cJSON_IsNumber(page))
      book->read_progress_page = page->valueint;
    if (completed && cJSON_IsBool(completed))
      book->read_progress_completed = cJSON_IsTrue(completed);
  }
}

// --- Public API ---

int komga_init(KomgaClient *client, const char *base_url, const char *api_key,
               const char *username, const char *password) {
  memset(client, 0, sizeof(KomgaClient));

  // Strip trailing slash from base_url
  strncpy(client->base_url, base_url, sizeof(client->base_url) - 1);
  int len = strlen(client->base_url);
  if (len > 0 && client->base_url[len - 1] == '/')
    client->base_url[len - 1] = '\0';

  if (api_key)
    strncpy(client->api_key, api_key, sizeof(client->api_key) - 1);
  if (username)
    strncpy(client->username, username, sizeof(client->username) - 1);
  if (password)
    strncpy(client->password, password, sizeof(client->password) - 1);

  client->curl = curl_easy_init();
  if (!client->curl) {
    fprintf(stderr, "Failed to initialize libcurl\n");
    return -1;
  }

  return 0;
}

void komga_cleanup(KomgaClient *client) {
  if (client->curl) {
    curl_easy_cleanup(client->curl);
    client->curl = NULL;
  }
}

int komga_test_connection(KomgaClient *client) {
  char url[600];
  snprintf(url, sizeof(url), "%s/api/v1/libraries", client->base_url);

  HttpBuffer buf;
  int rc = do_get(client, url, &buf);
  if (rc == 0) {
    httpbuf_free(&buf);
    client->connected = 1;
  }
  return rc;
}

int komga_get_libraries(KomgaClient *client, KomgaLibrary **out, int *count) {
  char url[600];
  snprintf(url, sizeof(url), "%s/api/v1/libraries", client->base_url);

  HttpBuffer buf;
  if (do_get(client, url, &buf) != 0)
    return -1;

  // Null-terminate the JSON
  buf.data = realloc(buf.data, buf.size + 1);
  buf.data[buf.size] = '\0';

  cJSON *root = cJSON_Parse(buf.data);
  httpbuf_free(&buf);

  if (!root) {
    fprintf(stderr, "Failed to parse libraries JSON\n");
    return -1;
  }

  // Libraries endpoint returns an array directly
  cJSON *arr = root;
  if (!cJSON_IsArray(arr)) {
    // Some versions may wrap in content
    arr = cJSON_GetObjectItem(root, "content");
    if (!arr || !cJSON_IsArray(arr)) {
      cJSON_Delete(root);
      return -1;
    }
  }

  int n = cJSON_GetArraySize(arr);
  *count = n;
  *out = calloc(n, sizeof(KomgaLibrary));

  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(arr, i);
    const cJSON *id = cJSON_GetObjectItem(item, "id");
    const cJSON *name = cJSON_GetObjectItem(item, "name");

    if (id && cJSON_IsString(id))
      strncpy((*out)[i].id, id->valuestring, sizeof((*out)[i].id) - 1);
    if (name && cJSON_IsString(name))
      strncpy((*out)[i].name, name->valuestring, sizeof((*out)[i].name) - 1);
  }

  cJSON_Delete(root);
  return 0;
}

void komga_free_libraries(KomgaLibrary *libs) { free(libs); }

int komga_get_series(KomgaClient *client, const char *library_id, int page,
                     int page_size, KomgaSeries **out, int *count,
                     int *total_pages) {
  char url[800];
  snprintf(url, sizeof(url),
           "%s/api/v1/series?library_id=%s&page=%d&size=%d&sort=metadata.titleSort,asc",
           client->base_url, library_id, page, page_size);

  HttpBuffer buf;
  if (do_get(client, url, &buf) != 0)
    return -1;

  buf.data = realloc(buf.data, buf.size + 1);
  buf.data[buf.size] = '\0';

  cJSON *root = cJSON_Parse(buf.data);
  httpbuf_free(&buf);

  if (!root) {
    fprintf(stderr, "Failed to parse series JSON\n");
    return -1;
  }

  cJSON *content = cJSON_GetObjectItem(root, "content");
  if (!content || !cJSON_IsArray(content)) {
    cJSON_Delete(root);
    return -1;
  }

  int n = cJSON_GetArraySize(content);
  *count = n;
  *out = calloc(n, sizeof(KomgaSeries));

  const cJSON *tp = cJSON_GetObjectItem(root, "totalPages");
  *total_pages = (tp && cJSON_IsNumber(tp)) ? tp->valueint : 1;

  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(content, i);
    const cJSON *id = cJSON_GetObjectItem(item, "id");
    const cJSON *name = cJSON_GetObjectItem(item, "name");
    const cJSON *lib_id = cJSON_GetObjectItem(item, "libraryId");
    const cJSON *bc = cJSON_GetObjectItem(item, "booksCount");

    if (id && cJSON_IsString(id))
      strncpy((*out)[i].id, id->valuestring, sizeof((*out)[i].id) - 1);
    if (name && cJSON_IsString(name))
      strncpy((*out)[i].name, name->valuestring, sizeof((*out)[i].name) - 1);
    if (lib_id && cJSON_IsString(lib_id))
      strncpy((*out)[i].library_id, lib_id->valuestring,
              sizeof((*out)[i].library_id) - 1);
    if (bc && cJSON_IsNumber(bc))
      (*out)[i].books_count = bc->valueint;
  }

  cJSON_Delete(root);
  return 0;
}

void komga_free_series(KomgaSeries *series) { free(series); }

int komga_get_books(KomgaClient *client, const char *series_id, int page,
                    int page_size, KomgaBook **out, int *count,
                    int *total_pages) {
  char url[800];
  snprintf(url, sizeof(url),
           "%s/api/v1/series/%s/books?page=%d&size=%d&sort=metadata.numberSort,asc",
           client->base_url, series_id, page, page_size);

  HttpBuffer buf;
  if (do_get(client, url, &buf) != 0)
    return -1;

  buf.data = realloc(buf.data, buf.size + 1);
  buf.data[buf.size] = '\0';

  cJSON *root = cJSON_Parse(buf.data);
  httpbuf_free(&buf);

  if (!root) {
    fprintf(stderr, "Failed to parse books JSON\n");
    return -1;
  }

  cJSON *content = cJSON_GetObjectItem(root, "content");
  if (!content || !cJSON_IsArray(content)) {
    cJSON_Delete(root);
    return -1;
  }

  int n = cJSON_GetArraySize(content);
  *count = n;
  *out = calloc(n, sizeof(KomgaBook));

  const cJSON *tp = cJSON_GetObjectItem(root, "totalPages");
  *total_pages = (tp && cJSON_IsNumber(tp)) ? tp->valueint : 1;

  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(content, i);
    parse_book(item, &(*out)[i]);
  }

  cJSON_Delete(root);
  return 0;
}

void komga_free_books(KomgaBook *books) { free(books); }

char *komga_get_series_thumbnail(KomgaClient *client, const char *series_id,
                                 size_t *out_size) {
  char url[700];
  snprintf(url, sizeof(url), "%s/api/v1/series/%s/thumbnail", client->base_url,
           series_id);
  return do_get_binary(client, url, out_size);
}

char *komga_get_book_thumbnail(KomgaClient *client, const char *book_id,
                               size_t *out_size) {
  char url[700];
  snprintf(url, sizeof(url), "%s/api/v1/books/%s/thumbnail", client->base_url,
           book_id);
  return do_get_binary(client, url, out_size);
}

char *komga_get_page(KomgaClient *client, const char *book_id, int page_num,
                     size_t *out_size) {
  char url[700];
  snprintf(url, sizeof(url), "%s/api/v1/books/%s/pages/%d", client->base_url,
           book_id, page_num);
  return do_get_binary(client, url, out_size);
}

int komga_download_book(KomgaClient *client, const char *book_id,
                        const char *save_path) {
  char url[700];
  snprintf(url, sizeof(url), "%s/api/v1/books/%s/file", client->base_url,
           book_id);

  FILE *fp = fopen(save_path, "wb");
  if (!fp) {
    fprintf(stderr, "Cannot open %s for writing\n", save_path);
    return -1;
  }

  curl_easy_reset(client->curl);
  curl_easy_setopt(client->curl, CURLOPT_URL, url);
  curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, fp);
  curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 600L);
  curl_easy_setopt(client->curl, CURLOPT_CONNECTTIMEOUT, 10L);
  setup_auth(client);

  CURLcode res = curl_easy_perform(client->curl);
  fclose(fp);

  if (res != CURLE_OK) {
    fprintf(stderr, "Download failed: %s\n", curl_easy_strerror(res));
    remove(save_path);
    return -1;
  }

  long http_code = 0;
  curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code < 200 || http_code >= 300) {
    fprintf(stderr, "Download returned HTTP %ld\n", http_code);
    remove(save_path);
    return -1;
  }

  return 0;
}

int komga_get_next_book(KomgaClient *client, const char *book_id,
                        KomgaBook *out) {
  char url[700];
  snprintf(url, sizeof(url), "%s/api/v1/books/%s/next", client->base_url,
           book_id);

  HttpBuffer buf;
  if (do_get(client, url, &buf) != 0)
    return -1;

  buf.data = realloc(buf.data, buf.size + 1);
  buf.data[buf.size] = '\0';

  cJSON *root = cJSON_Parse(buf.data);
  httpbuf_free(&buf);

  if (!root)
    return -1;

  parse_book(root, out);
  cJSON_Delete(root);
  return 0;
}

int komga_get_prev_book(KomgaClient *client, const char *book_id,
                        KomgaBook *out) {
  char url[700];
  snprintf(url, sizeof(url), "%s/api/v1/books/%s/previous", client->base_url,
           book_id);

  HttpBuffer buf;
  if (do_get(client, url, &buf) != 0)
    return -1;

  buf.data = realloc(buf.data, buf.size + 1);
  buf.data[buf.size] = '\0';

  cJSON *root = cJSON_Parse(buf.data);
  httpbuf_free(&buf);

  if (!root)
    return -1;

  parse_book(root, out);
  cJSON_Delete(root);
  return 0;
}

int komga_update_read_progress(KomgaClient *client, const char *book_id,
                               int page, int completed) {
  char url[700];
  snprintf(url, sizeof(url), "%s/api/v1/books/%s/read-progress",
           client->base_url, book_id);

  char body[128];
  snprintf(body, sizeof(body), "{\"page\":%d,\"completed\":%s}", page,
           completed ? "true" : "false");

  return do_patch_json(client, url, body);
}

int komga_get_book_details(KomgaClient *client, const char *book_id,
                           KomgaBook *out) {
  char url[700];
  snprintf(url, sizeof(url), "%s/api/v1/books/%s", client->base_url, book_id);

  HttpBuffer buf;
  if (do_get(client, url, &buf) != 0)
    return -1;

  buf.data = realloc(buf.data, buf.size + 1);
  buf.data[buf.size] = '\0';

  cJSON *root = cJSON_Parse(buf.data);
  httpbuf_free(&buf);

  if (!root)
    return -1;

  parse_book(root, out);
  cJSON_Delete(root);
  return 0;
}
