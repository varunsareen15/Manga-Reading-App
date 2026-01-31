#include "render_engine.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>

int init_sdl(AppContext *ctx, int width, int height) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    return -1;

  // Use Anisotropic scaling for best quality
  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2")) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  }

  int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
  if (!(IMG_Init(imgFlags) & imgFlags))
    return -1;

  if (TTF_Init() == -1)
    return -1;

  // Load font (size 24)
  ctx->font = TTF_OpenFont("font.ttf", 24);
  if (!ctx->font) {
    printf("Warning: font.ttf not found. Text will not display.\n");
  }

  ctx->window =
      SDL_CreateWindow("Modular Manga Reader", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, width, height,
                       SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
                           SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);

  if (!ctx->window)
    return -1;

  ctx->renderer = SDL_CreateRenderer(
      ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  ctx->current_texture = NULL;
  return 0;
}

void cleanup_sdl(AppContext *ctx) {
  if (ctx->font)
    TTF_CloseFont(ctx->font);
  if (ctx->current_texture)
    SDL_DestroyTexture(ctx->current_texture);
  if (ctx->renderer)
    SDL_DestroyRenderer(ctx->renderer);
  if (ctx->window)
    SDL_DestroyWindow(ctx->window);
  TTF_Quit();
  IMG_Quit();
  SDL_Quit();
}

void load_texture_from_memory(AppContext *ctx, char *buffer, size_t size) {
  if (ctx->current_texture) {
    SDL_DestroyTexture(ctx->current_texture);
    ctx->current_texture = NULL;
  }
  if (!buffer)
    return;

  SDL_RWops *rw = SDL_RWFromMem(buffer, size);
  SDL_Surface *surface = IMG_Load_RW(rw, 1);

  if (surface) {
    ctx->current_texture = SDL_CreateTextureFromSurface(ctx->renderer, surface);
    SDL_FreeSurface(surface);
  }
}

// Helper: Draw text overlay (Bottom Right)
void draw_overlay(AppContext *ctx, const char *text) {
  if (!ctx->font || !text)
    return;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Surface *surface = TTF_RenderText_Blended(ctx->font, text, white);
  if (!surface)
    return;
  SDL_Texture *texture = SDL_CreateTextureFromSurface(ctx->renderer, surface);

  int win_w, win_h;
  SDL_GetRendererOutputSize(ctx->renderer, &win_w, &win_h);

  SDL_Rect dest = {win_w - surface->w - 20, win_h - surface->h - 10, surface->w,
                   surface->h};

  // Draw background box
  SDL_Rect bg = {dest.x - 5, dest.y - 5, dest.w + 10, dest.h + 10};
  SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 150);
  SDL_RenderFillRect(ctx->renderer, &bg);

  SDL_RenderCopy(ctx->renderer, texture, NULL, &dest);
  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

// Helper: Draw Input Box (Center Screen)
void draw_input_box(AppContext *ctx, const char *input) {
  if (!ctx->font)
    return;

  char display_str[64];
  snprintf(display_str, sizeof(display_str), "Go to Page: %s_",
           input); // Add cursor

  SDL_Color white = {255, 255, 255, 255};
  SDL_Surface *surface = TTF_RenderText_Blended(ctx->font, display_str, white);
  if (!surface)
    return;
  SDL_Texture *texture = SDL_CreateTextureFromSurface(ctx->renderer, surface);

  int win_w, win_h;
  SDL_GetRendererOutputSize(ctx->renderer, &win_w, &win_h);

  // Center the box
  SDL_Rect dest;
  dest.w = surface->w;
  dest.h = surface->h;
  dest.x = (win_w - dest.w) / 2;
  dest.y = (win_h - dest.h) / 2;

  // Draw large background box
  SDL_Rect bg = {dest.x - 20, dest.y - 20, dest.w + 40, dest.h + 40};
  SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0,
                         200); // Darker, less transparent
  SDL_RenderFillRect(ctx->renderer, &bg);

  // Draw Blue Border
  SDL_SetRenderDrawColor(ctx->renderer, 100, 149, 237, 255);
  SDL_RenderDrawRect(ctx->renderer, &bg);

  SDL_RenderCopy(ctx->renderer, texture, NULL, &dest);
  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

void render_frame(AppContext *ctx, const char *overlay_text,
                  const char *input_text) {
  SDL_SetRenderDrawColor(ctx->renderer, 30, 30, 30, 255);
  SDL_RenderClear(ctx->renderer);

  if (ctx->current_texture) {
    int w_int, h_int;
    SDL_QueryTexture(ctx->current_texture, NULL, NULL, &w_int, &h_int);
    float w = (float)w_int;
    float h = (float)h_int;

    int renderer_w, renderer_h;
    SDL_GetRendererOutputSize(ctx->renderer, &renderer_w, &renderer_h);
    float win_w = (float)renderer_w;
    float win_h = (float)renderer_h;

    float scale = win_h / h;
    if (w * scale > win_w)
      scale = win_w / w;

    SDL_FRect dest;
    dest.w = w * scale;
    dest.h = h * scale;
    dest.x = (win_w - dest.w) / 2.0f;
    dest.y = (win_h - dest.h) / 2.0f;

    SDL_RenderCopyF(ctx->renderer, ctx->current_texture, NULL, &dest);
  }

  // Draw "Page X/Y"
  if (overlay_text)
    draw_overlay(ctx, overlay_text);

  // Draw "Go to Page: ..." if active
  if (input_text)
    draw_input_box(ctx, input_text);

  SDL_RenderPresent(ctx->renderer);
}
