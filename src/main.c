#include "bookmark_manager.h"
#include "cbz_handler.h"
#include "render_engine.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Global State ---
ViewMode view_mode = VIEW_SINGLE;
ManhwaScale manhwa_scale = SCALE_FIT_HEIGHT; // Default Manhwa scaling ('s')
int show_help = 0;
int scroll_y = 0; // Vertical scroll offset (pixels)

// Helper: Reset scroll when jumping to a fresh page (not scrolling)
void reset_view() { scroll_y = 0; }

// Helper: Load images into the Sliding Window (Prev, Curr, Next)
void refresh_page(MangaBook *book, AppContext *app) {
  size_t size;
  char *data;

  // 1. LOAD CURRENT PAGE (Slot 0)
  data = get_image_data(book, &size);
  load_texture_to_slot(app, data, size, 0);
  if (data)
    free(data);

  // 2. LOAD NEXT PAGE (Slot 1)
  // Always needed for Continuous Manhwa, Standard Double, or Double Cover
  int load_next = (view_mode == VIEW_MANHWA || view_mode == VIEW_DOUBLE);
  if (view_mode == VIEW_DOUBLE_COVER && book->current_index > 0)
    load_next = 1;

  if (load_next && (book->current_index + 1 < book->count)) {
    book->current_index++;
    data = get_image_data(book, &size);
    load_texture_to_slot(app, data, size, 1);
    if (data)
      free(data);
    book->current_index--; // Restore index
  } else {
    load_texture_to_slot(app, NULL, 0, 1); // Clear slot
  }

  // 3. LOAD PREVIOUS PAGE (Slot -1)
  // Only needed for Manhwa Continuous Scrolling (to scroll UP seamlessly)
  if (view_mode == VIEW_MANHWA && book->current_index > 0) {
    book->current_index--;
    data = get_image_data(book, &size);
    load_texture_to_slot(app, data, size, -1);
    if (data)
      free(data);
    book->current_index++; // Restore index
  } else {
    load_texture_to_slot(app, NULL, 0, -1); // Clear slot
  }

  // Update Window Title
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

  // --- 1. Initialize Systems ---
  if (init_bookmarks_db() != 0)
    return 1;

  MangaBook book;
  if (open_cbz(argv[1], &book) != 0)
    return 1;
  book.mode = detect_mode(argv[1]);

  // Set Defaults based on Mode
  if (book.mode == MODE_MANHWA) {
    view_mode = VIEW_MANHWA;
    manhwa_scale = SCALE_FIT_HEIGHT; // Default to 's' (Fit Screen)
  }

  // Load Bookmark
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

  // --- Main Loop ---
  while (running) {

    // --- Continuous Scroll Logic (Threshold Check) ---
    if (view_mode == VIEW_MANHWA) {
      int curr_h = get_scaled_height(&app, 0, manhwa_scale);

      // 1. SCROLLING DOWN: Did we pass the bottom of the current page?
      if (curr_h > 0 && scroll_y >= curr_h) {
        if (book.current_index < book.count - 1) {
          book.current_index++;
          scroll_y -=
              curr_h; // Subtract height to snap to top of next page (seamless)
          refresh_page(&book, &app);
        }
      }
      // 2. SCROLLING UP: Did we pass the top of the current page?
      else if (scroll_y < 0) {
        if (book.current_index > 0) {
          // We need dimensions of the PREVIOUS page to set offset correctly
          // We haven't loaded it as "curr" yet, so we ask render engine for
          // slot -1 size
          int prev_h = get_scaled_height(&app, -1, manhwa_scale);

          book.current_index--;
          scroll_y += prev_h; // Snap to bottom of previous page
          refresh_page(&book, &app);
        } else {
          scroll_y = 0; // Hard stop at the very beginning
        }
      }
    }

    // --- Event Handling ---
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = 0;

      // Mouse Wheel (Only active if not typing or in help)
      else if (e.type == SDL_MOUSEWHEEL && view_mode == VIEW_MANHWA &&
               !input_mode && !show_help) {
        // Wheel Down (y < 0) -> scroll_y increases (Content moves up)
        // Wheel Up (y > 0) -> scroll_y decreases (Content moves down)
        scroll_y -= e.wheel.y * SCROLL_STEP;
      }

      else if (show_help) {
        if (e.type == SDL_KEYDOWN &&
            (e.key.keysym.sym == SDLK_h || e.key.keysym.sym == SDLK_ESCAPE))
          show_help = 0;
      }

      else if (input_mode) {
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
      }

      else if (e.type == SDL_KEYDOWN) {
        int changed = 0;
        int shift = SDL_GetModState() & KMOD_SHIFT;
        int step =
            (view_mode == VIEW_SINGLE || view_mode == VIEW_MANHWA) ? 1 : 2;

        switch (e.key.keysym.sym) {
        // --- MANHWA SCROLL KEYS ---
        case SDLK_DOWN:
          if (view_mode == VIEW_MANHWA)
            scroll_y += SCROLL_STEP;
          break;
        case SDLK_UP:
          if (view_mode == VIEW_MANHWA)
            scroll_y -= SCROLL_STEP;
          break;

        // --- PAGE NAVIGATION ---
        case SDLK_LEFT:
          if (book.mode == MODE_MANHWA) {
            // In Manhwa, Left is Previous Page Jump
            prev_page(&book);
            reset_view();
            changed = 1;
          } else if (book.mode == MODE_COMIC || book.mode == MODE_MANHUA) {
            if (shift)
              book.current_index -= 10;
            else
              book.current_index -= step;
            if (book.current_index < 0)
              book.current_index = 0;
            changed = 1;
          } else {
            // Manga (RTL): Left is Next
            if (shift)
              book.current_index += 10;
            else
              book.current_index += step;
            if (book.current_index >= book.count)
              book.current_index = book.count - 1;
            changed = 1;
          }
          break;

        case SDLK_RIGHT:
          if (book.mode == MODE_MANHWA) {
            // In Manhwa, Right is Next Page Jump
            next_page(&book);
            reset_view();
            changed = 1;
          } else if (book.mode == MODE_COMIC || book.mode == MODE_MANHUA) {
            if (shift)
              book.current_index += 10;
            else
              book.current_index += step;
            if (book.current_index >= book.count)
              book.current_index = book.count - 1;
            changed = 1;
          } else {
            // Manga (RTL): Right is Prev
            if (shift)
              book.current_index -= 10;
            else
              book.current_index -= step;
            if (book.current_index < 0)
              book.current_index = 0;
            changed = 1;
          }
          break;

        // --- VIEW MODES ---
        case SDLK_s:
          if (book.mode == MODE_MANHWA) {
            // Manhwa: 's' = Fit Screen Height
            view_mode = VIEW_MANHWA;
            manhwa_scale = SCALE_FIT_HEIGHT;
            reset_view();
          } else {
            // Others: 's' = Single Page
            view_mode = VIEW_SINGLE;
          }
          changed = 1;
          break;

        case SDLK_d:
          if (book.mode == MODE_MANHWA) {
            if (!shift) {
              // Manhwa: 'd' = Fit Width (Zoomed)
              view_mode = VIEW_MANHWA;
              manhwa_scale = SCALE_FIT_WIDTH;
              reset_view(); // Reset scroll logic for new scale
            }
          } else {
            // Others: 'd' = Toggle Double
            if (shift)
              view_mode = (view_mode == VIEW_DOUBLE_COVER) ? VIEW_SINGLE
                                                           : VIEW_DOUBLE_COVER;
            else
              view_mode =
                  (view_mode == VIEW_DOUBLE) ? VIEW_SINGLE : VIEW_DOUBLE;
          }
          changed = 1;
          break;

        // --- UTILITIES ---
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

        // Alignment for Double View
        if ((view_mode == VIEW_DOUBLE_COVER) && book.current_index > 0 &&
            book.current_index % 2 == 0) {
          book.current_index--;
        }
        if (changed)
          refresh_page(&book, &app);
      }
    }

    // --- Render Frame ---
    snprintf(overlay, 32, "%d / %d", book.current_index + 1, book.count);
    PageDir p_dir = (book.mode == MODE_MANGA) ? DIR_MANGA : DIR_COMIC;

    render_frame(&app, overlay, input_mode ? input_buf : NULL, view_mode,
                 manhwa_scale, p_dir, show_help, scroll_y, book.mode);
  }

  // --- Cleanup ---
  save_bookmark(argv[1], book.current_index);
  close_bookmarks_db();
  close_cbz(&book);
  cleanup_sdl(&app);
  return 0;
}
