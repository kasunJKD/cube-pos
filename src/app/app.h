#ifndef APP_H
#define APP_H

#include "../pos_util.h"
#include "../ui/ui.h"
#include "../core/core.h" //might have to change to datatypes.h ?

#define DEBUG 1

typedef struct {
	Arena persistant_memory;
	Arena frame_arena;
}MemoryContext;

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

#endif
