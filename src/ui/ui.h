#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
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

#endif
