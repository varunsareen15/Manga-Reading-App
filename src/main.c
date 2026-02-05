#include "bookmark_manager.h"
#include "cbz_handler.h"
#include "file_utils.h"
#include "render_engine.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    // Temporarily increment to load next
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

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <file>\n", argv[0]);
    return 1;
  }

  if (init_bookmarks_db() != 0)
    return 1;

  MangaBook book;
  if (open_cbz(argv[1], &book) != 0)
    return 1;
  strncpy(current_file_path, argv[1], 1023);
  book.mode = detect_mode(argv[1]);

  if (book.mode == MODE_MANHWA) {
    view_mode = VIEW_MANHWA;
    manhwa_scale = SCALE_FIT_HEIGHT;
  }

  int saved = load_bookmark(argv[1]);
  if (saved > 0 && saved < book.count)
    book.current_index = saved;

  AppContext app;
  if (init_sdl(&app, 800, 1000) != 0) {
    close_cbz(&book);
    return 1;
  }

  refresh_page(&book, &app);

  int running = 1;
  SDL_Event e;
  char overlay[32];
  char input_buf[10] = "";
  int input_mode = 0;
  const int SCROLL_STEP = 60;

  while (running) {

    // --- Continuous Scroll Logic ---
    if (view_mode == VIEW_MANHWA && !prompt_next) {
      int curr_h = get_scaled_height(&app, 0, manhwa_scale);
      if (curr_h > 0 && scroll_y >= curr_h) {
        if (book.current_index < book.count - 1) {
          book.current_index++;
          scroll_y -= curr_h;
          refresh_page(&book, &app);
        }
      } else if (scroll_y < 0) {
        if (book.current_index > 0) {
          int prev_h = get_scaled_height(&app, -1, manhwa_scale);
          book.current_index--;
          scroll_y += prev_h;
          refresh_page(&book, &app);
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
              refresh_page(&book, &app);
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
          // CALCULATE STEP
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

          // --- LEFT ARROW ---
          case SDLK_LEFT:
            if (left_is_next) {
              // MANGA NEXT
              if (prompt_next == 1) {
                if (get_neighbor_file(current_file_path, 1, next_file_path,
                                      1024)) {
                  load_new_file(&book, &app, next_file_path);
                  refresh_page(&book, &app);
                }
              } else if (prompt_next == -1) {
                prompt_next = 0;
              } else {
                // CHECK BOUNDS USING STEP
                if (book.current_index + step >= book.count) {
                  if (get_neighbor_file(current_file_path, 1, next_file_path,
                                        1024))
                    prompt_next = 1;
                  else {
                    book.current_index = book.count - 1;
                    changed = 1;
                  } // Clamp to end
                } else {
                  book.current_index += step;
                  changed = 1;
                }
              }
            } else {
              // COMIC PREV
              if (prompt_next == -1) {
                if (get_neighbor_file(current_file_path, -1, next_file_path,
                                      1024)) {
                  load_new_file(&book, &app, next_file_path);
                  refresh_page(&book, &app);
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

          // --- RIGHT ARROW ---
          case SDLK_RIGHT:
            if (!left_is_next) {
              // COMIC NEXT
              if (prompt_next == 1) {
                if (get_neighbor_file(current_file_path, 1, next_file_path,
                                      1024)) {
                  load_new_file(&book, &app, next_file_path);
                  refresh_page(&book, &app);
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
              // MANGA PREV
              if (prompt_next == -1) {
                if (get_neighbor_file(current_file_path, -1, next_file_path,
                                      1024)) {
                  load_new_file(&book, &app, next_file_path);
                  refresh_page(&book, &app);
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
            toggle_fullscreen(&app);
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
            if (SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
              SDL_SetWindowFullscreen(app.window, 0);
            else
              running = 0;
            break;
          }

          // --- ALIGNMENT CORRECTION ---
          // 1. Double Cover Mode: Ensure we land on Odd numbers (1, 3, 5...)
          if ((view_mode == VIEW_DOUBLE_COVER) && book.current_index > 0 &&
              book.current_index % 2 == 0) {
            book.current_index--;
          }
          // 2. Standard Double Mode: Ensure we land on Even numbers (0,
          // 2, 4...)
          else if (view_mode == VIEW_DOUBLE && book.current_index % 2 != 0) {
            // If we are at 1, we want 0. If 3, we want 2.
            book.current_index--;
          }

          if (changed)
            refresh_page(&book, &app);
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

    render_frame(&app, overlay, input_mode ? input_buf : NULL, view_mode,
                 manhwa_scale, p_dir, show_help, scroll_y, book.mode,
                 popup_msg);
  }

  save_bookmark(current_file_path, book.current_index);
  close_bookmarks_db();
  close_cbz(&book);
  cleanup_sdl(&app);
  return 0;
}
