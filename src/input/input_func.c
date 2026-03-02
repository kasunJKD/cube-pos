#include "input.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <string.h>

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

    if (e->type == SDL_MOUSEWHEEL) {
        in->scroll_y += -e->wheel.y;
    }

    if (e->type == SDL_FINGERDOWN) {
        in->mouse_down = 1;
        in->mouse_x = (int)(e->tfinger.x * screen_w);
        in->mouse_y = (int)(e->tfinger.y * screen_h);
    }

    if (e->type == SDL_FINGERUP) {
        in->mouse_down = 0;
    }

    /* Hardware keyboard: text input (handles Unicode, layout-aware) */
    if (e->type == SDL_TEXTINPUT) {
        /* Append to text_input buffer if room */
        int cur = (int)strlen(in->text_input);
        int add = (int)strlen(e->text.text);
        if (cur + add < INPUT_TEXT_BUF - 1) {
            strncat(in->text_input, e->text.text, INPUT_TEXT_BUF - cur - 1);
        }
    }

    /* Backspace and Enter via KEYDOWN (not caught by TEXTINPUT) */
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_BACKSPACE) in->key_backspace = 1;
        if (e->key.keysym.sym == SDLK_RETURN ||
            e->key.keysym.sym == SDLK_RETURN2 ||
            e->key.keysym.sym == SDLK_KP_ENTER) in->key_enter = 1;
    }
}

void input_finalize(InputState *in)
{
    in->mouse_pressed  =  in->mouse_down && !in->prev_mouse_down;
    in->mouse_released = !in->mouse_down &&  in->prev_mouse_down;
    in->prev_mouse_down = in->mouse_down;
    /* scroll_y, text_input, key_* are reset by input_clear_text after UI consumes them */
}

void input_clear_text(InputState *in)
{
    in->text_input[0] = '\0';
    in->key_backspace  = 0;
    in->key_enter      = 0;
}
