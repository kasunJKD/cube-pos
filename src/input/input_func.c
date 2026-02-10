#include "input.h"
#include <SDL2/SDL_events.h>


void input_handle_event(InputState *in, SDL_Event *e, int screen_w, int screen_h)
{
    if (e->type == SDL_MOUSEMOTION) {
        in->mouse_x = e->motion.x;
        in->mouse_y = e->motion.y;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN) {
        in->mouse_down = 1;
    }

    if (e->type == SDL_MOUSEBUTTONUP) {
        in->mouse_down = 0;
    }

if (e->type == SDL_FINGERDOWN) {
    in->mouse_down = 1;
    in->mouse_x = e->tfinger.x * screen_w;
    in->mouse_y = e->tfinger.y * screen_h;
}

if (e->type == SDL_FINGERUP) {
    in->mouse_down = 0;
}
}

void input_finalize(InputState *in)
{
    in->mouse_pressed  =  in->mouse_down && !in->prev_mouse_down;
    in->mouse_released = !in->mouse_down &&  in->prev_mouse_down;

    in->prev_mouse_down = in->mouse_down;
}
