#include "ui.h"
#include "../app/app.h"

void draw_panel(SDL_Renderer *r, SDL_Rect rect,
                SDL_Color fill, SDL_Color border)
{
    // fill
    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(r, &rect);

    // border
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(r, &rect);
}

void immediate_mode_ui_draw(AppState app_state,  SDL_Renderer *renderer, int w, int h)
{
	#if DEBUG

	app_state.ui_layout = mark_layout_bounds(w, h);

	draw_panel(renderer, app_state.ui_layout.products,
		   (SDL_Color){40,40,40,255},
		   (SDL_Color){100,100,100,255});

	draw_panel(renderer, app_state.ui_layout.cart,
		   (SDL_Color){35,35,35,255},
		   (SDL_Color){120,120,120,255});

	draw_panel(renderer, app_state.ui_layout.totals,
		   (SDL_Color){30,30,30,255},
		   (SDL_Color){150,150,150,255});

	draw_panel(renderer, app_state.ui_layout.payments,
		   (SDL_Color){25,25,25,255},
		   (SDL_Color){180,180,180,255});
	#endif
}
