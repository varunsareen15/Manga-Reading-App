#include "render_engine.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>

// --- 1. INITIALIZATION & CLEANUP ---

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

  ctx->font = TTF_OpenFont("/home/varun/Repos/Manga-Reading-App/font.ttf", 24);
  if (!ctx->font)
    printf("Warning: /home/varun/Repos/Manga-Reading-App/font.ttf not found.\n");

  ctx->window =
      SDL_CreateWindow("Modular Manga Reader", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, width, height,
                       SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
                           SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);

  if (!ctx->window)
    return -1;

  ctx->renderer = SDL_CreateRenderer(
      ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  // Initialize Texture Slots
  ctx->tex_prev = NULL;
  ctx->tex_curr = NULL;
  ctx->tex_next = NULL;
  return 0;
}

void cleanup_sdl(AppContext *ctx) {
  if (ctx->font)
    TTF_CloseFont(ctx->font);
  if (ctx->tex_prev)
    SDL_DestroyTexture(ctx->tex_prev);
  if (ctx->tex_curr)
    SDL_DestroyTexture(ctx->tex_curr);
  if (ctx->tex_next)
    SDL_DestroyTexture(ctx->tex_next);
  if (ctx->renderer)
    SDL_DestroyRenderer(ctx->renderer);
  if (ctx->window)
    SDL_DestroyWindow(ctx->window);
  TTF_Quit();
  IMG_Quit();
  SDL_Quit();
}

void clear_slots(AppContext *ctx) {
  if (ctx->tex_prev) {
    SDL_DestroyTexture(ctx->tex_prev);
    ctx->tex_prev = NULL;
  }
  if (ctx->tex_curr) {
    SDL_DestroyTexture(ctx->tex_curr);
    ctx->tex_curr = NULL;
  }
  if (ctx->tex_next) {
    SDL_DestroyTexture(ctx->tex_next);
    ctx->tex_next = NULL;
  }
}

void load_texture_to_slot(AppContext *ctx, char *buffer, size_t size,
                          int slot) {
  // Determine which pointer to use: -1=Prev, 0=Curr, 1=Next
  SDL_Texture **target = (slot == -1)  ? &ctx->tex_prev
                         : (slot == 1) ? &ctx->tex_next
                                       : &ctx->tex_curr;

  // Clear existing texture in this slot
  if (*target) {
    SDL_DestroyTexture(*target);
    *target = NULL;
  }

  if (!buffer)
    return;

  SDL_RWops *rw = SDL_RWFromMem(buffer, size);
  SDL_Surface *surface = IMG_Load_RW(rw, 1);

  if (surface) {
    *target = SDL_CreateTextureFromSurface(ctx->renderer, surface);
    SDL_FreeSurface(surface);
  }
}

SDL_Texture *render_text_texture(SDL_Renderer *renderer, TTF_Font *font,
                                 const char *text, SDL_Color color, int *out_w,
                                 int *out_h) {
  SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
  if (!surf)
    return NULL;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
  if (out_w)
    *out_w = surf->w;
  if (out_h)
    *out_h = surf->h;
  SDL_FreeSurface(surf);
  return tex;
}

// --- 2. HELPER FUNCTIONS ---

// Calculates the on-screen height of a texture based on the current scaling
// mode
int get_scaled_height(AppContext *ctx, int slot, ManhwaScale scale_mode) {
  SDL_Texture *tex = (slot == -1)  ? ctx->tex_prev
                     : (slot == 1) ? ctx->tex_next
                                   : ctx->tex_curr;
  if (!tex)
    return 0;

  int w, h;
  SDL_QueryTexture(tex, NULL, NULL, &w, &h);

  int win_w, win_h;
  SDL_GetRendererOutputSize(ctx->renderer, &win_w, &win_h);

  if (scale_mode == SCALE_FIT_WIDTH) {
    // Mode 'd': Scale to fit width, height grows proportionally
    float scale = (float)win_w / w;
    return (int)(h * scale);
  } else {
    // Mode 's': Scale to fit height (standard view)
    float scale = (float)win_h / h;
    // Don't let it be wider than the screen
    if (w * scale > win_w)
      scale = (float)win_w / w;
    return (int)(h * scale);
  }
}

