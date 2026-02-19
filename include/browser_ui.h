#ifndef BROWSER_UI_H
#define BROWSER_UI_H

#include "komga_client.h"
#include "render_engine.h"
#include <SDL2/SDL.h>

typedef enum {
  BROWSER_SERIES, // Grid of series covers
  BROWSER_BOOKS,  // Grid of volumes in a series
} BrowserView;

typedef struct {
  SDL_Texture *texture;
  int width;
  int height;
} CoverImage;

typedef enum {
  BROWSER_ACTION_NONE,
  BROWSER_ACTION_OPEN_BOOK,
  BROWSER_ACTION_DOWNLOAD_BOOK,
  BROWSER_ACTION_QUIT,
} BrowserAction;

typedef struct {
  BrowserAction action;
  char book_id[64];
  char series_name[256];
  ReadMode suggested_mode;
} BrowserResult;

typedef struct {
  // Navigation state
  BrowserView current_view;

  // Library data
  KomgaLibrary *libraries;
  int library_count;
  int selected_library;

  // Series data (current page)
  KomgaSeries *series_list;
  int series_count;
  int series_total_pages;
  int series_current_page;
  int selected_series;
  CoverImage *series_covers;

  // Books data (current page)
  KomgaBook *books_list;
  int books_count;
  int books_total_pages;
  int books_current_page;
  int selected_book;
  CoverImage *book_covers;

  // Grid layout
  int grid_cols;
  int grid_scroll_y;

  // Breadcrumb
  char selected_series_name[256];

  // Loading state
  int is_loading;

  // Download state
  int is_downloading;
  double download_progress;
  char download_status[256];
} BrowserState;

void browser_init(BrowserState *state);
void browser_cleanup(BrowserState *state, SDL_Renderer *renderer);

int browser_load_libraries(BrowserState *state, KomgaClient *client);
int browser_load_series(BrowserState *state, KomgaClient *client,
                        SDL_Renderer *renderer, int page);
int browser_load_books(BrowserState *state, KomgaClient *client,
                       SDL_Renderer *renderer, int page);

void browser_render(BrowserState *state, AppContext *app);
BrowserResult browser_handle_event(BrowserState *state, SDL_Event *event,
                                   KomgaClient *client, AppContext *app);

#endif
