#include "browser_ui.h"
#include <SDL2/SDL_image.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Layout constants ---
#define TAB_HEIGHT 50
#define BREADCRUMB_HEIGHT 40
#define FOOTER_HEIGHT 40
#define COVER_WIDTH 180
#define COVER_PADDING 20
#define COVER_LABEL_HEIGHT 30
#define SCROLL_STEP 40

// --- Colors ---
static const SDL_Color COLOR_WHITE = {255, 255, 255, 255};
static const SDL_Color COLOR_GRAY = {150, 150, 150, 255};
static const SDL_Color COLOR_ACCENT = {100, 149, 237, 255};
static const SDL_Color COLOR_BG = {30, 30, 30, 255};
static const SDL_Color COLOR_TAB_BG = {45, 45, 45, 255};

// --- Helpers ---

static ReadMode detect_mode_from_library(const char *name) {
  char lower[256];
  strncpy(lower, name, 255);
  lower[255] = '\0';
  for (int i = 0; lower[i]; i++)
    lower[i] = tolower(lower[i]);

  if (strstr(lower, "manhwa") || strstr(lower, "webtoon"))
    return MODE_MANHWA;
  if (strstr(lower, "manhua"))
    return MODE_MANHUA;
  if (strstr(lower, "comic"))
    return MODE_COMIC;
  return MODE_MANGA;
}

static SDL_Texture *load_texture_from_bytes(SDL_Renderer *renderer, char *data,
                                            size_t size, int *out_w,
                                            int *out_h) {
  if (!data || size == 0)
    return NULL;

  SDL_RWops *rw = SDL_RWFromMem(data, size);
  SDL_Surface *surf = IMG_Load_RW(rw, 1);
  if (!surf)
    return NULL;

  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
  if (out_w)
    *out_w = surf->w;
  if (out_h)
    *out_h = surf->h;
  SDL_FreeSurface(surf);
  return tex;
}

static void free_covers(CoverImage *covers, int count,
                        SDL_Renderer *renderer) {
  if (!covers)
    return;
  for (int i = 0; i < count; i++) {
    if (covers[i].texture)
      SDL_DestroyTexture(covers[i].texture);
  }
  free(covers);
}

static void draw_text(AppContext *app, const char *text, int x, int y,
                      SDL_Color color) {
  if (!app->font || !text || !text[0])
    return;
  int w, h;
  SDL_Texture *tex =
      render_text_texture(app->renderer, app->font, text, color, &w, &h);
  if (tex) {
    SDL_Rect dest = {x, y, w, h};
    SDL_RenderCopy(app->renderer, tex, NULL, &dest);
    SDL_DestroyTexture(tex);
  }
}

static void draw_text_truncated(AppContext *app, const char *text, int x, int y,
                                int max_w, SDL_Color color) {
  if (!app->font || !text || !text[0])
    return;

  char buf[256];
  strncpy(buf, text, 255);
  buf[255] = '\0';

  // Truncate with ellipsis if needed
  int w, h;
  TTF_SizeText(app->font, buf, &w, &h);
  while (w > max_w && strlen(buf) > 4) {
    buf[strlen(buf) - 4] = '.';
    buf[strlen(buf) - 3] = '.';
    buf[strlen(buf) - 2] = '.';
    buf[strlen(buf) - 1] = '\0';
    TTF_SizeText(app->font, buf, &w, &h);
  }

  draw_text(app, buf, x, y, color);
}

// --- Lifecycle ---

void browser_init(BrowserState *state) {
  memset(state, 0, sizeof(BrowserState));
  state->current_view = BROWSER_SERIES;
  state->grid_cols = 4;
}

void browser_cleanup(BrowserState *state, SDL_Renderer *renderer) {
  if (state->series_covers)
    free_covers(state->series_covers, state->series_count, renderer);
  if (state->book_covers)
    free_covers(state->book_covers, state->books_count, renderer);
  if (state->series_list)
    komga_free_series(state->series_list);
  if (state->books_list)
    komga_free_books(state->books_list);
  if (state->libraries)
    komga_free_libraries(state->libraries);
  memset(state, 0, sizeof(BrowserState));
}

// --- Data Loading ---

