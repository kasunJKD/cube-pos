#include "app.h"
#include "../db/db.h"
#include <string.h>

global AppState app_state = {0};

void app_reload_menu(AppState *s)
{
    s->category_count = db_load_categories(
        &s->db, s->categories, MAX_CATEGORIES);
    s->product_count  = db_load_products(
        &s->db, s->products, MAX_PRODUCTS);
    s->discount_preset_count =
        db_load_discount_presets(
            &s->db, s->discount_presets, MAX_DISC_PRESETS);
}