void render_help_menu(AppContext *ctx, ReadMode mode) {
  if (!ctx->font)
    return;

  const char *lines_std[] = {"--- HELP MENU (Standard) ---",
                             "Arrows : Navigation",
                             "Shift+Arrows : Skip 10 Pages",
                             "S : Single Page View",
                             "D : Double Page View",
                             "Shift+D : Double (Skip Cover)",
                             "G : Go to Page",
                             "B / E : Start / End",
                             "F : Fullscreen",
                             "H : Close Help",
                             "ESC : Quit"};
  int count_std = 11;

  const char *lines_manhwa[] = {"--- HELP MENU (Manhwa) ---",
                                "Scroll / Arrows : Continuous Read",
                                "Left / Right : Jump Page",
                                "S : Fit Height (Default)",
                                "d : Fit Width (Zoomed)",
                                "G : Go to Page",
                                "B / E : Start / End",
                                "F : Fullscreen",
                                "H : Close Help",
                                "ESC : Quit"};
  int count_manhwa = 10;

  const char **lines = (mode == MODE_MANHWA) ? lines_manhwa : lines_std;
  int count = (mode == MODE_MANHWA) ? count_manhwa : count_std;

  // Dim Background
  SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 220);
  SDL_RenderFillRect(ctx->renderer, NULL);

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

void draw_ui(AppContext *ctx, const char *overlay_text,
             const char *input_text) {
  if (!ctx->font)
    return;
  SDL_Color white = {255, 255, 255, 255};
  int win_w, win_h;
  SDL_GetRendererOutputSize(ctx->renderer, &win_w, &win_h);

  // 1. Page Counter Overlay
  if (overlay_text) {
    SDL_Surface *s = TTF_RenderText_Blended(ctx->font, overlay_text, white);
    if (s) {
      SDL_Texture *t = SDL_CreateTextureFromSurface(ctx->renderer, s);
      SDL_Rect dest = {win_w - s->w - 20, win_h - s->h - 10, s->w, s->h};

      // Background Box
      SDL_Rect bg = {dest.x - 5, dest.y - 5, dest.w + 10, dest.h + 10};
      SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 150);
      SDL_RenderFillRect(ctx->renderer, &bg);

      SDL_RenderCopy(ctx->renderer, t, NULL, &dest);
      SDL_FreeSurface(s);
      SDL_DestroyTexture(t);
    }
  }

  // 2. Input Box (Go To Page)
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

      SDL_SetRenderDrawColor(ctx->renderer, 100, 149, 237, 255); // Blue Border
      SDL_RenderDrawRect(ctx->renderer, &bg);

      SDL_RenderCopy(ctx->renderer, t, NULL, &dest);
      SDL_FreeSurface(s);
      SDL_DestroyTexture(t);
    }
  }
}

// --- 3. MAIN RENDER FUNCTION ---

