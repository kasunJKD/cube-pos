#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>
#include "../pos_util.h"

i8 draw_button(SDL_Renderer *r,
                 TTF_Font *font,
                 SDL_Rect bounds,
                 const char *text,
                 int mx, int my,
                 i8 mouse_pressed)
{
    i8 hot =
        mx >= bounds.x &&
        mx <= bounds.x + bounds.w &&
        my >= bounds.y &&
        my <= bounds.y + bounds.h;

    SDL_Color fill =
        hot ? (SDL_Color){80,140,200,255}
            : (SDL_Color){60,60,60,255};

    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(r, &bounds);

    SDL_SetRenderDrawColor(r, 200,200,200,255);
    SDL_RenderDrawRect(r, &bounds);

    /* Draw centered text — guard against empty label */
    if (text && text[0] != '\0') {
        SDL_Surface *surf =
            TTF_RenderUTF8_Blended(font, text, (SDL_Color){255,255,255,255});
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex) {
                SDL_Rect txt = {
                    bounds.x + (bounds.w - surf->w)/2,
                    bounds.y + (bounds.h - surf->h)/2,
                    surf->w, surf->h
                };
                SDL_RenderCopy(r, tex, NULL, &txt);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    return hot && mouse_pressed;
}

/* Coloured variant: caller supplies fill and text colours */
i8 draw_button_ex(SDL_Renderer *r, TTF_Font *font, SDL_Rect bounds,
                  const char *text,
                  SDL_Color fill, SDL_Color text_col,
                  int mx, int my, i8 mouse_pressed)
{
    i8 hot =
        mx >= bounds.x &&
        mx <= bounds.x + bounds.w &&
        my >= bounds.y &&
        my <= bounds.y + bounds.h;

    /* Lighten fill slightly when hot */
    SDL_Color f = hot
        ? (SDL_Color){ (u8)SDL_min(fill.r + 40, 255),
                       (u8)SDL_min(fill.g + 40, 255),
                       (u8)SDL_min(fill.b + 40, 255), fill.a }
        : fill;

    SDL_SetRenderDrawColor(r, f.r, f.g, f.b, f.a);
    SDL_RenderFillRect(r, &bounds);

    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderDrawRect(r, &bounds);

    if (text && text[0] != '\0') {
        SDL_Surface *surf =
            TTF_RenderUTF8_Blended(font, text, text_col);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex) {
                SDL_Rect txt = {
                    bounds.x + (bounds.w - surf->w) / 2,
                    bounds.y + (bounds.h - surf->h) / 2,
                    surf->w, surf->h
                };
                SDL_RenderCopy(r, tex, NULL, &txt);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    return hot && mouse_pressed;
}
