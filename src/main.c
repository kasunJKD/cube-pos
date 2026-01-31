#include "pos_util.h"
#include <SDL2/SDL.h>

#define MAX_CART_ITEMS 32

typedef struct {
	Arena persistant_memory;
	Arena frame_arena;
}MemoryContext;

typedef struct {
	i32 id;
	const char* category_name;
} Category;

typedef struct {
	i32 id;
	i32 category_id;
	const char *key_name; //because need to have locality
	float price;
} Product;

typedef struct {
	i32 product_id;
	i32 qty;
	float discount_percent;
} CartItem;

typedef struct {
	SDL_Rect products;
	SDL_Rect cart;
	SDL_Rect totals;
	SDL_Rect payments;
} Layout;

typedef enum {
	VIEW_CATEGORIES,
	VIEW_PRODUCTS
} ProductView;

typedef struct {
	ProductView current_view;
	i32 selected_category_id;
	i32 focused_cart_index;
} UIState;

typedef struct {
	Layout ui_layout;
	UIState ui_state;
	MemoryContext mem;

	CartItem cart_items[MAX_CART_ITEMS];
	i32 cart_items_count;

	Category *categories;
	i32 category_count;

	Product *products;
	i32 product_count;
} AppState;

Category categories[] = {
	{1, "Food"},
	{2, "drink"}
};

Product products[] = {
	{1, 1, "Burger", 1200},
	{2, 1, "Pizza", 1500},
	{3, 1, "Pasta", 1000},
	{4, 1, "Rice", 800},
	{5, 1, "Noodles", 900},
	{6, 2, "Soup", 700},
};


int product_count = 6;

AppState app_state = {0};

//ran in every frame in case size changed
Layout mark_layout(i32 w, i32 h)
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

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "POS",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        //SDL_WINDOW_FULLSCREEN_DESKTOP
        SDL_WINDOW_RESIZABLE
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

   //init section =================
   size_t perm_mem = Mbytes(5);
   unsigned char buffer_perm[perm_mem];
   arena_init(&app_state.mem.persistant_memory, buffer_perm, perm_mem);

   size_t frame_mem = Kbytes(5);
   unsigned char buffer_frame[frame_mem];
   arena_init(&app_state.mem.frame_arena, buffer_frame, frame_mem);

    i8 running = 1;
    SDL_Event e;

    while (running) {
	arena_release_all(&app_state.mem.frame_arena);
	
	while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	
	app_state.ui_layout = mark_layout(w, h);

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
        

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