int browser_load_libraries(BrowserState *state, KomgaClient *client) {
  state->is_loading = 1;
  int rc =
      komga_get_libraries(client, &state->libraries, &state->library_count);
  state->is_loading = 0;

  if (rc != 0 || state->library_count == 0)
    return -1;

  state->selected_library = 0;
  return 0;
}

int browser_load_series(BrowserState *state, KomgaClient *client,
                        SDL_Renderer *renderer, int page) {
  // Free previous series data
  if (state->series_covers) {
    free_covers(state->series_covers, state->series_count, renderer);
    state->series_covers = NULL;
  }
  if (state->series_list) {
    komga_free_series(state->series_list);
    state->series_list = NULL;
  }
  state->series_count = 0;

  state->is_loading = 1;
  int rc = komga_get_series(
      client, state->libraries[state->selected_library].id, page, 20,
      &state->series_list, &state->series_count, &state->series_total_pages);
  state->is_loading = 0;

  if (rc != 0)
    return -1;

  state->series_current_page = page;
  state->selected_series = 0;
  state->grid_scroll_y = 0;

  // Load cover thumbnails
  state->series_covers = calloc(state->series_count, sizeof(CoverImage));
  for (int i = 0; i < state->series_count; i++) {
    size_t size;
    char *data =
        komga_get_series_thumbnail(client, state->series_list[i].id, &size);
    if (data) {
      state->series_covers[i].texture = load_texture_from_bytes(
          renderer, data, size, &state->series_covers[i].width,
          &state->series_covers[i].height);
      free(data);
    }
  }

  return 0;
}

int browser_load_books(BrowserState *state, KomgaClient *client,
                       SDL_Renderer *renderer, int page) {
  // Free previous book data
  if (state->book_covers) {
    free_covers(state->book_covers, state->books_count, renderer);
    state->book_covers = NULL;
  }
  if (state->books_list) {
    komga_free_books(state->books_list);
    state->books_list = NULL;
  }
  state->books_count = 0;

  if (state->selected_series < 0 ||
      state->selected_series >= state->series_count)
    return -1;

  const char *series_id = state->series_list[state->selected_series].id;
  strncpy(state->selected_series_name,
          state->series_list[state->selected_series].name,
          sizeof(state->selected_series_name) - 1);

  state->is_loading = 1;
  int rc = komga_get_books(client, series_id, page, 20, &state->books_list,
                           &state->books_count, &state->books_total_pages);
  state->is_loading = 0;

  if (rc != 0)
    return -1;

  state->books_current_page = page;
  state->selected_book = 0;
  state->grid_scroll_y = 0;

  // Load book cover thumbnails
  state->book_covers = calloc(state->books_count, sizeof(CoverImage));
  for (int i = 0; i < state->books_count; i++) {
    size_t size;
    char *data =
        komga_get_book_thumbnail(client, state->books_list[i].id, &size);
    if (data) {
      state->book_covers[i].texture = load_texture_from_bytes(
          renderer, data, size, &state->book_covers[i].width,
          &state->book_covers[i].height);
      free(data);
    }
  }

  return 0;
}

// --- Rendering ---

static void render_tab_bar(BrowserState *state, AppContext *app, int win_w) {
  // Tab bar background
  SDL_Rect tab_bg = {0, 0, win_w, TAB_HEIGHT};
  SDL_SetRenderDrawColor(app->renderer, COLOR_TAB_BG.r, COLOR_TAB_BG.g,
                         COLOR_TAB_BG.b, 255);
  SDL_RenderFillRect(app->renderer, &tab_bg);

  if (!state->libraries || state->library_count == 0)
    return;

  int tab_w = win_w / state->library_count;
  if (tab_w > 250)
    tab_w = 250;

  for (int i = 0; i < state->library_count; i++) {
    int x = i * tab_w;

    // Selected tab underline
    if (i == state->selected_library) {
      SDL_Rect underline = {x + 5, TAB_HEIGHT - 4, tab_w - 10, 4};
      SDL_SetRenderDrawColor(app->renderer, COLOR_ACCENT.r, COLOR_ACCENT.g,
                             COLOR_ACCENT.b, 255);
      SDL_RenderFillRect(app->renderer, &underline);
    }

    // Tab label
    SDL_Color col =
        (i == state->selected_library) ? COLOR_WHITE : COLOR_GRAY;
    draw_text_truncated(app, state->libraries[i].name, x + 15, 12, tab_w - 30,
                        col);
  }

  // Bottom border
  SDL_SetRenderDrawColor(app->renderer, 60, 60, 60, 255);
  SDL_Rect border = {0, TAB_HEIGHT - 1, win_w, 1};
  SDL_RenderFillRect(app->renderer, &border);
}

