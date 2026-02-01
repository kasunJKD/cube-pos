#include "ui.h"

//ran in every frame in case size changed
Layout mark_layout_bounds(int w, int h)
{
	Layout layout = {0};
	int padding = 10;
	int left_w  = w * 0.55;
	int right_w = w - left_w - padding*3;

	layout.products = (SDL_Rect){
		padding,
		padding,
		left_w,
		h - 200
	};

	layout.cart = (SDL_Rect){
		left_w + padding*2,
		padding,
		right_w,
		h - 300
	};

	layout.totals = (SDL_Rect){
		left_w + padding*2,
		h - 280,
		right_w,
		120
	};

	layout.payments = (SDL_Rect){
		padding,
		h - 160,
		w - padding*2,
		140
	};

	return layout; 
}

