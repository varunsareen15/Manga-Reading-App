#include "bookmark_manager.h"
#include "cbz_handler.h"
#include "render_engine.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// State Globals
ViewMode view_mode = VIEW_SINGLE;
int show_help = 0;

void refresh_page(MangaBook *book, AppContext *app) {
  // 1. Load Primary Page (Current Index)
  size_t size;
  char *data = get_image_data(book, &size);
  load_texture_to_slot(app, data, size, 0); // Slot 0
  if (data)
    free(data);

  // 2. Load Secondary Page (If Double Mode)
  // Rules:
  // - If VIEW_SINGLE: Don't load.
  // - If VIEW_DOUBLE_COVER and index is 0: Don't load (Cover is single).
  // - Else: Load index + 1.
  int load_second = 0;

  if (view_mode == VIEW_DOUBLE) {
    load_second = 1;
  } else if (view_mode == VIEW_DOUBLE_COVER) {
    if (book->current_index > 0)
      load_second = 1;
  }

  if (load_second && (book->current_index + 1 < book->count)) {
    // HACK: Temporarily increment index to fetch next image
    book->current_index++;
    data = get_image_data(book, &size);
    load_texture_to_slot(app, data, size, 1); // Slot 1
    if (data)
      free(data);
    book->current_index--; // Restore index
  } else {
    clear_slot(app, 1);
  }

  // Title Update
  char title[256];
  snprintf(title, sizeof(title), "Reader - Page %d / %d",
           book->current_index + 1, book->count);
  SDL_SetWindowTitle(app->window, title);
}

// ... (Toggle Fullscreen Helper - Same as before) ...
void toggle_fullscreen(AppContext *app) {
  Uint32 flags = SDL_GetWindowFlags(app->window);
  SDL_SetWindowFullscreen(app->window, (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
                                           ? 0
                                           : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

// ... (Detect Mode Helper - Same as before) ...
ReadMode detect_mode(const char *path) {
  char lower[1024];
  strncpy(lower, path, 1023);
  for (int i = 0; lower[i]; i++)
    lower[i] = tolower(lower[i]);
  if (strstr(lower, "/comic/") || strstr(lower, "\\comic\\"))
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
  book.mode = detect_mode(argv[1]);

  // Bookmark
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
  int input_mode = 0;
  char input_buf[10] = "";

  while (running) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = 0;

      // --- HELP OVERLAY TRAP ---
      else if (show_help) {
        if (e.type == SDL_KEYDOWN) {
          if (e.key.keysym.sym == SDLK_h || e.key.keysym.sym == SDLK_ESCAPE) {
            show_help = 0; // Close Help
          }
        }
      }
      // --- INPUT MODE ---
      else if (input_mode) {
        if (e.type == SDL_TEXTINPUT) {
          if (isdigit(e.text.text[0]) && strlen(input_buf) < 5)
            strcat(input_buf, e.text.text);
        } else if (e.type == SDL_KEYDOWN) {
          if (e.key.keysym.sym == SDLK_RETURN) {
            int p = atoi(input_buf);
            if (p > 0 && p <= book.count) {
              book.current_index = p - 1;
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
      }
      // --- NORMAL NAVIGATION ---
      else if (e.type == SDL_KEYDOWN) {
        int changed = 0;
        int shift = SDL_GetModState() & KMOD_SHIFT;
        int step =
            (view_mode == VIEW_SINGLE) ? 1 : 2; // Jump 2 pages in double mode

        // Handling Double+Cover alignment
        // If we are in VIEW_DOUBLE_COVER, page 0 is single.
        // Page 1 is paired with 2. So pairs start at odd numbers.

        switch (e.key.keysym.sym) {
        // Navigation
        case SDLK_LEFT:
          if (book.mode == MODE_MANGA) { // Next
            if (shift)
              book.current_index += 10;
            else
              book.current_index += step;
            if (book.current_index >= book.count)
              book.current_index = book.count - 1;
          } else { // Prev
            if (shift)
              book.current_index -= 10;
            else
              book.current_index -= step;
            if (book.current_index < 0)
              book.current_index = 0;
          }
          changed = 1;
          break;

        case SDLK_RIGHT:
          if (book.mode == MODE_MANGA) { // Prev
            if (shift)
              book.current_index -= 10;
            else
              book.current_index -= step;
            if (book.current_index < 0)
              book.current_index = 0;
          } else { // Next
            if (shift)
              book.current_index += 10;
            else
              book.current_index += step;
            if (book.current_index >= book.count)
              book.current_index = book.count - 1;
          }
          changed = 1;
          break;

        case SDLK_b:
          book.current_index = 0;
          changed = 1;
          break;
        case SDLK_e:
          book.current_index = book.count - 1;
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

        case SDLK_s:
          view_mode = VIEW_SINGLE;
          printf("Mode: Single Page\n");
          changed = 1;
          break;

        case SDLK_d:
          if (shift) {
            // Toggle Double (Cover Offset)
            view_mode = (view_mode == VIEW_DOUBLE_COVER) ? VIEW_SINGLE
                                                         : VIEW_DOUBLE_COVER;
            printf("Mode: Double (Cover Offset)\n");
          } else {
            // Toggle Standard Double
            view_mode = (view_mode == VIEW_DOUBLE) ? VIEW_SINGLE : VIEW_DOUBLE;
            printf("Mode: Double (Standard)\n");
          }
          changed = 1;
          break;

        case SDLK_ESCAPE:
          if (SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
            SDL_SetWindowFullscreen(app.window, 0);
          else
            running = 0;
          break;
        }

        // Alignment Correction for Double+Cover Mode
        // If we are on an even page (e.g. 2) but pairs are (1,2), we should
        // shift to 1.
        if (view_mode == VIEW_DOUBLE_COVER && book.current_index > 0 &&
            book.current_index % 2 == 0) {
          book.current_index--;
        }

        if (changed)
          refresh_page(&book, &app);
      }
    }

    snprintf(overlay, 32, "%d / %d", book.current_index + 1, book.count);

    // Render Call
    // Map CBZ Mode to Render Direction
    PageDir p_dir = (book.mode == MODE_MANGA) ? DIR_MANGA : DIR_COMIC;

    render_frame(&app, overlay, input_mode ? input_buf : NULL, view_mode, p_dir,
                 show_help);
  }

  save_bookmark(argv[1], book.current_index);
  close_bookmarks_db();
  close_cbz(&book);
  cleanup_sdl(&app);
  return 0;
}