void render_frame(AppContext *ctx, const char *overlay_text,
                  const char *input_text, ViewMode mode, ManhwaScale scale_mode,
                  PageDir dir, int show_help, int scroll_y, ReadMode book_mode,
                  const char *popup_message) {

  SDL_SetRenderDrawColor(ctx->renderer, 30, 30, 30, 255);
  SDL_RenderClear(ctx->renderer);

  // 1. HELP MENU (Exclusive View)
  if (show_help) {
    render_help_menu(ctx, book_mode);
    SDL_RenderPresent(ctx->renderer);
    return;
  }

  int win_w, win_h;
  SDL_GetRendererOutputSize(ctx->renderer, &win_w, &win_h);

  // ==========================================
  // LOGIC 1: MANHWA CONTINUOUS SCROLL
  // ==========================================
  if (mode == VIEW_MANHWA) {
    int center_x = 0;

    // --- A. Draw Current Page (Middle) ---
    if (ctx->tex_curr) {
      int w, h;
      SDL_QueryTexture(ctx->tex_curr, NULL, NULL, &w, &h);
      float scale;
      if (scale_mode == SCALE_FIT_WIDTH)
        scale = (float)win_w / w;
      else { // Fit Height
        scale = (float)win_h / h;
        if (w * scale > win_w)
          scale = (float)win_w / w;
        center_x = (win_w - (w * scale)) / 2;
      }
      SDL_FRect dest = {(float)center_x, (float)-scroll_y, w * scale,
                        h * scale};
      SDL_RenderCopyF(ctx->renderer, ctx->tex_curr, NULL, &dest);
    }

    // --- B. Draw Next Page (Below) ---
    if (ctx->tex_next) {
      int curr_h = get_scaled_height(ctx, 0, scale_mode);
      int w, h;
      SDL_QueryTexture(ctx->tex_next, NULL, NULL, &w, &h);
      float scale;
      if (scale_mode == SCALE_FIT_WIDTH)
        scale = (float)win_w / w;
      else {
        scale = (float)win_h / h;
        if (w * scale > win_w)
          scale = (float)win_w / w;
        center_x = (win_w - (w * scale)) / 2;
      }
      // Position: Starts exactly where Current ends
      SDL_FRect dest = {(float)center_x, (float)(-scroll_y + curr_h), w * scale,
                        h * scale};
      SDL_RenderCopyF(ctx->renderer, ctx->tex_next, NULL, &dest);
    }

    // --- C. Draw Prev Page (Above) ---
    if (ctx->tex_prev) {
      int prev_h = get_scaled_height(ctx, -1, scale_mode);
      int w, h;
      SDL_QueryTexture(ctx->tex_prev, NULL, NULL, &w, &h);
      float scale;
      if (scale_mode == SCALE_FIT_WIDTH)
        scale = (float)win_w / w;
      else {
        scale = (float)win_h / h;
        if (w * scale > win_w)
          scale = (float)win_w / w;
        center_x = (win_w - (w * scale)) / 2;
      }
      // Position: Ends exactly where Current starts
      SDL_FRect dest = {(float)center_x, (float)(-scroll_y - prev_h), w * scale,
                        h * scale};
      SDL_RenderCopyF(ctx->renderer, ctx->tex_prev, NULL, &dest);
    }
  }
  // ==========================================
  // LOGIC 2: STANDARD (Manga/Comic)
  // ==========================================
  else {
    // --- SINGLE VIEW ---
    if (mode == VIEW_SINGLE || !ctx->tex_next) {
      if (ctx->tex_curr) {
        int w, h;
        SDL_QueryTexture(ctx->tex_curr, NULL, NULL, &w, &h);
        float scale = (float)win_h / h;
        if (w * scale > win_w)
          scale = (float)win_w / w;

        SDL_FRect dest = {(win_w - w * scale) / 2, (win_h - h * scale) / 2,
                          w * scale, h * scale};
        SDL_RenderCopyF(ctx->renderer, ctx->tex_curr, NULL, &dest);
      }
    }
    // --- DOUBLE VIEW ---
    else {
      int w1, h1, w2, h2;
      SDL_QueryTexture(ctx->tex_curr, NULL, NULL, &w1, &h1);
      SDL_QueryTexture(ctx->tex_next, NULL, NULL, &w2,
                       &h2); // tex_next is secondary page

      // Scale both to fit height
      float scale = (float)win_h / (h1 > h2 ? h1 : h2);
      float total_w = (w1 + w2) * scale;

      // If combined width is too big, shrink
      if (total_w > win_w)
        scale = scale * (win_w / total_w);

      float dw1 = w1 * scale;
      float dw2 = w2 * scale;
      float start_x = (win_w - (dw1 + dw2)) / 2;
      float y1 = (win_h - h1 * scale) / 2;
      float y2 = (win_h - h2 * scale) / 2;

      SDL_FRect dest1, dest2;

      if (dir == DIR_MANGA) {
        // Manga (RTL): [Next][Current]
        dest2 = (SDL_FRect){start_x, y2, dw2, h2 * scale};       // Left
        dest1 = (SDL_FRect){start_x + dw2, y1, dw1, h1 * scale}; // Right
      } else {
        // Comic (LTR): [Current][Next]
        dest1 = (SDL_FRect){start_x, y1, dw1, h1 * scale};       // Left
        dest2 = (SDL_FRect){start_x + dw1, y2, dw2, h2 * scale}; // Right
      }

      SDL_RenderCopyF(ctx->renderer, ctx->tex_next, NULL, &dest2);
      SDL_RenderCopyF(ctx->renderer, ctx->tex_curr, NULL, &dest1);
    }
  }

  // 3. UI OVERLAYS (Page Count, Input Box)
  draw_ui(ctx, overlay_text, input_text);

  // 4. POPUP MESSAGE (New!)
  if (popup_message) {
    render_popup(ctx, popup_message);
  }

  // 5. PRESENT FRAME (Only called once!)
  SDL_RenderPresent(ctx->renderer);
}

void render_popup(AppContext *ctx, const char *message) {
  if (!ctx->font)
    return;

  int win_w, win_h;
  SDL_GetRendererOutputSize(ctx->renderer, &win_w, &win_h);
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color accent = {100, 149, 237, 255}; // Cornflower Blue

  SDL_Surface *s = TTF_RenderText_Blended(ctx->font, message, white);
  if (s) {
    SDL_Texture *t = SDL_CreateTextureFromSurface(ctx->renderer, s);

    int box_w = s->w + 40;
    int box_h = s->h + 40;
    SDL_Rect box = {(win_w - box_w) / 2, (win_h - box_h) / 2, box_w, box_h};

    // Draw Shadow
    SDL_Rect shadow = {box.x + 5, box.y + 5, box.w, box.h};
    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 100);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(ctx->renderer, &shadow);

    // Draw Box Background
    SDL_SetRenderDrawColor(ctx->renderer, 30, 30, 30, 255);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_NONE);
    SDL_RenderFillRect(ctx->renderer, &box);

    // Draw Border
    SDL_SetRenderDrawColor(ctx->renderer, accent.r, accent.g, accent.b, 255);
    SDL_RenderDrawRect(ctx->renderer, &box);

    // Draw Text
    SDL_Rect text_dest = {box.x + 20, box.y + 20, s->w, s->h};
    SDL_RenderCopy(ctx->renderer, t, NULL, &text_dest);

    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
  }
}
