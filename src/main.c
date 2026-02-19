#include "bookmark_manager.h"
#include "browser_ui.h"
#include "cbz_handler.h"
#include "config.h"
#include "file_utils.h"
#include "komga_client.h"
#include "page_provider.h"
#include "render_engine.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// --- Global State ---
ViewMode view_mode = VIEW_SINGLE;
ManhwaScale manhwa_scale = SCALE_FIT_HEIGHT;
int show_help = 0;
int scroll_y = 0;

// --- File Navigation State ---
char current_file_path[1024];
int prompt_next = 0;
char next_file_path[1024];

void reset_view() {
  scroll_y = 0;
  prompt_next = 0;
}

void load_new_file(MangaBook *book, AppContext *app, const char *new_path);
void refresh_page(MangaBook *book, AppContext *app);
void toggle_fullscreen(AppContext *app);
ReadMode detect_mode(const char *path);

// --- Forward declarations for Komga reader ---
void refresh_page_komga(PageProvider *prov, AppContext *app);

// ==========================================================
// LOCAL FILE HELPERS (unchanged from original)
// ==========================================================

void load_new_file(MangaBook *book, AppContext *app, const char *new_path) {
  save_bookmark(current_file_path, book->current_index);
  close_cbz(book);

  if (open_cbz(new_path, book) != 0) {
    printf("Failed to open %s\n", new_path);
    exit(1);
  }
  strncpy(current_file_path, new_path, 1023);

  // Auto-detect Mode
  char lower[1024];
  strncpy(lower, new_path, 1023);
  for (int i = 0; lower[i]; i++)
    lower[i] = tolower(lower[i]);

  if (strstr(lower, "manhwa") || strstr(lower, "webtoon"))
    book->mode = MODE_MANHWA;
  else if (strstr(lower, "manhua"))
    book->mode = MODE_MANHUA;
  else if (strstr(lower, "comic"))
    book->mode = MODE_COMIC;
  else
    book->mode = MODE_MANGA;

  if (book->mode == MODE_MANHWA) {
    view_mode = VIEW_MANHWA;
    manhwa_scale = SCALE_FIT_HEIGHT;
  } else {
    view_mode = VIEW_SINGLE;
  }

  int saved = load_bookmark(new_path);
  book->current_index = (saved > 0 && saved < book->count) ? saved : 0;
  reset_view();
}

void refresh_page(MangaBook *book, AppContext *app) {
  size_t size;
  char *data;

  // 1. CURRENT
  data = get_image_data(book, &size);
  load_texture_to_slot(app, data, size, 0);
  if (data)
    free(data);

  // 2. NEXT
  int load_next = (view_mode == VIEW_MANHWA || view_mode == VIEW_DOUBLE);
  if (view_mode == VIEW_DOUBLE_COVER && book->current_index > 0)
    load_next = 1;

  if (load_next && (book->current_index + 1 < book->count)) {
    book->current_index++;
    data = get_image_data(book, &size);
    load_texture_to_slot(app, data, size, 1);
    if (data)
      free(data);
    book->current_index--;
  } else {
    load_texture_to_slot(app, NULL, 0, 1);
  }

  // 3. PREVIOUS
  if (view_mode == VIEW_MANHWA && book->current_index > 0) {
    book->current_index--;
    data = get_image_data(book, &size);
    load_texture_to_slot(app, data, size, -1);
    if (data)
      free(data);
    book->current_index++;
  } else {
    load_texture_to_slot(app, NULL, 0, -1);
  }

  char title[256];
  const char *mode_str = (book->mode == MODE_MANHWA) ? "Manhwa" : "Reader";
  snprintf(title, sizeof(title), "%s - Page %d / %d", mode_str,
           book->current_index + 1, book->count);
  SDL_SetWindowTitle(app->window, title);
}

