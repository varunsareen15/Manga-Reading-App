#include "render_engine.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>

int init_sdl(AppContext *ctx, int width, int height) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    return -1;
  // High Quality Scaling
  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2")) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  }
  int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
  if (!(IMG_Init(imgFlags) & imgFlags))
    return -1;

  if (TTF_Init() == -1)
    return -1;
  ctx->font = TTF_OpenFont("font.ttf", 24);
  if (!ctx->font)
    printf("Warning: font.ttf not found.\n");

  ctx->window =
      SDL_CreateWindow("Modular Manga Reader", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, width, height,
                       SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
                           SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);

  if (!ctx->window)
    return -1;

  ctx->renderer = SDL_CreateRenderer(
      ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  ctx->tex_primary = NULL;
  ctx->tex_secondary = NULL;
  return 0;
}

void cleanup_sdl(AppContext *ctx) {
  if (ctx->font)
    TTF_CloseFont(ctx->font);
  if (ctx->tex_primary)
    SDL_DestroyTexture(ctx->tex_primary);
  if (ctx->tex_secondary)
    SDL_DestroyTexture(ctx->tex_secondary);
  if (ctx->renderer)
    SDL_DestroyRenderer(ctx->renderer);
  if (ctx->window)
    SDL_DestroyWindow(ctx->window);
  TTF_Quit();
  IMG_Quit();
  SDL_Quit();
}

void clear_slot(AppContext *ctx, int slot) {
  if (slot == 0 && ctx->tex_primary) {
    SDL_DestroyTexture(ctx->tex_primary);
    ctx->tex_primary = NULL;
  } else if (slot == 1 && ctx->tex_secondary) {
    SDL_DestroyTexture(ctx->tex_secondary);
    ctx->tex_secondary = NULL;
  }
}

void load_texture_to_slot(AppContext *ctx, char *buffer, size_t size,
                          int slot) {
  clear_slot(ctx, slot);
  if (!buffer)
    return;

  SDL_RWops *rw = SDL_RWFromMem(buffer, size);
  SDL_Surface *surface = IMG_Load_RW(rw, 1);

  if (surface) {
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ctx->renderer, surface);
    if (slot == 0)
      ctx->tex_primary = tex;
    else
      ctx->tex_secondary = tex;
    SDL_FreeSurface(surface);
  }
}

// Helper: Draw Help Menu
void render_help_menu(AppContext *ctx) {
  if (!ctx->font)
    return;

  const char *lines[] = {"--- HELP MENU ---",
                         "Arrows : Navigation",
                         "Shift+Arrows : Skip 10 Pages",
                         "S : Toggle Single Page",
                         "D : Toggle Double Page",
                         "Shift+D : Toggle Double (Skip Cover)",
                         "G : Go to Page",
                         "B / E : Start / End",
                         "F : Fullscreen",
                         "H : Close Help",
                         "ESC : Quit"};
  int count = 11;

  SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 220);
  SDL_RenderFillRect(ctx->renderer, NULL); // Dim background

  int y = 100;
  SDL_Color white = {255, 255, 255, 255};
  int win_w, win_h;
  SDL_GetRendererOutputSize(ctx->renderer, &win_w, &win_h);

  for (int i = 0; i < count; i++) {
    SDL_Surface *surf = TTF_RenderText_Blended(ctx->font, lines[i], white);
    if (surf) {
      SDL_Texture *tex = SDL_CreateTextureFromSurface(ctx->renderer, surf);
      SDL_Rect dest = {(win_w - surf->w) / 2, y, surf->w, surf->h};
      SDL_RenderCopy(ctx->renderer, tex, NULL, &dest);
      y += surf->h + 10;
      SDL_FreeSurface(surf);
      SDL_DestroyTexture(tex);
    }
  }
}

