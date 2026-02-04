#ifndef RENDER_ENGINE_H
#define RENDER_ENGINE_H

#include "cbz_handler.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef enum {
  VIEW_SINGLE,       // Standard Fit Screen
  VIEW_DOUBLE,       // Side-by-Side
  VIEW_DOUBLE_COVER, // Offset
  VIEW_MANHWA        // Continuous Vertical Scroll
} ViewMode;

// Sub-mode for Manhwa: How is it scaled?
typedef enum {
  SCALE_FIT_WIDTH, // "d" key (Zoomed)
  SCALE_FIT_HEIGHT // "s" key (Fit Screen)
} ManhwaScale;

typedef enum { DIR_MANGA, DIR_COMIC } PageDir;

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;

  // Sliding Window Buffer
  SDL_Texture *tex_prev; // Index - 1
  SDL_Texture *tex_curr; // Index (Current)
  SDL_Texture *tex_next; // Index + 1

  TTF_Font *font;
} AppContext;

int init_sdl(AppContext *ctx, int width, int height);
void cleanup_sdl(AppContext *ctx);

// Slot: -1=Prev, 0=Curr, 1=Next
void load_texture_to_slot(AppContext *ctx, char *buffer, size_t size, int slot);
void clear_slots(AppContext *ctx);

// Helper to get the rendered height of a specific slot
int get_scaled_height(AppContext *ctx, int slot, ManhwaScale scale_mode);

void render_frame(AppContext *ctx, const char *overlay_text,
                  const char *input_text, ViewMode mode, ManhwaScale scale_mode,
                  PageDir dir, int show_help, int scroll_y, ReadMode book_mode);

#endif
