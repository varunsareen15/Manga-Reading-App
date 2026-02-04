#ifndef RENDER_ENGINE_H
#define RENDER_ENGINE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef enum {
  VIEW_SINGLE,
  VIEW_DOUBLE,
  VIEW_DOUBLE_COVER // Double page, but Page 0 is separate (Shift+D)
} ViewMode;

typedef enum {
  DIR_MANGA, // Right-to-Left (Page 2 | Page 1)
  DIR_COMIC  // Left-to-Right (Page 1 | Page 2)
} PageDir;

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *tex_primary;   // The "Current" page
  SDL_Texture *tex_secondary; // The "Next" page (for double view)
  TTF_Font *font;
} AppContext;

int init_sdl(AppContext *ctx, int width, int height);
void cleanup_sdl(AppContext *ctx);
void load_texture_to_slot(AppContext *ctx, char *buffer, size_t size, int slot);
void clear_slot(AppContext *ctx, int slot);
void render_frame(AppContext *ctx, const char *overlay_text,
                  const char *input_text, ViewMode mode, PageDir dir,
                  int show_help);

#endif
