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

void draw_product_grid(SDL_Renderer *r,
                       TTF_Font *font,
                       SDL_Rect area,
                       int mx, int my, i8 md)
{
    int cols = 3;
    int gap = 10;

    int bw = (area.w - gap*(cols+1)) / cols;
    int bh = 90;

    const char *items[] = {
        "Burger","Pizza","Rice","Soup","Coke","Water"
    };

    int count = 6;

    for (int i=0;i<count;i++)
    {
        int col = i % cols;
        int row = i / cols;

        SDL_Rect btn = {
            area.x + gap + col*(bw+gap),
            area.y + gap + row*(bh+gap),
            bw,
            bh
        };

        if (draw_button(r,font,btn,items[i],mx,my,md)) {
            printf("Pressed %s\n", items[i]);
        }
    }
}

void draw_cart(SDL_Renderer *r,
               TTF_Font *font,
               SDL_Rect area)
{
    int y = area.y + 10;

    draw_text(r,font,"Cart Items", area.x+10,y,(SDL_Color){255,255,255,255});
    y += 40;

    draw_text(r,font,"Burger x2", area.x+10,y,(SDL_Color){200,200,200,255});
    y += 30;

    draw_text(r,font,"Pizza x1", area.x+10,y,(SDL_Color){200,200,200,255});
}

void draw_payment(SDL_Renderer *r,
                  TTF_Font *font,
                  SDL_Rect area,
                  int mx,int my,i8 md)
{
    SDL_Rect cash = {
        area.x + 20,
        area.y + 30,
        180,80
    };

    SDL_Rect card = {
        area.x + 220,
        area.y + 30,
        180,80
    };

    if (draw_button(r,font,cash,"CASH",mx,my,md))
        printf("Cash\n");

    if (draw_button(r,font,card,"CARD",mx,my,md))
        printf("Card\n");
}

void immediate_mode_ui_draw(AppState app_state,  SDL_Renderer *renderer, int w, int h)
{

	app_state.ui_layout = mark_layout_bounds(w, h);
	#if DEBUG
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



	draw_product_grid(renderer,app_state.font,app_state.ui_layout.products,app_state.input_state.mouse_x,app_state.input_state.mouse_y, app_state.input_state.mouse_down);
	draw_cart(renderer,app_state.font,app_state.ui_layout.cart);
	draw_payment(renderer,app_state.font,app_state.ui_layout.payments,app_state.input_state.mouse_x,app_state.input_state.mouse_y, app_state.input_state.mouse_down);
}