static void render_breadcrumb(BrowserState *state, AppContext *app, int win_w) {
  int y = TAB_HEIGHT + 5;
  char crumb[512];

  if (state->current_view == BROWSER_SERIES) {
    snprintf(crumb, sizeof(crumb), "%s",
             state->libraries[state->selected_library].name);
  } else {
    snprintf(crumb, sizeof(crumb), "%s > %s",
             state->libraries[state->selected_library].name,
             state->selected_series_name);
  }

  draw_text_truncated(app, crumb, 15, y, win_w - 30, COLOR_GRAY);
}

static void render_cover_grid(AppContext *app, CoverImage *covers,
                              const char **names, int count, int selected,
                              int grid_cols, int scroll_y, int y_offset,
                              int win_w, int win_h) {
  int cell_w = COVER_WIDTH + COVER_PADDING;
  int cell_h = (int)(COVER_WIDTH * 1.4f) + COVER_LABEL_HEIGHT + COVER_PADDING;

  // Recalculate grid_cols based on window width
  grid_cols = (win_w - COVER_PADDING) / cell_w;
  if (grid_cols < 1)
    grid_cols = 1;

  int start_x = (win_w - grid_cols * cell_w) / 2 + COVER_PADDING / 2;

  for (int i = 0; i < count; i++) {
    int col = i % grid_cols;
    int row = i / grid_cols;

    int x = start_x + col * cell_w;
    int y = y_offset + row * cell_h - scroll_y;

    // Skip if off-screen
    if (y + cell_h < y_offset || y > win_h)
      continue;

    // Selection highlight
    if (i == selected) {
      SDL_Rect sel = {x - 3, y - 3, COVER_WIDTH + 6,
                      (int)(COVER_WIDTH * 1.4f) + 6};
      SDL_SetRenderDrawColor(app->renderer, COLOR_ACCENT.r, COLOR_ACCENT.g,
                             COLOR_ACCENT.b, 255);
      SDL_RenderDrawRect(app->renderer, &sel);
      // Draw a second border for thickness
      SDL_Rect sel2 = {x - 2, y - 2, COVER_WIDTH + 4,
                       (int)(COVER_WIDTH * 1.4f) + 4};
      SDL_RenderDrawRect(app->renderer, &sel2);
    }

    // Cover image or placeholder
    int cover_h = (int)(COVER_WIDTH * 1.4f);
    if (covers && covers[i].texture) {
      // Maintain aspect ratio within the cell
      float ar = (float)covers[i].width / covers[i].height;
      int draw_w = COVER_WIDTH;
      int draw_h = (int)(draw_w / ar);
      if (draw_h > cover_h) {
        draw_h = cover_h;
        draw_w = (int)(draw_h * ar);
      }
      int cx = x + (COVER_WIDTH - draw_w) / 2;
      int cy = y + (cover_h - draw_h) / 2;

      SDL_Rect dest = {cx, cy, draw_w, draw_h};
      SDL_RenderCopy(app->renderer, covers[i].texture, NULL, &dest);
    } else {
      // Gray placeholder
      SDL_Rect ph = {x, y, COVER_WIDTH, cover_h};
      SDL_SetRenderDrawColor(app->renderer, 60, 60, 60, 255);
      SDL_RenderFillRect(app->renderer, &ph);
    }

    // Label
    if (names && names[i])
      draw_text_truncated(app, names[i], x, y + cover_h + 5, COVER_WIDTH,
                          COLOR_WHITE);
  }
}

