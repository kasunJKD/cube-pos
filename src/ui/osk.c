/*
 * osk.c — On-Screen Keyboard
 *
 * Drawn at the bottom of the screen whenever ui->active_text_buf != NULL
 * and app_state->input_touchscreen == 1.
 *
 * Layout (4 rows):
 *   Row 0:  1 2 3 4 5 6 7 8 9 0  -  ⌫
 *   Row 1:  q w e r t y u i o p
 *   Row 2:  a s d f g h j k l
 *   Row 3:  ⇧  z x c v b n m  .  SPACE  Done
 *
 * Shift toggles upper/lower case for one keystroke then reverts.
 * Caps-lock: double-tap Shift.
 */

#include "ui.h"
#include <string.h>
#include <stdio.h>

/* ── Key geometry ──────────────────────────────────────────────── */
#define OSK_KEY_H     52
#define OSK_KEY_GAP    6
#define OSK_ROWS       4
#define OSK_PAD_X     10
#define OSK_PAD_Y      8

/* Shift state */
typedef enum {
    SHIFT_OFF   = 0,
    SHIFT_ONCE  = 1,   /* next key is uppercase, then back to off */
    SHIFT_CAPS  = 2,   /* locked upper until tapped again          */
} ShiftState;

static ShiftState s_shift = SHIFT_OFF;

/* Key definition */
typedef struct {
    const char *lo;   /* label / char when shift off   */
    const char *hi;   /* label / char when shift on    */
    int          w;   /* width multiplier (1 = normal) */
} OskKey;

/* Row definitions ------------------------------------------------- */
static const OskKey ROW0[] = {
    {"1","!",1},{"2","@",1},{"3","#",1},{"4","$",1},{"5","%",1},
    {"6","^",1},{"7","&",1},{"8","*",1},{"9","(",1},{"0",")",1},
    {"-","_",1},{"<",NULL,2},  /* backspace — w=2 */
};
static const OskKey ROW1[] = {
    {"q","Q",1},{"w","W",1},{"e","E",1},{"r","R",1},{"t","T",1},
    {"y","Y",1},{"u","U",1},{"i","I",1},{"o","O",1},{"p","P",1},
};
static const OskKey ROW2[] = {
    {"a","A",1},{"s","S",1},{"d","D",1},{"f","F",1},{"g","G",1},
    {"h","H",1},{"j","J",1},{"k","K",1},{"l","L",1},
};
static const OskKey ROW3[] = {
    {"^",NULL,2},    /* shift — w=2 */
    {"z","Z",1},{"x","X",1},{"c","C",1},{"v","V",1},{"b","B",1},
    {"n","N",1},{"m","M",1},{".",".",1},
    {" ",NULL,3},    /* space — w=3 */
    {"Done",NULL,3}, /* done  — w=3 */
};

#define ROWLEN(r) ((int)(sizeof(r)/sizeof(r[0])))

typedef struct { const OskKey *keys; int count; } RowDef;
static const RowDef ROWS[OSK_ROWS] = {
    {ROW0, ROWLEN(ROW0)},
    {ROW1, ROWLEN(ROW1)},
    {ROW2, ROWLEN(ROW2)},
    {ROW3, ROWLEN(ROW3)},
};

/* ── Geometry helpers ──────────────────────────────────────────── */

/* Total unit-widths in a row (each key is w units wide) */
static int row_total_units(const RowDef *row)
{
    int total = 0;
    for (int i = 0; i < row->count; i++) total += row->keys[i].w;
    return total;
}

/* Panel height */
static int osk_panel_h(void)
{
    return OSK_ROWS * (OSK_KEY_H + OSK_KEY_GAP) + OSK_PAD_Y * 2;
}

/* ── Draw helper (re-uses draw_button_ex from button.c) ────────── */
/* We can't include button.c directly (unity build handles that), so
   we call draw_button / draw_button_ex which are already in scope. */

