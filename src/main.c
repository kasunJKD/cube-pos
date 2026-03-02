#include "pos_util.c"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <signal.h>
#include "app/app.c"
#include "core/product.c"
#include "hardware/printer.c"
#include "db/db.c"
#include "net/sync.c"
#include "config/config.c"
#include "auth/sha256.c"
#include "auth/auth.c"
#include "ui/layout.c"
#include "ui/osk.c"
#include "ui/idraw.c"
#include "ui/button.c"
#include "ui/font.c"
#include "input/input_func.c"


/* ── Signal handling ────────────────────────────────────────── */
static volatile i8 g_quit = 0;

static void sig_handler(int sig)
{
    (void)sig;
    g_quit = 1;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    /* Load config */
    PosConfig cfg;
    config_load(&cfg, "pos.conf");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return 1;
    }

    u32 win_flags = SDL_WINDOW_RESIZABLE;
    if (cfg.fullscreen) win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    SDL_Window *window = SDL_CreateWindow(
        "POS",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        cfg.display_width, cfg.display_height,
        win_flags
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    TTF_Init();

    app_state.font = TTF_OpenFont("./assets/NotoSansSinhala-VariableFont.ttf", 20);
    if (!app_state.font) {
        printf("Font load failed\n");
        return 1;
    }

   //init section =================
   size_t perm_mem = Mbytes(5);
   unsigned char buffer_perm[perm_mem];
   arena_init(&app_state.mem.persistant_memory, buffer_perm, perm_mem);

   size_t frame_mem = Kbytes(5);
   unsigned char buffer_frame[frame_mem];
   arena_init(&app_state.mem.frame_arena, buffer_frame, frame_mem);

   // open local database
   db_open(&app_state.db, cfg.db_path);
   // restore transaction counter from DB (so IDs don't reset on restart)
   app_state.next_txn_id = 1;
   if (db_is_open(&app_state.db)) {
       db_load_next_txn_id(&app_state.db, &app_state.next_txn_id);
       // seed defaults on first run (no-ops if data already exists)
       db_seed_default_admin(&app_state.db);
       db_seed_categories(&app_state.db, categories, category_count);
       db_seed_products(&app_state.db, products, product_count);
       // load DB-backed data into app_state
       app_reload_menu(&app_state);
       // load users for login screen
       app_state.user_count = db_load_users(
           &app_state.db, app_state.users, MAX_USERS);
   }

   // input mode from config
   app_state.input_touchscreen = (i8)cfg.input_touchscreen;

   // start on the login screen
   app_state.ui_state.screen                = SCREEN_LOGIN;
   app_state.ui_state.current_view          = VIEW_CATEGORIES;
   app_state.ui_state.selected_category_id  = 0;
   app_state.ui_state.login_selected_user_id = -1;

   // printer setup — try default device, silently degrade if not found
   app_state.printer.fd = -1;
   snprintf(app_state.printer.store_name,    sizeof(app_state.printer.store_name),    "%s", cfg.store_name);
   snprintf(app_state.printer.store_address, sizeof(app_state.printer.store_address), "%s", cfg.store_address);
   snprintf(app_state.printer.currency,      sizeof(app_state.printer.currency),      "%s", cfg.currency);
   printer_open(&app_state.printer, cfg.printer_device); /* non-fatal if missing */

   // network sync config — from pos.conf
   app_state.net_cfg.enabled = cfg.sync_enabled;
   snprintf(app_state.net_cfg.host,    sizeof(app_state.net_cfg.host),    "%s", cfg.sync_host);
   app_state.net_cfg.port = cfg.sync_port;
   snprintf(app_state.net_cfg.path,    sizeof(app_state.net_cfg.path),    "%s", cfg.sync_path);
   snprintf(app_state.net_cfg.api_key, sizeof(app_state.net_cfg.api_key), "%s", cfg.sync_api_key);
   app_state.last_sync_ms = 0;

    i8  running    = 1;
    u32 last_ms    = SDL_GetTicks();
    SDL_Event e;

    while (running && !g_quit) {
	arena_release_all(&app_state.mem.frame_arena);

	u32 now_ms2 = SDL_GetTicks();
	u32 dt_ms   = now_ms2 - last_ms;
	last_ms     = now_ms2;

	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	
	while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }

            input_handle_event(&app_state.input_state, &e, w, h);
        }

        input_finalize(&app_state.input_state);

        /* Periodic sync */
        u32 now_ms = SDL_GetTicks();
        u32 sync_interval_ms = (u32)(cfg.sync_interval_sec * 1000);
        if (now_ms - app_state.last_sync_ms > sync_interval_ms) {
            app_state.last_sync_ms = now_ms;
            sync_try(&app_state.net_cfg, &app_state.db,
                     app_state.products, app_state.product_count);
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);


	immediate_mode_ui_draw(&app_state, renderer, w, h, dt_ms);

        SDL_RenderPresent(renderer);
    }

    db_close(&app_state.db);
    printer_close(&app_state.printer);
    TTF_CloseFont(app_state.font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
