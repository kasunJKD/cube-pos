#ifndef INPUT_H
#define INPUT_H

#include "../pos_util.h"

#define INPUT_TEXT_BUF 32   /* max SDL_TEXTINPUT chars per frame */

typedef struct {
    i32  mouse_x;
    i32  mouse_y;
    i8   mouse_down;
    i8   mouse_pressed;
    i8   mouse_released;
    i8   prev_mouse_down;
    i32  scroll_y;          /* wheel delta this frame: positive = scroll down */

    /* Hardware keyboard — filled by input_handle_event each frame */
    char text_input[INPUT_TEXT_BUF]; /* UTF-8 chars typed this frame        */
    i8   key_backspace;              /* 1 if backspace was pressed this frame */
    i8   key_enter;                  /* 1 if Return/Enter was pressed         */
} InputState;

/* Reset per-frame keyboard state (call after UI consumes it) */
void input_clear_text(InputState *in);

#endif