// Helper: Draw overlays (Page count, etc.)
void draw_ui(AppContext *ctx, const char *overlay_text,
             const char *input_text) {
  // Re-use logic from previous version (omitted for brevity, assume similar to
  // before)
  // ... (Paste your draw_overlay and draw_input_box helpers here or keep them
  // if merging) ... For this full file example, I'll simplify the overlay
  // drawing:

  if (!ctx->font)
    return;
  SDL_Color white = {255, 255, 255, 255};
  int win_w, win_h;
  SDL_GetRendererOutputSize(ctx->renderer, &win_w, &win_h);

  // Page Counter
  if (overlay_text) {
    SDL_Surface *s = TTF_RenderText_Blended(ctx->font, overlay_text, white);
    if (s) {
      SDL_Texture *t = SDL_CreateTextureFromSurface(ctx->renderer, s);
      SDL_Rect dest = {win_w - s->w - 20, win_h - s->h - 10, s->w, s->h};
      // Box
      SDL_Rect bg = {dest.x - 5, dest.y - 5, dest.w + 10, dest.h + 10};
      SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 150);
      SDL_RenderFillRect(ctx->renderer, &bg);
      SDL_RenderCopy(ctx->renderer, t, NULL, &dest);
      SDL_FreeSurface(s);
      SDL_DestroyTexture(t);
    }
  }

  // Input Box
  if (input_text) {
    char buf[64];
    snprintf(buf, 64, "Go to: %s_", input_text);
    SDL_Surface *s = TTF_RenderText_Blended(ctx->font, buf, white);
    if (s) {
      SDL_Texture *t = SDL_CreateTextureFromSurface(ctx->renderer, s);
      SDL_Rect dest = {(win_w - s->w) / 2, (win_h - s->h) / 2, s->w, s->h};
      SDL_Rect bg = {dest.x - 20, dest.y - 20, dest.w + 40, dest.h + 40};
      SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 220);
      SDL_RenderFillRect(ctx->renderer, &bg);
      SDL_SetRenderDrawColor(ctx->renderer, 100, 149, 237, 255);
      SDL_RenderDrawRect(ctx->renderer, &bg);
      SDL_RenderCopy(ctx->renderer, t, NULL, &dest);
      SDL_FreeSurface(s);
      SDL_DestroyTexture(t);
    }
  }
}

void render_frame(AppContext *ctx, const char *overlay_text,
                  const char *input_text, ViewMode mode, PageDir dir,
                  int show_help) {
  SDL_SetRenderDrawColor(ctx->renderer, 30, 30, 30, 255);
  SDL_RenderClear(ctx->renderer);

  if (show_help) {
    render_help_menu(ctx);
    SDL_RenderPresent(ctx->renderer);
    return;
  }

  int win_w, win_h;
  SDL_GetRendererOutputSize(ctx->renderer, &win_w, &win_h);

  // LOGIC: Single Page
  if (mode == VIEW_SINGLE || !ctx->tex_secondary) {
    if (ctx->tex_primary) {
      int w, h;
      SDL_QueryTexture(ctx->tex_primary, NULL, NULL, &w, &h);
      float scale = (float)win_h / h;
      if (w * scale > win_w)
        scale = (float)win_w / w;

      SDL_FRect dest = {(win_w - w * scale) / 2, (win_h - h * scale) / 2,
                        w * scale, h * scale};
      SDL_RenderCopyF(ctx->renderer, ctx->tex_primary, NULL, &dest);
    }
  }
  // LOGIC: Double Page
  else {
    int w1, h1, w2, h2;
    SDL_QueryTexture(ctx->tex_primary, NULL, NULL, &w1, &h1);
    SDL_QueryTexture(ctx->tex_secondary, NULL, NULL, &w2, &h2);

    // Combined width logic
    // We want them same height visually.
    // Let's assume we scale both to fit height, then shrink if too wide.

    float scale = (float)win_h / (h1 > h2 ? h1 : h2); // Scale to max height

    float total_w = (w1 + w2) * scale;

    // If combined width is too big, scale down further
    if (total_w > win_w) {
      scale = scale * (win_w / total_w);
    }

    float dw1 = w1 * scale;
    float dw2 = w2 * scale;
    float start_x = (win_w - (dw1 + dw2)) / 2;
    float y1 = (win_h - h1 * scale) / 2;
    float y2 = (win_h - h2 * scale) / 2;

    SDL_FRect dest1, dest2;

    // Positioning based on Direction
    if (dir == DIR_MANGA) {
      // Manga (RTL): [Secondary(Next)] [Primary(Curr)]
      // Example: [Page 2] [Page 1]
      dest2 = (SDL_FRect){start_x, y2, dw2, h2 * scale};       // Left
      dest1 = (SDL_FRect){start_x + dw2, y1, dw1, h1 * scale}; // Right
    } else {
      // Comic (LTR): [Primary(Curr)] [Secondary(Next)]
      // Example: [Page 1] [Page 2]
      dest1 = (SDL_FRect){start_x, y1, dw1, h1 * scale};       // Left
      dest2 = (SDL_FRect){start_x + dw1, y2, dw2, h2 * scale}; // Right
    }

    SDL_RenderCopyF(ctx->renderer, ctx->tex_secondary, NULL, &dest2);
    SDL_RenderCopyF(ctx->renderer, ctx->tex_primary, NULL, &dest1);
  }

  draw_ui(ctx, overlay_text, input_text);
  SDL_RenderPresent(ctx->renderer);
}
