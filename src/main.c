#include "cbz_handler.h"
#include "render_engine.h"
#include <stdio.h>
#include <stdlib.h>

// --- Helper Function ---
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

// --- Helper: Toggle Fullscreen ---
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

  // --- 1. Open CBZ ---
  MangaBook book;
  int err = open_cbz(argv[1], &book);
  if (err != 0) {
    printf("Failed to open CBZ file. Error code: %d\n", err);
    return 1;
  }
  printf("Opened book with %d pages.\n", book.count);

  // --- 2. Init Graphics ---
  AppContext app;
  // Window size (800x1000) is just a fallback now, since we auto-maximize
  if (init_sdl(&app, 800, 1000) != 0) {
    printf("Failed to init SDL.\n");
    close_cbz(&book);
    return 1;
  }

  refresh_page(&book, &app);

  // --- 3. Main Loop ---
  int running = 1;
  SDL_Event event;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = 0;
      } else if (event.type == SDL_KEYDOWN) {
        int changed = 0;
        switch (event.key.keysym.sym) {
        case SDLK_LEFT:
          next_page(&book);
          changed = 1;
          break;
        case SDLK_RIGHT:
          prev_page(&book);
          changed = 1;
          break;
        case SDLK_f: // Toggle Fullscreen
          toggle_fullscreen(&app);
          break;
        case SDLK_ESCAPE:
          // Exit fullscreen if active, otherwise quit
          if (SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
            SDL_SetWindowFullscreen(app.window, 0);
          } else {
            running = 0;
          }
          break;
        }

        if (changed) {
          refresh_page(&book, &app);
        }
      }
    }
    render_frame(&app);
  }

  // --- 4. Cleanup ---
  close_cbz(&book);
  cleanup_sdl(&app);

  return 0;
}