void toggle_fullscreen(AppContext *app) {
  Uint32 flags = SDL_GetWindowFlags(app->window);
  SDL_SetWindowFullscreen(app->window, (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
                                           ? 0
                                           : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

ReadMode detect_mode(const char *path) {
  char lower[1024];
  strncpy(lower, path, 1023);
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

// ==========================================================
// KOMGA READER HELPERS
// ==========================================================

void refresh_page_komga(PageProvider *prov, AppContext *app) {
  size_t size;
  char *data;

  // 1. CURRENT
  data = provider_get_page(prov, prov->current_index, &size);
  load_texture_to_slot(app, data, size, 0);
  if (data)
    free(data);

  // 2. NEXT
  int load_next = (view_mode == VIEW_MANHWA || view_mode == VIEW_DOUBLE);
  if (view_mode == VIEW_DOUBLE_COVER && prov->current_index > 0)
    load_next = 1;

  if (load_next && (prov->current_index + 1 < prov->count)) {
    data = provider_get_page(prov, prov->current_index + 1, &size);
    load_texture_to_slot(app, data, size, 1);
    if (data)
      free(data);
  } else {
    load_texture_to_slot(app, NULL, 0, 1);
  }

  // 3. PREVIOUS
  if (view_mode == VIEW_MANHWA && prov->current_index > 0) {
    data = provider_get_page(prov, prov->current_index - 1, &size);
    load_texture_to_slot(app, data, size, -1);
    if (data)
      free(data);
  } else {
    load_texture_to_slot(app, NULL, 0, -1);
  }

  char title[256];
  const char *mode_str = (prov->read_mode == MODE_MANHWA) ? "Manhwa" : "Reader";
  snprintf(title, sizeof(title), "%s - Page %d / %d [Komga]", mode_str,
           prov->current_index + 1, prov->count);
  SDL_SetWindowTitle(app->window, title);
}

// ==========================================================
// RUN_READER_LOCAL — original reader, extracted from main()
// ==========================================================

void run_reader_local(AppContext *app, const char *filepath) {
  MangaBook book;
  if (open_cbz(filepath, &book) != 0) {
    printf("Failed to open %s\n", filepath);
    return;
  }
  strncpy(current_file_path, filepath, 1023);
  book.mode = detect_mode(filepath);

  if (book.mode == MODE_MANHWA) {
    view_mode = VIEW_MANHWA;
    manhwa_scale = SCALE_FIT_HEIGHT;
  } else {
    view_mode = VIEW_SINGLE;
  }

  int saved = load_bookmark(filepath);
  if (saved > 0 && saved < book.count)
    book.current_index = saved;

  refresh_page(&book, app);

  int running = 1;
  SDL_Event e;
  char overlay[32];
  char input_buf[10] = "";
  int input_mode = 0;
  const int SCROLL_STEP = 60;

  while (running) {

    // --- Continuous Scroll Logic ---
    if (view_mode == VIEW_MANHWA && !prompt_next) {
      int curr_h = get_scaled_height(app, 0, manhwa_scale);
      if (curr_h > 0 && scroll_y >= curr_h) {
        if (book.current_index < book.count - 1) {
          book.current_index++;
          scroll_y -= curr_h;
          refresh_page(&book, app);
        }
      } else if (scroll_y < 0) {
        if (book.current_index > 0) {
          int prev_h = get_scaled_height(app, -1, manhwa_scale);
          book.current_index--;
          scroll_y += prev_h;
          refresh_page(&book, app);
        } else {
          scroll_y = 0;
        }
      }
    }

    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = 0;
      else if (e.type == SDL_MOUSEWHEEL && view_mode == VIEW_MANHWA &&
               !input_mode && !show_help && !prompt_next) {
        scroll_y -= e.wheel.y * SCROLL_STEP;
      } else if (show_help) {
        if (e.type == SDL_KEYDOWN &&
            (e.key.keysym.sym == SDLK_h || e.key.keysym.sym == SDLK_ESCAPE))
          show_help = 0;
      } else if (input_mode) {
        if (e.type == SDL_TEXTINPUT) {
          if (isdigit(e.text.text[0]) && strlen(input_buf) < 5)
            strcat(input_buf, e.text.text);
        } else if (e.type == SDL_KEYDOWN) {
          if (e.key.keysym.sym == SDLK_RETURN) {
            int p = atoi(input_buf);
            if (p > 0 && p <= book.count) {
              book.current_index = p - 1;
              reset_view();
              refresh_page(&book, app);
            }
            input_mode = 0;
            SDL_StopTextInput();
          } else if (e.key.keysym.sym == SDLK_ESCAPE) {
            input_mode = 0;
            SDL_StopTextInput();
          } else if (e.key.keysym.sym == SDLK_BACKSPACE) {
            int l = strlen(input_buf);
            if (l > 0)
              input_buf[l - 1] = 0;
          }
        }
      } else if (e.type == SDL_KEYDOWN) {
        if (prompt_next != 0 && e.key.keysym.sym == SDLK_ESCAPE) {
          prompt_next = 0;
        } else {
          int changed = 0;
          int shift = SDL_GetModState() & KMOD_SHIFT;
          int step =
              (view_mode == VIEW_SINGLE || view_mode == VIEW_MANHWA) ? 1 : 2;
          int left_is_next = (book.mode == MODE_MANGA);

          switch (e.key.keysym.sym) {
          case SDLK_DOWN:
            if (view_mode == VIEW_MANHWA)
              scroll_y += SCROLL_STEP;
            break;
          case SDLK_UP:
            if (view_mode == VIEW_MANHWA)
              scroll_y -= SCROLL_STEP;
            break;

          case SDLK_LEFT:
            if (left_is_next) {
              if (prompt_next == 1) {
                if (get_neighbor_file(current_file_path, 1, next_file_path,
                                      1024)) {
                  load_new_file(&book, app, next_file_path);
                  refresh_page(&book, app);
                }
              } else if (prompt_next == -1) {
                prompt_next = 0;
              } else {
                if (book.current_index + step >= book.count) {
                  if (get_neighbor_file(current_file_path, 1, next_file_path,
                                        1024))
                    prompt_next = 1;
                  else {
                    book.current_index = book.count - 1;
                    changed = 1;
                  }
                } else {
                  book.current_index += step;
                  changed = 1;
                }
              }
            } else {
              if (prompt_next == -1) {
                if (get_neighbor_file(current_file_path, -1, next_file_path,
                                      1024)) {
                  load_new_file(&book, app, next_file_path);
                  refresh_page(&book, app);
                }
              } else if (prompt_next == 1) {
                prompt_next = 0;
              } else {
                if (book.current_index == 0) {
                  if (get_neighbor_file(current_file_path, -1, next_file_path,
                                        1024))
                    prompt_next = -1;
                } else {
                  book.current_index -= step;
                  if (book.current_index < 0)
                    book.current_index = 0;
                  changed = 1;
                }
              }
            }
            break;

          case SDLK_RIGHT:
            if (!left_is_next) {
              if (prompt_next == 1) {
                if (get_neighbor_file(current_file_path, 1, next_file_path,
                                      1024)) {
                  load_new_file(&book, app, next_file_path);
                  refresh_page(&book, app);
                }
              } else if (prompt_next == -1) {
                prompt_next = 0;
              } else {
                if (book.current_index + step >= book.count) {
                  if (get_neighbor_file(current_file_path, 1, next_file_path,
                                        1024))
                    prompt_next = 1;
                  else {
                    book.current_index = book.count - 1;
                    changed = 1;
                  }
                } else {
                  book.current_index += step;
                  changed = 1;
                }
              }
            } else {
              if (prompt_next == -1) {
                if (get_neighbor_file(current_file_path, -1, next_file_path,
                                      1024)) {
                  load_new_file(&book, app, next_file_path);
                  refresh_page(&book, app);
                }
              } else if (prompt_next == 1) {
                prompt_next = 0;
              } else {
                if (book.current_index == 0) {
                  if (get_neighbor_file(current_file_path, -1, next_file_path,
                                        1024))
                    prompt_next = -1;
                } else {
                  book.current_index -= step;
                  if (book.current_index < 0)
                    book.current_index = 0;
                  changed = 1;
                }
              }
            }
            break;

          case SDLK_s:
            if (book.mode == MODE_MANHWA) {
              view_mode = VIEW_MANHWA;
              manhwa_scale = SCALE_FIT_HEIGHT;
              reset_view();
            } else {
              view_mode = VIEW_SINGLE;
            }
            changed = 1;
            break;

          case SDLK_d:
            if (book.mode == MODE_MANHWA) {
              if (!shift) {
                view_mode = VIEW_MANHWA;
                manhwa_scale = SCALE_FIT_WIDTH;
                reset_view();
              }
            } else {
              if (shift)
                view_mode = (view_mode == VIEW_DOUBLE_COVER)
                                ? VIEW_SINGLE
                                : VIEW_DOUBLE_COVER;
              else
                view_mode =
                    (view_mode == VIEW_DOUBLE) ? VIEW_SINGLE : VIEW_DOUBLE;
            }
            changed = 1;
            break;

          case SDLK_b:
            book.current_index = 0;
            reset_view();
            changed = 1;
            break;
          case SDLK_e:
            book.current_index = book.count - 1;
            reset_view();
            changed = 1;
            break;
          case SDLK_f:
            toggle_fullscreen(app);
            break;
          case SDLK_g:
            input_mode = 1;
            input_buf[0] = 0;
            SDL_StartTextInput();
            break;
          case SDLK_h:
            show_help = !show_help;
            break;
          case SDLK_ESCAPE:
            if (SDL_GetWindowFlags(app->window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
              SDL_SetWindowFullscreen(app->window, 0);
            else
              running = 0;
            break;
          }

          // --- ALIGNMENT CORRECTION ---
          if ((view_mode == VIEW_DOUBLE_COVER) && book.current_index > 0 &&
              book.current_index % 2 == 0) {
            book.current_index--;
          } else if (view_mode == VIEW_DOUBLE &&
                     book.current_index % 2 != 0) {
            book.current_index--;
          }

          if (changed)
            refresh_page(&book, app);
        }
      }
    }

    snprintf(overlay, 32, "%d / %d", book.current_index + 1, book.count);
    PageDir p_dir = (book.mode == MODE_MANGA) ? DIR_MANGA : DIR_COMIC;

    const char *popup_msg = NULL;
    if (prompt_next == 1)
      popup_msg = "End of Volume. Press Next again to continue.";
    else if (prompt_next == -1)
      popup_msg = "Start of Volume. Press Back again to go back.";

    render_frame(app, overlay, input_mode ? input_buf : NULL, view_mode,
                 manhwa_scale, p_dir, show_help, scroll_y, book.mode,
                 popup_msg);
  }

  save_bookmark(current_file_path, book.current_index);
  close_cbz(&book);
}

// ==========================================================
// RUN_READER_KOMGA — streaming reader from Komga
// ==========================================================

void run_reader_komga(AppContext *app, KomgaClient *client,
                      const char *book_id, ReadMode mode) {
  PageProvider prov;
  if (provider_open_komga(&prov, client, book_id, mode) != 0) {
    printf("Failed to open Komga book %s\n", book_id);
    return;
  }

  // Sync bookmark: pick higher of Komga progress vs local
  int local_page = 0, local_completed = 0;
  if (load_komga_progress(book_id, &local_page, &local_completed) == 0) {
    if (local_page > prov.current_index && local_page < prov.count)
      prov.current_index = local_page;
  }

  if (mode == MODE_MANHWA) {
    view_mode = VIEW_MANHWA;
    manhwa_scale = SCALE_FIT_HEIGHT;
  } else {
    view_mode = VIEW_SINGLE;
  }
  reset_view();

  refresh_page_komga(&prov, app);

  int running = 1;
  SDL_Event e;
  char overlay[32];
  char input_buf[10] = "";
  int input_mode = 0;
  const int SCROLL_STEP = 60;
  int pages_since_sync = 0;
  int komga_prompt_next = 0;

  while (running) {

    // --- Continuous Scroll Logic (Manhwa) ---
    if (view_mode == VIEW_MANHWA && !komga_prompt_next) {
      int curr_h = get_scaled_height(app, 0, manhwa_scale);
      if (curr_h > 0 && scroll_y >= curr_h) {
        if (prov.current_index < prov.count - 1) {
          prov.current_index++;
          scroll_y -= curr_h;
          refresh_page_komga(&prov, app);
          save_komga_progress(book_id, prov.current_index, 0);
          pages_since_sync++;
        }
      } else if (scroll_y < 0) {
        if (prov.current_index > 0) {
          int prev_h = get_scaled_height(app, -1, manhwa_scale);
          prov.current_index--;
          scroll_y += prev_h;
          refresh_page_komga(&prov, app);
        } else {
          scroll_y = 0;
        }
      }
    }

    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = 0;
      } else if (e.type == SDL_MOUSEWHEEL && view_mode == VIEW_MANHWA &&
                 !input_mode && !show_help && !komga_prompt_next) {
        scroll_y -= e.wheel.y * SCROLL_STEP;
      } else if (show_help) {
        if (e.type == SDL_KEYDOWN &&
            (e.key.keysym.sym == SDLK_h || e.key.keysym.sym == SDLK_ESCAPE))
          show_help = 0;
      } else if (input_mode) {
        if (e.type == SDL_TEXTINPUT) {
          if (isdigit(e.text.text[0]) && strlen(input_buf) < 5)
            strcat(input_buf, e.text.text);
        } else if (e.type == SDL_KEYDOWN) {
          if (e.key.keysym.sym == SDLK_RETURN) {
            int p = atoi(input_buf);
            if (p > 0 && p <= prov.count) {
              prov.current_index = p - 1;
              reset_view();
              refresh_page_komga(&prov, app);
            }
            input_mode = 0;
            SDL_StopTextInput();
          } else if (e.key.keysym.sym == SDLK_ESCAPE) {
            input_mode = 0;
            SDL_StopTextInput();
          } else if (e.key.keysym.sym == SDLK_BACKSPACE) {
            int l = strlen(input_buf);
            if (l > 0)
              input_buf[l - 1] = 0;
          }
        }
      } else if (e.type == SDL_KEYDOWN) {
        if (komga_prompt_next != 0 && e.key.keysym.sym == SDLK_ESCAPE) {
          komga_prompt_next = 0;
        } else {
          int changed = 0;
          int step =
              (view_mode == VIEW_SINGLE || view_mode == VIEW_MANHWA) ? 1 : 2;
          int left_is_next = (mode == MODE_MANGA);

          switch (e.key.keysym.sym) {
          case SDLK_DOWN:
            if (view_mode == VIEW_MANHWA)
              scroll_y += SCROLL_STEP;
            break;
          case SDLK_UP:
            if (view_mode == VIEW_MANHWA)
              scroll_y -= SCROLL_STEP;
            break;

          case SDLK_LEFT:
            if (left_is_next) {
              // MANGA: left = next
              if (komga_prompt_next == 1) {
                // Move to next volume
                KomgaBook next_book;
                if (komga_get_next_book(client, book_id, &next_book) == 0) {
                  // Save progress, close, reopen with next book
                  int completed = (prov.current_index >= prov.count - 1);
                  komga_update_read_progress(client, book_id,
                                             prov.current_index + 1, completed);
                  save_komga_progress(book_id, prov.current_index, completed);
                  provider_close(&prov);
                  if (provider_open_komga(&prov, client, next_book.id, mode) ==
                      0) {
                    strncpy((char *)book_id, next_book.id, 63);
                    prov.current_index = 0;
                    reset_view();
                    pages_since_sync = 0;
                    refresh_page_komga(&prov, app);
                  } else {
                    running = 0;
                  }
                }
                komga_prompt_next = 0;
              } else if (komga_prompt_next == -1) {
                komga_prompt_next = 0;
              } else {
                if (prov.current_index + step >= prov.count) {
                  KomgaBook tmp;
                  if (komga_get_next_book(client, book_id, &tmp) == 0)
                    komga_prompt_next = 1;
                  else {
                    prov.current_index = prov.count - 1;
                    changed = 1;
                  }
                } else {
                  prov.current_index += step;
                  changed = 1;
                }
              }
            } else {
              // COMIC: left = prev
              if (komga_prompt_next == -1) {
                KomgaBook prev_book;
                if (komga_get_prev_book(client, book_id, &prev_book) == 0) {
                  komga_update_read_progress(client, book_id,
                                             prov.current_index + 1, 0);
                  save_komga_progress(book_id, prov.current_index, 0);
                  provider_close(&prov);
                  if (provider_open_komga(&prov, client, prev_book.id, mode) ==
                      0) {
                    strncpy((char *)book_id, prev_book.id, 63);
                    prov.current_index = prov.count - 1;
                    reset_view();
                    pages_since_sync = 0;
                    refresh_page_komga(&prov, app);
                  } else {
                    running = 0;
                  }
                }
                komga_prompt_next = 0;
              } else if (komga_prompt_next == 1) {
                komga_prompt_next = 0;
              } else {
                if (prov.current_index == 0) {
                  KomgaBook tmp;
                  if (komga_get_prev_book(client, book_id, &tmp) == 0)
                    komga_prompt_next = -1;
                } else {
                  prov.current_index -= step;
                  if (prov.current_index < 0)
                    prov.current_index = 0;
                  changed = 1;
                }
              }
            }
            break;

          case SDLK_RIGHT:
            if (!left_is_next) {
              // COMIC: right = next
              if (komga_prompt_next == 1) {
                KomgaBook next_book;
                if (komga_get_next_book(client, book_id, &next_book) == 0) {
                  int completed = (prov.current_index >= prov.count - 1);
                  komga_update_read_progress(client, book_id,
                                             prov.current_index + 1, completed);
                  save_komga_progress(book_id, prov.current_index, completed);
                  provider_close(&prov);
                  if (provider_open_komga(&prov, client, next_book.id, mode) ==
                      0) {
                    strncpy((char *)book_id, next_book.id, 63);
                    prov.current_index = 0;
                    reset_view();
                    pages_since_sync = 0;
                    refresh_page_komga(&prov, app);
                  } else {
                    running = 0;
                  }
                }
                komga_prompt_next = 0;
              } else if (komga_prompt_next == -1) {
                komga_prompt_next = 0;
              } else {
                if (prov.current_index + step >= prov.count) {
                  KomgaBook tmp;
                  if (komga_get_next_book(client, book_id, &tmp) == 0)
                    komga_prompt_next = 1;
                  else {
                    prov.current_index = prov.count - 1;
                    changed = 1;
                  }
                } else {
                  prov.current_index += step;
                  changed = 1;
                }
              }
            } else {
              // MANGA: right = prev
              if (komga_prompt_next == -1) {
                KomgaBook prev_book;
                if (komga_get_prev_book(client, book_id, &prev_book) == 0) {
                  komga_update_read_progress(client, book_id,
                                             prov.current_index + 1, 0);
                  save_komga_progress(book_id, prov.current_index, 0);
                  provider_close(&prov);
                  if (provider_open_komga(&prov, client, prev_book.id, mode) ==
                      0) {
                    strncpy((char *)book_id, prev_book.id, 63);
                    prov.current_index = prov.count - 1;
                    reset_view();
                    pages_since_sync = 0;
                    refresh_page_komga(&prov, app);
                  } else {
                    running = 0;
                  }
                }
                komga_prompt_next = 0;
              } else if (komga_prompt_next == 1) {
                komga_prompt_next = 0;
              } else {
                if (prov.current_index == 0) {
                  KomgaBook tmp;
                  if (komga_get_prev_book(client, book_id, &tmp) == 0)
                    komga_prompt_next = -1;
                } else {
                  prov.current_index -= step;
                  if (prov.current_index < 0)
                    prov.current_index = 0;
                  changed = 1;
                }
              }
            }
            break;

          case SDLK_s:
            if (mode == MODE_MANHWA) {
              view_mode = VIEW_MANHWA;
              manhwa_scale = SCALE_FIT_HEIGHT;
              reset_view();
            } else {
              view_mode = VIEW_SINGLE;
            }
            changed = 1;
            break;

          case SDLK_d:
            if (mode == MODE_MANHWA) {
              if (!(SDL_GetModState() & KMOD_SHIFT)) {
                view_mode = VIEW_MANHWA;
                manhwa_scale = SCALE_FIT_WIDTH;
                reset_view();
              }
            } else {
              if (SDL_GetModState() & KMOD_SHIFT)
                view_mode = (view_mode == VIEW_DOUBLE_COVER)
                                ? VIEW_SINGLE
                                : VIEW_DOUBLE_COVER;
              else
                view_mode =
                    (view_mode == VIEW_DOUBLE) ? VIEW_SINGLE : VIEW_DOUBLE;
            }
            changed = 1;
            break;

          case SDLK_b:
            prov.current_index = 0;
            reset_view();
            changed = 1;
            break;
          case SDLK_e:
            prov.current_index = prov.count - 1;
            reset_view();
            changed = 1;
            break;
          case SDLK_f:
            toggle_fullscreen(app);
            break;
          case SDLK_g:
            input_mode = 1;
            input_buf[0] = 0;
            SDL_StartTextInput();
            break;
          case SDLK_h:
            show_help = !show_help;
            break;
          case SDLK_ESCAPE:
            if (SDL_GetWindowFlags(app->window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
              SDL_SetWindowFullscreen(app->window, 0);
            else
              running = 0;
            break;
          }

          // Alignment correction
          if ((view_mode == VIEW_DOUBLE_COVER) && prov.current_index > 0 &&
              prov.current_index % 2 == 0) {
            prov.current_index--;
          } else if (view_mode == VIEW_DOUBLE &&
                     prov.current_index % 2 != 0) {
            prov.current_index--;
          }

          if (changed) {
            refresh_page_komga(&prov, app);
            save_komga_progress(book_id, prov.current_index, 0);
            pages_since_sync++;
            if (pages_since_sync >= 5) {
              komga_update_read_progress(client, book_id,
                                         prov.current_index + 1, 0);
              pages_since_sync = 0;
            }
          }
        }
      }
    }

    snprintf(overlay, 32, "%d / %d", prov.current_index + 1, prov.count);
    PageDir p_dir = (mode == MODE_MANGA) ? DIR_MANGA : DIR_COMIC;

    const char *popup_msg = NULL;
    if (komga_prompt_next == 1)
      popup_msg = "End of Volume. Press Next again to continue.";
    else if (komga_prompt_next == -1)
      popup_msg = "Start of Volume. Press Back again to go back.";

    render_frame(app, overlay, input_mode ? input_buf : NULL, view_mode,
                 manhwa_scale, p_dir, show_help, scroll_y, mode, popup_msg);
  }

  // Final sync
  int completed = (prov.current_index >= prov.count - 1);
  komga_update_read_progress(client, book_id, prov.current_index + 1,
                             completed);
  save_komga_progress(book_id, prov.current_index, completed);
  provider_close(&prov);
  clear_slots(app);
}

// ==========================================================
// RUN_BROWSER — Komga library browser
// ==========================================================

static void mkdir_p(const char *path) {
  char tmp[1024];
  strncpy(tmp, path, sizeof(tmp) - 1);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  mkdir(tmp, 0755);
}

void run_browser(AppContext *app, AppConfig *config) {
  KomgaClient client;
  if (komga_init(&client, config->komga_url, config->komga_api_key,
                 config->komga_username, config->komga_password) != 0) {
    printf("Failed to initialize Komga client\n");
    return;
  }

  if (komga_test_connection(&client) != 0) {
    printf("Cannot connect to Komga at %s\n", config->komga_url);
    komga_cleanup(&client);
    return;
  }

  BrowserState state;
  browser_init(&state);

  if (browser_load_libraries(&state, &client) != 0) {
    printf("Failed to load libraries from Komga\n");
    browser_cleanup(&state, app->renderer);
    komga_cleanup(&client);
    return;
  }

  // Load first library's series
  browser_load_series(&state, &client, app->renderer, 0);

  SDL_SetWindowTitle(app->window, "Manga Reader - Library Browser");

  int running = 1;
  SDL_Event e;

  while (running) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = 0;
        break;
      }

      BrowserResult result =
          browser_handle_event(&state, &e, &client, app);

      switch (result.action) {
      case BROWSER_ACTION_OPEN_BOOK:
        // Enter reader mode
        run_reader_komga(app, &client, result.book_id,
                         result.suggested_mode);
        SDL_SetWindowTitle(app->window, "Manga Reader - Library Browser");
        // Refresh browser render after returning
        break;

      case BROWSER_ACTION_DOWNLOAD_BOOK: {
        // Download book
        char save_dir[1024];
        snprintf(save_dir, sizeof(save_dir), "%s/%s", config->download_path,
                 result.series_name);
        mkdir_p(save_dir);

        int idx = state.selected_book;
        char save_path[1024];
        snprintf(save_path, sizeof(save_path), "%s/%s.cbz", save_dir,
                 state.books_list[idx].name);

        state.is_downloading = 1;
        snprintf(state.download_status, sizeof(state.download_status),
                 "Downloading %s...", state.books_list[idx].name);
        state.download_progress = 0.0;

        // Render downloading state
        browser_render(&state, app);

        // Blocking download (simple approach)
        if (komga_download_book(&client, result.book_id, save_path) == 0) {
          state.download_progress = 1.0;
          snprintf(state.download_status, sizeof(state.download_status),
                   "Download complete: %s", state.books_list[idx].name);
        } else {
          snprintf(state.download_status, sizeof(state.download_status),
                   "Download failed!");
        }

        // Show result briefly
        browser_render(&state, app);
        SDL_Delay(1500);
        state.is_downloading = 0;
        break;
      }

      case BROWSER_ACTION_QUIT:
        running = 0;
        break;

      case BROWSER_ACTION_NONE:
        break;
      }
    }

    if (running)
      browser_render(&state, app);

    SDL_Delay(16); // ~60fps
  }

  browser_cleanup(&state, app->renderer);
  komga_cleanup(&client);
}

// ==========================================================
// MAIN — entry point dispatcher
// ==========================================================

int main(int argc, char *argv[]) {
  if (init_bookmarks_db() != 0)
    return 1;

  AppConfig config;
  config_set_defaults(&config);
  config_load(&config); // OK if it fails

  // Check for --book <id> flag
  const char *komga_book_id = NULL;
  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--book") == 0) {
      komga_book_id = argv[i + 1];
      break;
    }
  }

  AppContext app;
  if (init_sdl(&app, 800, 1000) != 0) {
    close_bookmarks_db();
    return 1;
  }

  if (komga_book_id && config_has_komga(&config)) {
    // Direct Komga book mode
    KomgaClient client;
    if (komga_init(&client, config.komga_url, config.komga_api_key,
                   config.komga_username, config.komga_password) == 0) {
      run_reader_komga(&app, &client, komga_book_id, MODE_MANGA);
      komga_cleanup(&client);
    }
  } else if (argc >= 2 && !komga_book_id) {
    // Local file mode (original behavior)
    run_reader_local(&app, argv[1]);
  } else if (config_has_komga(&config)) {
    // Browser mode
    run_browser(&app, &config);
  } else {
    printf("Usage: %s <file.cbz>\n", argv[0]);
    printf("       %s --book <komga-book-id>\n", argv[0]);
    printf("       %s                          (Komga browser, needs config)\n",
           argv[0]);
    printf("\nConfig: ~/.config/manga_reader/config.ini\n");
  }

  close_bookmarks_db();
  cleanup_sdl(&app);
  return 0;
}