i8 draw_osk(SDL_Renderer *r, TTF_Font *font,
            InputState *in, UIState *ui, int w, int h)
{
    if (!ui->active_text_buf) return 0;

    int ph     = osk_panel_h();
    int panel_y = h - ph;
    int panel_x = 0;

    /* Dim background behind OSK */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_Rect dim = {0, panel_y, w, ph};
    SDL_RenderFillRect(r, &dim);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    /* OSK panel */
    SDL_SetRenderDrawColor(r, 28, 30, 40, 255);
    SDL_Rect bg = {panel_x, panel_y, w, ph};
    SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawColor(r, 60, 65, 90, 255);
    SDL_RenderDrawRect(r, &bg);

    i8 done = 0;

    for (int ri = 0; ri < OSK_ROWS; ri++) {
        const RowDef *row  = &ROWS[ri];
        int row_y = panel_y + OSK_PAD_Y + ri * (OSK_KEY_H + OSK_KEY_GAP);

        /* Available width for keys in this row */
        int avail  = w - OSK_PAD_X * 2;
        int units  = row_total_units(row);
        /* unit_w = (avail - gaps_between_keys) / total_units */
        int gaps   = (row->count - 1) * OSK_KEY_GAP;
        int unit_w = (avail - gaps) / units;

        int kx = panel_x + OSK_PAD_X;

        for (int ki = 0; ki < row->count; ki++) {
            const OskKey *k = &row->keys[ki];
            int kw = k->w * unit_w + (k->w - 1) * OSK_KEY_GAP;

            SDL_Rect btn = {kx, row_y, kw, OSK_KEY_H};

            /* Pick label */
            i8 shifted = (s_shift != SHIFT_OFF);
            const char *label = (shifted && k->hi) ? k->hi : k->lo;

            /* Special-key colours */
            SDL_Color fill, tcol;
            i8 is_shift   = (k->lo[0] == '^' && k->lo[1] == '\0');
            i8 is_bksp    = (k->lo[0] == '<' && k->lo[1] == '\0');
            i8 is_space   = (k->lo[0] == ' ' && k->lo[1] == '\0');
            i8 is_done    = (strncmp(k->lo, "Done", 4) == 0);

            if (is_done) {
                fill = (SDL_Color){30,  110,  60,  255};
                tcol = (SDL_Color){180, 255, 200,  255};
            } else if (is_bksp) {
                fill = (SDL_Color){110,  40,  40, 255};
                tcol = (SDL_Color){255, 180, 180, 255};
            } else if (is_shift) {
                SDL_Color base = (s_shift == SHIFT_CAPS)
                    ? (SDL_Color){80, 120, 200, 255}
                    : (SDL_Color){70,  70, 100, 255};
                fill = base;
                tcol = (SDL_Color){220, 220, 255, 255};
            } else if (is_space) {
                fill = (SDL_Color){50, 54, 72, 255};
                tcol = (SDL_Color){200, 200, 200, 255};
            } else {
                fill = (SDL_Color){48, 52, 70, 255};
                tcol = (SDL_Color){230, 230, 230, 255};
            }

            /* Hotness check for colour lightening (handled inside draw_button_ex) */
            i8 tapped = draw_button_ex(r, font, btn, label,
                                       fill, tcol,
                                       in->mouse_x, in->mouse_y,
                                       in->mouse_pressed);

            if (tapped) {
                if (is_done) {
                    /* Close OSK, deactivate field */
                    ui->active_text_buf = NULL;
                    s_shift = SHIFT_OFF;
                    done = 1;
                } else if (is_bksp) {
                    textbuf_backspace(ui->active_text_buf);
                } else if (is_shift) {
                    /* Toggle: off→once→caps→off */
                    if      (s_shift == SHIFT_OFF)  s_shift = SHIFT_ONCE;
                    else if (s_shift == SHIFT_ONCE)  s_shift = SHIFT_CAPS;
                    else                             s_shift = SHIFT_OFF;
                } else {
                    /* Append the character */
                    textbuf_append(ui->active_text_buf, label);
                    /* If shift-once, revert after one keypress */
                    if (s_shift == SHIFT_ONCE) s_shift = SHIFT_OFF;
                }
            }

            kx += kw + OSK_KEY_GAP;
        }
    }

    return done;
}
