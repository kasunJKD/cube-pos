#include "pos_util.c"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "app/app.c"
#include "core/product.c"
#include "ui/layout.c"
#include "ui/idraw.c"
#include "ui/button.c"
#include "ui/font.c"


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

    i8 running = 1;
    SDL_Event e;

    while (running) {
	arena_release_all(&app_state.mem.frame_arena);
	
	while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
            if (e.type == SDL_MOUSEMOTION) {
                app_state.input_state.mouse_x = e.motion.x;
                app_state.input_state.mouse_y = e.motion.y;
            }


            if (e.type == SDL_MOUSEBUTTONDOWN) {
                app_state.input_state.mouse_down = 1;
            }


            if (e.type == SDL_MOUSEBUTTONUP) {
                app_state.input_state.mouse_down = 0;
            }

            app_state.input_state.prev_mouse_down = app_state.input_state.mouse_down;
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

	int w, h;
	SDL_GetWindowSize(window, &w, &h);

	immediate_mode_ui_draw(app_state, renderer, w, h);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