static void render_footer(BrowserState *state, AppContext *app, int win_w,
                          int win_h) {
  SDL_Rect footer_bg = {0, win_h - FOOTER_HEIGHT, win_w, FOOTER_HEIGHT};
  SDL_SetRenderDrawColor(app->renderer, COLOR_TAB_BG.r, COLOR_TAB_BG.g,
                         COLOR_TAB_BG.b, 255);
  SDL_RenderFillRect(app->renderer, &footer_bg);

  int cur_page, total_pages;
  if (state->current_view == BROWSER_SERIES) {
    cur_page = state->series_current_page + 1;
    total_pages = state->series_total_pages;
  } else {
    cur_page = state->books_current_page + 1;
    total_pages = state->books_total_pages;
  }

  char page_info[64];
  snprintf(page_info, sizeof(page_info), "Page %d/%d", cur_page, total_pages);
  draw_text(app, page_info, 15, win_h - FOOTER_HEIGHT + 8, COLOR_WHITE);

  const char *keys = (state->current_view == BROWSER_SERIES)
                         ? "Enter:Open  Tab:Library  PgUp/Dn:Page  ESC:Quit"
                         : "Enter:Read  D:Download  Bksp:Back  PgUp/Dn:Page";
  int tw, th;
  if (app->font) {
    TTF_SizeText(app->font, keys, &tw, &th);
    draw_text(app, keys, win_w - tw - 15, win_h - FOOTER_HEIGHT + 8,
              COLOR_GRAY);
  }
}

void browser_render(BrowserState *state, AppContext *app) {
  SDL_SetRenderDrawColor(app->renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b,
                         255);
  SDL_RenderClear(app->renderer);

  int win_w, win_h;
  SDL_GetRendererOutputSize(app->renderer, &win_w, &win_h);

  // Update grid_cols based on window width
  int cell_w = COVER_WIDTH + COVER_PADDING;
  state->grid_cols = (win_w - COVER_PADDING) / cell_w;
  if (state->grid_cols < 1)
    state->grid_cols = 1;

  render_tab_bar(state, app, win_w);
  render_breadcrumb(state, app, win_w);

  int content_y = TAB_HEIGHT + BREADCRUMB_HEIGHT;

  if (state->is_loading) {
    draw_text(app, "Loading...", win_w / 2 - 50, win_h / 2, COLOR_WHITE);
  } else if (state->current_view == BROWSER_SERIES) {
    // Build names array
    const char **names = NULL;
    if (state->series_count > 0) {
      names = malloc(sizeof(char *) * state->series_count);
      for (int i = 0; i < state->series_count; i++)
        names[i] = state->series_list[i].name;
    }

    render_cover_grid(app, state->series_covers, names, state->series_count,
                      state->selected_series, state->grid_cols,
                      state->grid_scroll_y, content_y, win_w,
                      win_h - FOOTER_HEIGHT);

    free(names);

    if (state->series_count == 0)
      draw_text(app, "No series found.", win_w / 2 - 80, win_h / 2,
                COLOR_GRAY);
  } else {
    const char **names = NULL;
    if (state->books_count > 0) {
      names = malloc(sizeof(char *) * state->books_count);
      for (int i = 0; i < state->books_count; i++)
        names[i] = state->books_list[i].name;
    }

    render_cover_grid(app, state->book_covers, names, state->books_count,
                      state->selected_book, state->grid_cols,
                      state->grid_scroll_y, content_y, win_w,
                      win_h - FOOTER_HEIGHT);

    free(names);

    if (state->books_count == 0)
      draw_text(app, "No books found.", win_w / 2 - 70, win_h / 2,
                COLOR_GRAY);
  }

  // Download progress overlay
  if (state->is_downloading) {
    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(app->renderer, NULL);

    draw_text(app, state->download_status, win_w / 2 - 100, win_h / 2 - 30,
              COLOR_WHITE);

    // Progress bar
    int bar_w = 300;
    int bar_h = 20;
    SDL_Rect bar_bg = {win_w / 2 - bar_w / 2, win_h / 2, bar_w, bar_h};
    SDL_SetRenderDrawColor(app->renderer, 60, 60, 60, 255);
    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_NONE);
    SDL_RenderFillRect(app->renderer, &bar_bg);

    int fill_w = (int)(bar_w * state->download_progress);
    SDL_Rect bar_fill = {bar_bg.x, bar_bg.y, fill_w, bar_h};
    SDL_SetRenderDrawColor(app->renderer, COLOR_ACCENT.r, COLOR_ACCENT.g,
                           COLOR_ACCENT.b, 255);
    SDL_RenderFillRect(app->renderer, &bar_fill);

    SDL_SetRenderDrawColor(app->renderer, COLOR_ACCENT.r, COLOR_ACCENT.g,
                           COLOR_ACCENT.b, 255);
    SDL_RenderDrawRect(app->renderer, &bar_bg);
  }

  render_footer(state, app, win_w, win_h);
  SDL_RenderPresent(app->renderer);
}

