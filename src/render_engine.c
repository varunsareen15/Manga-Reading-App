#include "render_engine.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>

int init_sdl(AppContext *ctx, int width, int height) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    return -1;

  // "2" = Anisotropic (Best), "1" = Linear (Good)
  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2")) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  }

  int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
  if (!(IMG_Init(imgFlags) & imgFlags))
    return -1;

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
  if (ctx->current_texture)
    SDL_DestroyTexture(ctx->current_texture);
  if (ctx->renderer)
    SDL_DestroyRenderer(ctx->renderer);
  if (ctx->window)
    SDL_DestroyWindow(ctx->window);
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
  } else {
    printf("IMG_Load Error: %s\n", IMG_GetError());
  }
}

void render_frame(AppContext *ctx) {
  SDL_SetRenderDrawColor(ctx->renderer, 30, 30, 30, 255);
  SDL_RenderClear(ctx->renderer);

  if (ctx->current_texture) {
    int w_int, h_int;
    SDL_QueryTexture(ctx->current_texture, NULL, NULL, &w_int, &h_int);

    float w = (float)w_int;
    float h = (float)h_int;

    // Use Output Size (Pixels) instead of Window Size (Points)
    int renderer_w, renderer_h;
    SDL_GetRendererOutputSize(ctx->renderer, &renderer_w, &renderer_h);

    float win_w = (float)renderer_w;
    float win_h = (float)renderer_h;

    // 1. Try fitting to height
    float scale = win_h / h;

    // 2. If fitting to height makes it too wide, fit to width instead
    if (w * scale > win_w) {
      scale = win_w / w;
    }

    SDL_FRect dest;
    dest.w = w * scale;
    dest.h = h * scale;
    dest.x = (win_w - dest.w) / 2.0f;
    dest.y = (win_h - dest.h) / 2.0f;

    SDL_RenderCopyF(ctx->renderer, ctx->current_texture, NULL, &dest);
  }
  SDL_RenderPresent(ctx->renderer);
}
