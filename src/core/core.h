#ifndef CORE_H
#define CORE_H

#include "../pos_util.h"

#define MAX_CART_ITEMS 32

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

typedef enum {
	VIEW_CATEGORIES,
	VIEW_PRODUCTS
} ProductView;


#endif
