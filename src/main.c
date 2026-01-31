#include "bookmark_manager.h" 
#include "cbz_handler.h"
#include "render_engine.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void refresh_page(MangaBook *book, AppContext *app) {
  size_t size;
  char *data = get_image_data(book, &size);
  if (data) {
    load_texture_from_memory(app, data, size);
    free(data);
    char title[256];
    snprintf(title, sizeof(title), "Reader - Page %d / %d",
             book->current_index + 1, book->count);
    SDL_SetWindowTitle(app->window, title);
  }
}

void toggle_fullscreen(AppContext *app) {
  Uint32 flags = SDL_GetWindowFlags(app->window);
  if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
    SDL_SetWindowFullscreen(app->window, 0);
  } else {
    SDL_SetWindowFullscreen(app->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <file.cbz>\n", argv[0]);
    return 1;
  }

  MangaBook book;
  if (open_cbz(argv[1], &book) != 0) {
    printf("Failed to open CBZ.\n");
    return 1;
  }

  // --- NEW: Load Bookmark ---
  int saved_page = load_bookmark(argv[1]);
  if (saved_page > 0 && saved_page < book.count) {
    book.current_index = saved_page;
    printf("Resuming %s at page %d\n", argv[1], saved_page + 1);
  }

  AppContext app;
  if (init_sdl(&app, 800, 1000) != 0) {
    printf("Failed to init SDL.\n");
    close_cbz(&book);
    return 1;
  }

  refresh_page(&book, &app);

  int running = 1;
  SDL_Event event;
  char overlay_text[32];

  // Input Mode State
  int input_mode = 0;
  char input_buffer[10] = "";

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = 0;
      } else if (input_mode) {
        if (event.type == SDL_TEXTINPUT) {
          if (isdigit(event.text.text[0]) && strlen(input_buffer) < 5) {
            strcat(input_buffer, event.text.text);
          }
        } else if (event.type == SDL_KEYDOWN) {
          switch (event.key.keysym.sym) {
          case SDLK_BACKSPACE: {
            int len = strlen(input_buffer);
            if (len > 0)
              input_buffer[len - 1] = '\0';
            break;
          }
          case SDLK_RETURN:
          case SDLK_KP_ENTER: {
            int page = atoi(input_buffer);
            if (page > 0 && page <= book.count) {
              book.current_index = page - 1;
              refresh_page(&book, &app);
            }
            input_mode = 0;
            SDL_StopTextInput();
            break;
          }
          case SDLK_ESCAPE:
            input_mode = 0;
            SDL_StopTextInput();
            break;
          }
        }
      } else if (event.type == SDL_KEYDOWN) {
        int changed = 0;
        int shift = SDL_GetModState() & KMOD_SHIFT;

        switch (event.key.keysym.sym) {
        case SDLK_LEFT:
          if (shift) {
            book.current_index += 10;
            if (book.current_index >= book.count)
              book.current_index = book.count - 1;
          } else {
            next_page(&book);
          }
          changed = 1;
          break;

        case SDLK_RIGHT:
          if (shift) {
            book.current_index -= 10;
            if (book.current_index < 0)
              book.current_index = 0;
          } else {
            prev_page(&book);
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

        case SDLK_g:
          input_mode = 1;
          input_buffer[0] = '\0';
          SDL_StartTextInput();
          break;

        case SDLK_f:
          toggle_fullscreen(&app);
          break;

        case SDLK_ESCAPE:
          if (SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
            SDL_SetWindowFullscreen(app.window, 0);
          } else {
            running = 0;
          }
          break;
        }

        if (changed)
          refresh_page(&book, &app);
      }
    }

    snprintf(overlay_text, sizeof(overlay_text), "%d / %d",
             book.current_index + 1, book.count);
    render_frame(&app, overlay_text, input_mode ? input_buffer : NULL);
  }

  // --- NEW: Save Bookmark on Exit ---
  save_bookmark(argv[1], book.current_index);

  close_cbz(&book);
  cleanup_sdl(&app);

  return 0;
}
