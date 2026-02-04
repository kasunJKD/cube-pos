#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>

void 
draw_text(
    SDL_Renderer *r,
    TTF_Font *font,
    const char* text,
    int x, int y,
    SDL_Color color
) 
{
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);

    SDL_Rect dst = {x, y, surf->w, surf->h};

    SDL_RenderCopy(r, tex, NULL, &dst);

    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);    
}
