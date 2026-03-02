#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../pos_util.h"
#include "../core/core.h"
#include "../input/input.h"

/* ── Layout ─────────────────────────────────────────────────── */
typedef struct {
    SDL_Rect products;
    SDL_Rect cart;
    SDL_Rect totals;
    SDL_Rect payments;
} Layout;

Layout mark_layout_bounds(int w, int h);

/* ── Screen (top-level app state machine) ───────────────────── */
typedef enum {
    SCREEN_LOGIN = 0,
    SCREEN_POS,          /* normal sales mode     */
    SCREEN_EDIT,         /* admin edit mode        */
} AppScreen;

/* ── Overlay (modal on top of current screen) ───────────────── */
typedef enum {
    OVERLAY_NONE = 0,
    OVERLAY_CASH_PAYMENT,
    OVERLAY_CARD_PAYMENT,
    OVERLAY_RECEIPT,
    OVERLAY_LOGIN_PIN,       /* PIN pad for selected user  */
    OVERLAY_EDIT_CATEGORY,   /* add/edit a category        */
    OVERLAY_EDIT_PRODUCT,    /* add/edit a product         */
    OVERLAY_EDIT_DISCOUNT,   /* add/edit discount preset   */
    OVERLAY_EDIT_COLOUR,     /* colour picker              */
    OVERLAY_EDIT_USER,       /* add/edit user              */
    OVERLAY_CONFIRM_DELETE,  /* generic confirm/cancel     */
} OverlayMode;

/* ── Generic text-input accumulator ─────────────────────────── */
#define TEXT_BUF_LEN 128
typedef struct {
    char buf[TEXT_BUF_LEN];
    i32  len;
} TextBuf;

static inline void textbuf_clear(TextBuf *t)
    { t->buf[0]='\0'; t->len=0; }
static inline void textbuf_append(TextBuf *t, const char *s)
{
    while (*s && t->len < TEXT_BUF_LEN-1)
        { t->buf[t->len++] = *s++; t->buf[t->len]='\0'; }
}
static inline void textbuf_backspace(TextBuf *t)
    { if (t->len>0) t->buf[--t->len]='\0'; }

/* ── Edit-mode sub-panel selection ─────────────────────────── */
typedef enum {
    EDIT_PANEL_CATEGORIES = 0,
    EDIT_PANEL_PRODUCTS,
    EDIT_PANEL_DISCOUNTS,
    EDIT_PANEL_USERS,
} EditPanel;

/* ── Colour picker state ────────────────────────────────────── */
typedef struct {
    u8   r, g, b;
    i8   target;   /* 0=btn_color  1=text_color */
} ColourPickState;

/* ── UI State ───────────────────────────────────────────────── */
typedef struct {
    AppScreen   screen;
    OverlayMode overlay;
    ProductView current_view;
    i32         selected_category_id;
    i32         focused_cart_index;

    /* ── Scroll ───────────────────────────────────────────── */
    i32  product_scroll_y;
    i32  cart_scroll_y;

    /* ── Alert toast ─────────────────────────────────────── */
    char alert_msg[128];
    u32  alert_ttl_ms;

    /* ── Numeric keypad (cash tendered / PIN) ────────────── */
    TextBuf numpad;

    /* ── Login screen: which user is being logged in ──────── */
    i32  login_selected_user_id;  /* -1 = none */

    /* ── Edit mode ───────────────────────────────────────── */
    EditPanel   edit_panel;          /* which tab is active  */
    i32         edit_selected_id;    /* ID of item being edited, 0=new */

    /* Scratch buffers for the editor forms */
    TextBuf     edit_name;
    TextBuf     edit_price;          /* product price */
    TextBuf     edit_pin;            /* user PIN      */
    i32         edit_role;           /* user role     */
    i32         edit_category_id;    /* product category */

    /* Colour currently being edited */
    ColourPickState colour_pick;
    /* Which colour field the picker is editing */
    /* (stored back into edit_btn_color / edit_txt_color) */
    u8  edit_btn_r, edit_btn_g, edit_btn_b;
    u8  edit_txt_r, edit_txt_g, edit_txt_b;

    /* Confirm-delete: what are we deleting? */
    char confirm_msg[128];
    i32  confirm_delete_id;
    i32  confirm_delete_type;  /* 0=cat 1=prod 2=disc 3=user */

    /* ── Text-input focus ────────────────────────────────── */
    /* Points to the TextBuf currently being edited (NULL = none).
       In keyboard mode: SDL text input events are routed here.
       In touchscreen mode: the OSK writes here when open.       */
    TextBuf *active_text_buf;

} UIState;

/* ── Widgets ─────────────────────────────────────────────────── */
i8 draw_button(SDL_Renderer *r, TTF_Font *font, SDL_Rect bounds,
               const char *text, int mx, int my, i8 mouse_pressed);

/* Coloured button: custom fill + text colour */
i8 draw_button_ex(SDL_Renderer *r, TTF_Font *font, SDL_Rect bounds,
                  const char *text,
                  SDL_Color fill, SDL_Color text_col,
                  int mx, int my, i8 mouse_pressed);

void draw_text(SDL_Renderer *r, TTF_Font *font, const char *text,
               int x, int y, SDL_Color color);

/* Alert toast */
void ui_show_alert(UIState *ui, const char *msg, u32 duration_ms);

/* On-screen keyboard: drawn at bottom of screen when active_text_buf != NULL
   and input_touchscreen == 1.  Returns 1 if the user tapped the close/done key. */
i8 draw_osk(SDL_Renderer *r, TTF_Font *font, InputState *in, UIState *ui, int w, int h);

#endif /* UI_H */
