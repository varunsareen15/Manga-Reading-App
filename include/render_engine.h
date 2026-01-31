#ifndef RENDER_ENGINE_H
#define RENDER_ENGINE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *current_texture;
  TTF_Font *font;
} AppContext;

int init_sdl(AppContext *ctx, int width, int height);
void cleanup_sdl(AppContext *ctx);
void load_texture_from_memory(AppContext *ctx, char *buffer, size_t size);

// Updated: Now accepts 'input_text' (can be NULL)
void render_frame(AppContext *ctx, const char *overlay_text,
                  const char *input_text);

#endif
