#ifndef RENDER_ENGINE_H
#define RENDER_ENGINE_H

#include <SDL2/SDL.h>

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *current_texture;
} AppContext;

// Initialize SDL and Window
// Returns 0 on success, -1 on failure
int init_sdl(AppContext *ctx, int width, int height);

// Clean up SDL
void cleanup_sdl(AppContext *ctx);

// Create a texture from raw memory buffer
void load_texture_from_memory(AppContext *ctx, char *buffer, size_t size);

// Clear screen, calculate aspect ratio, draw texture, flip buffer
void render_frame(AppContext *ctx);

#endif
