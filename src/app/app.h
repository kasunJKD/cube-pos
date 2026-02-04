#ifndef APP_H
#define APP_H

#include "../pos_util.h"
#include "../ui/ui.h"
#include "../core/core.h" //might have to change to datatypes.h ?
//
#include <SDL2/SDL_ttf.h>

#define DEBUG 1

typedef struct {
	Arena persistant_memory;
	Arena frame_arena;
}MemoryContext;

//add this to seperate H file
typedef struct {
	i32 mouse_x;
	i32 mouse_y;
	i8 mouse_down;
	i8 mouse_pressed;
	i8 prev_mouse_down;
}InputState;

typedef struct {
	Layout ui_layout;
	UIState ui_state;
	MemoryContext mem;

	TTF_Font *font;

	CartItem cart_items[MAX_CART_ITEMS];
	i32 cart_items_count;

	Category *categories;
	i32 category_count;

	Product *products;
	i32 product_count;

	InputState input_state;

} AppState;


#endif
