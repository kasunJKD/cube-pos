#ifndef APP_H
#define APP_H

#include "../pos_util.h"
#include "../ui/ui.h"
#include "../core/core.h"
#include "../input/input.h"
#include "../hardware/printer.h"
#include "../db/db.h"
#include "../net/sync.h"
#include "../auth/auth.h"
//
#include <SDL2/SDL_ttf.h>

#define DEBUG 0   /* set 1 to show panel outlines */

typedef struct {
    Arena persistant_memory;
    Arena frame_arena;
} MemoryContext;

typedef struct {
    Layout    ui_layout;
    UIState   ui_state;
    MemoryContext mem;

    TTF_Font *font;

    /* ── Cart ──────────────────────────────────────────────── */
    CartItem cart_items[MAX_CART_ITEMS];
    i32      cart_items_count;

    /* ── Products & Categories (DB-loaded, dynamic) ─────── */
    Category      categories[MAX_CATEGORIES];
    i32           category_count;
    Product       products[MAX_PRODUCTS];
    i32           product_count;

    /* ── Discount presets ───────────────────────────────── */
    DiscountPreset discount_presets[MAX_DISC_PRESETS];
    i32            discount_preset_count;

    /* ── Users (loaded at login screen) ─────────────────── */
    User  users[MAX_USERS];
    i32   user_count;

    InputState input_state;

    /* ── Transactions ───────────────────────────────────── */
    i64         next_txn_id;
    Transaction last_txn;

    /* ── Hardware ───────────────────────────────────────── */
    Printer printer;

    /* ── Persistence / Sync ─────────────────────────────── */
    PosDB     db;
    NetConfig net_cfg;
    u32       last_sync_ms;

    /* ── Auth / Session ─────────────────────────────────── */
    Session session;

    /* ── Input mode (from config) ────────────────────────── */
    i8  input_touchscreen;   /* 1 = OSK pops up; 0 = hardware keyboard */

} AppState;

/* Helper: reload categories and products from DB into AppState */
void app_reload_menu(AppState *s);

#endif