// --- Input Handling ---

static int get_item_at_pos(BrowserState *state, int mx, int my, int win_w) {
  int cell_w = COVER_WIDTH + COVER_PADDING;
  int cell_h = (int)(COVER_WIDTH * 1.4f) + COVER_LABEL_HEIGHT + COVER_PADDING;
  int grid_cols = (win_w - COVER_PADDING) / cell_w;
  if (grid_cols < 1)
    grid_cols = 1;

  int start_x = (win_w - grid_cols * cell_w) / 2 + COVER_PADDING / 2;
  int content_y = TAB_HEIGHT + BREADCRUMB_HEIGHT;

  int count = (state->current_view == BROWSER_SERIES) ? state->series_count
                                                       : state->books_count;

  for (int i = 0; i < count; i++) {
    int col = i % grid_cols;
    int row = i / grid_cols;

    int x = start_x + col * cell_w;
    int y = content_y + row * cell_h - state->grid_scroll_y;

    if (mx >= x && mx < x + COVER_WIDTH && my >= y &&
        my < y + (int)(COVER_WIDTH * 1.4f) + COVER_LABEL_HEIGHT) {
      return i;
    }
  }
  return -1;
}

BrowserResult browser_handle_event(BrowserState *state, SDL_Event *event,
                                   KomgaClient *client, AppContext *app) {
  BrowserResult result = {BROWSER_ACTION_NONE, "", "", MODE_MANGA};

  if (state->is_downloading || state->is_loading)
    return result;

  int win_w, win_h;
  SDL_GetRendererOutputSize(app->renderer, &win_w, &win_h);

  int count = (state->current_view == BROWSER_SERIES) ? state->series_count
                                                       : state->books_count;
  int *selected = (state->current_view == BROWSER_SERIES)
                      ? &state->selected_series
                      : &state->selected_book;

  if (event->type == SDL_MOUSEWHEEL) {
    state->grid_scroll_y -= event->wheel.y * SCROLL_STEP;
    if (state->grid_scroll_y < 0)
      state->grid_scroll_y = 0;
    return result;
  }

  if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
    // Check if click is on tab bar
    if (event->button.y < TAB_HEIGHT && state->libraries) {
      int tab_w = win_w / state->library_count;
      if (tab_w > 250)
        tab_w = 250;
      int tab_idx = event->button.x / tab_w;
      if (tab_idx >= 0 && tab_idx < state->library_count &&
          tab_idx != state->selected_library) {
        state->selected_library = tab_idx;
        state->current_view = BROWSER_SERIES;
        browser_load_series(state, client, app->renderer, 0);
      }
      return result;
    }

    // Check if click is on a grid item
    int idx = get_item_at_pos(state, event->button.x, event->button.y, win_w);
    if (idx >= 0) {
      *selected = idx;

      if (state->current_view == BROWSER_SERIES) {
        // Enter series
        browser_load_books(state, client, app->renderer, 0);
        state->current_view = BROWSER_BOOKS;
      } else {
        // Open book
        result.action = BROWSER_ACTION_OPEN_BOOK;
        strncpy(result.book_id, state->books_list[idx].id,
                sizeof(result.book_id) - 1);
        strncpy(result.series_name, state->selected_series_name,
                sizeof(result.series_name) - 1);
        result.suggested_mode = detect_mode_from_library(
            state->libraries[state->selected_library].name);
      }
    }
    return result;
  }

  if (event->type != SDL_KEYDOWN)
    return result;

  SDL_Keycode key = event->key.keysym.sym;

  switch (key) {
  case SDLK_LEFT:
    if (*selected > 0)
      (*selected)--;
    break;

  case SDLK_RIGHT:
    if (*selected < count - 1)
      (*selected)++;
    break;

  case SDLK_UP:
    if (*selected >= state->grid_cols)
      *selected -= state->grid_cols;
    break;

  case SDLK_DOWN:
    if (*selected + state->grid_cols < count)
      *selected += state->grid_cols;
    break;

  case SDLK_RETURN:
    if (count > 0) {
      if (state->current_view == BROWSER_SERIES) {
        browser_load_books(state, client, app->renderer, 0);
        state->current_view = BROWSER_BOOKS;
      } else {
        result.action = BROWSER_ACTION_OPEN_BOOK;
        strncpy(result.book_id, state->books_list[*selected].id,
                sizeof(result.book_id) - 1);
        strncpy(result.series_name, state->selected_series_name,
                sizeof(result.series_name) - 1);
        result.suggested_mode = detect_mode_from_library(
            state->libraries[state->selected_library].name);
      }
    }
    break;

  case SDLK_d:
    if (state->current_view == BROWSER_BOOKS && count > 0) {
      result.action = BROWSER_ACTION_DOWNLOAD_BOOK;
      strncpy(result.book_id, state->books_list[*selected].id,
              sizeof(result.book_id) - 1);
      strncpy(result.series_name, state->selected_series_name,
              sizeof(result.series_name) - 1);
      result.suggested_mode = detect_mode_from_library(
          state->libraries[state->selected_library].name);
    }
    break;

  case SDLK_BACKSPACE:
    if (state->current_view == BROWSER_BOOKS) {
      state->current_view = BROWSER_SERIES;
      state->grid_scroll_y = 0;
    } else {
      result.action = BROWSER_ACTION_QUIT;
    }
    break;

  case SDLK_ESCAPE:
    if (state->current_view == BROWSER_BOOKS) {
      state->current_view = BROWSER_SERIES;
      state->grid_scroll_y = 0;
    } else {
      result.action = BROWSER_ACTION_QUIT;
    }
    break;

  case SDLK_TAB: {
    int next = (state->selected_library + 1) % state->library_count;
    if (next != state->selected_library) {
      state->selected_library = next;
      state->current_view = BROWSER_SERIES;
      browser_load_series(state, client, app->renderer, 0);
    }
    break;
  }

  case SDLK_1:
  case SDLK_2:
  case SDLK_3:
  case SDLK_4: {
    int idx = key - SDLK_1;
    if (idx < state->library_count && idx != state->selected_library) {
      state->selected_library = idx;
      state->current_view = BROWSER_SERIES;
      browser_load_series(state, client, app->renderer, 0);
    }
    break;
  }

  case SDLK_PAGEDOWN: {
    int cur_page = (state->current_view == BROWSER_SERIES)
                       ? state->series_current_page
                       : state->books_current_page;
    int total = (state->current_view == BROWSER_SERIES)
                    ? state->series_total_pages
                    : state->books_total_pages;
    if (cur_page + 1 < total) {
      if (state->current_view == BROWSER_SERIES)
        browser_load_series(state, client, app->renderer, cur_page + 1);
      else
        browser_load_books(state, client, app->renderer, cur_page + 1);
    }
    break;
  }

  case SDLK_PAGEUP: {
    int cur_page = (state->current_view == BROWSER_SERIES)
                       ? state->series_current_page
                       : state->books_current_page;
    if (cur_page > 0) {
      if (state->current_view == BROWSER_SERIES)
        browser_load_series(state, client, app->renderer, cur_page - 1);
      else
        browser_load_books(state, client, app->renderer, cur_page - 1);
    }
    break;
  }

  default:
    break;
  }

  // Auto-scroll to keep selected item visible
  if (count > 0 && state->grid_cols > 0) {
    int cell_h =
        (int)(COVER_WIDTH * 1.4f) + COVER_LABEL_HEIGHT + COVER_PADDING;
    int row = *selected / state->grid_cols;
    int item_y = row * cell_h;
    int visible_h = win_h - TAB_HEIGHT - BREADCRUMB_HEIGHT - FOOTER_HEIGHT;

    if (item_y < state->grid_scroll_y)
      state->grid_scroll_y = item_y;
    else if (item_y + cell_h > state->grid_scroll_y + visible_h)
      state->grid_scroll_y = item_y + cell_h - visible_h;
  }

  return result;
}
