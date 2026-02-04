#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>
#include "../pos_util.h"

i8 draw_button(SDL_Renderer *r,
                 TTF_Font *font,
                 SDL_Rect bounds,
                 const char *text,
                 int mx, int my,
                 i8 mouse_down)
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

    // draw centered text
    SDL_Surface *surf =
        TTF_RenderUTF8_Blended(font, text, (SDL_Color){255,255,255,255});
    SDL_Texture *tex =
        SDL_CreateTextureFromSurface(r, surf);

    SDL_Rect txt = {
        bounds.x + (bounds.w - surf->w)/2,
        bounds.y + (bounds.h - surf->h)/2,
        surf->w,
        surf->h
    };

    SDL_RenderCopy(r, tex, NULL, &txt);

    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);

    return hot && mouse_down;
}
