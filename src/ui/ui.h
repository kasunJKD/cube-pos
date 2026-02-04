#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../pos_util.h"
#include "../core/core.h" //might have to change to datatypes.h ?

typedef struct {
	SDL_Rect products;
	SDL_Rect cart;
	SDL_Rect totals;
	SDL_Rect payments;
} Layout;


typedef struct {
	ProductView current_view;
	i32 selected_category_id;
	i32 focused_cart_index;
} UIState;

Layout mark_layout_bounds(int w, int h);


i8 draw_button(SDL_Renderer *r,
                 TTF_Font *font,
                 SDL_Rect bounds,
                 const char *text,
                 int mx, int my,
                 i8 mouse_down);

void 
draw_text(
    SDL_Renderer *r,
    TTF_Font *font,
    const char* text,
    int x, int y,
    SDL_Color color
); 

#endif
