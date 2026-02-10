#ifndef INPUT_H
#define INPUT_H

#include "../pos_util.h"

typedef struct {
	i32 mouse_x;
	i32 mouse_y;
	i8 mouse_down;
	i8 mouse_pressed;
	i8 mouse_released;
	i8 prev_mouse_down;
}InputState;

#endif
