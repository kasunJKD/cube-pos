#include "ui.h"
#include "../app/app.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────
   Primitive: filled panel with border
   ───────────────────────────────────────────────────────────── */
void draw_panel(SDL_Renderer *r, SDL_Rect rect,
                SDL_Color fill, SDL_Color border)
{
    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(r, &rect);
}

/* ─────────────────────────────────────────────────────────────
   Cart helpers
   ───────────────────────────────────────────────────────────── */

/* Find existing cart slot for a product, returns -1 if not found */
internal i32
cart_find(AppState *s, i32 product_id)
{
    for (i32 i = 0; i < s->cart_items_count; i++) {
        if (s->cart_items[i].product_id == product_id)
            return i;
    }
    return -1;
}

/* Add one unit of product to cart, or increment if already present */
internal void
cart_add(AppState *s, i32 product_id)
{
    i32 idx = cart_find(s, product_id);
    if (idx >= 0) {
        s->cart_items[idx].qty++;
        return;
    }
    if (s->cart_items_count >= MAX_CART_ITEMS) return;

    CartItem *item = &s->cart_items[s->cart_items_count++];
    item->product_id      = product_id;
    item->qty             = 1;
    item->discount_percent = 0.0f;
}

/* Decrement qty; remove slot if qty reaches 0 */
internal void
cart_decrement(AppState *s, i32 index)
{
    if (index < 0 || index >= s->cart_items_count) return;
    s->cart_items[index].qty--;
    if (s->cart_items[index].qty <= 0) {
        /* Compact: overwrite with last */
        s->cart_items[index] = s->cart_items[--s->cart_items_count];
    }
}

/* Find product by id in app_state arrays */
internal Product*
product_by_id(AppState *s, i32 id)
{
    for (i32 i = 0; i < s->product_count; i++) {
        if (s->products[i].id == id)
            return &s->products[i];
    }
    return NULL;
}

/* ─────────────────────────────────────────────────────────────
   Cart totals
   ───────────────────────────────────────────────────────────── */
typedef struct {
    float subtotal;
    float discount_total;
    float tax;           /* 0% by default — set TAX_RATE to enable */
    float grand_total;
} CartTotals;

#define TAX_RATE 0.0f   /* e.g. 0.08f for 8% */

internal CartTotals
cart_calc_totals(AppState *s)
{
    CartTotals t = {0};
    for (i32 i = 0; i < s->cart_items_count; i++) {
        Product *p = product_by_id(s, s->cart_items[i].product_id);
        if (!p) continue;
        float line = p->price * (float)s->cart_items[i].qty;
        float disc = line * (s->cart_items[i].discount_percent / 100.0f);
        t.subtotal       += line;
        t.discount_total += disc;
    }
    float after_disc  = t.subtotal - t.discount_total;
    t.tax             = after_disc * TAX_RATE;
    t.grand_total     = after_disc + t.tax;
    return t;
}

/* ─────────────────────────────────────────────────────────────
   Category grid
   ───────────────────────────────────────────────────────────── */
internal void
draw_category_grid(AppState *s, SDL_Renderer *r, SDL_Rect area)
{
    int cols   = 3;
    int gap    = 12;
    int bw     = (area.w - gap * (cols + 1)) / cols;
    int bh     = 100;
    int scroll = s->ui_state.product_scroll_y;

    SDL_RenderSetClipRect(r, &area);

    for (i32 i = 0; i < s->category_count; i++) {
        int col = i % cols;
        int row = i / cols;

        SDL_Rect btn = {
            area.x + gap + col * (bw + gap),
            area.y + gap + row * (bh + gap) - scroll,
            bw, bh
        };

        if (draw_button(r, s->font, btn,
                        s->categories[i].category_name,
                        s->input_state.mouse_x,
                        s->input_state.mouse_y,
                        s->input_state.mouse_pressed)) {
            s->ui_state.selected_category_id = s->categories[i].id;
            s->ui_state.current_view         = VIEW_PRODUCTS;
            s->ui_state.product_scroll_y     = 0;
        }
    }

    SDL_RenderSetClipRect(r, NULL);
}

/* ─────────────────────────────────────────────────────────────
   Product grid (filtered by selected category, + back button)
   ───────────────────────────────────────────────────────────── */
internal void
draw_product_grid(AppState *s, SDL_Renderer *r, SDL_Rect area)
{
    int gap    = 12;
    int cols   = 3;
    int bw     = (area.w - gap * (cols + 1)) / cols;
    int bh     = 90;
    int scroll = s->ui_state.product_scroll_y;

    SDL_RenderSetClipRect(r, &area);

    /* Back button — not scrolled */
    SDL_Rect back_btn = {area.x + gap, area.y + gap, 120, 44};
    if (draw_button(r, s->font, back_btn, "< Back",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->ui_state.current_view     = VIEW_CATEGORIES;
        s->ui_state.product_scroll_y = 0;
    }

    int grid_y_start = area.y + gap + 44 + gap - scroll;
    int item_index   = 0;

    for (i32 i = 0; i < s->product_count; i++) {
        if (s->products[i].category_id != s->ui_state.selected_category_id)
            continue;

        int col = item_index % cols;
        int row = item_index / cols;

        SDL_Rect btn = {
            area.x + gap + col * (bw + gap),
            grid_y_start + row * (bh + gap),
            bw, bh
        };

        /* Build label: "Name\nPrice" — show price below name */
        char label[64];
        snprintf(label, sizeof(label), "%s", s->products[i].key_name);

        if (draw_button(r, s->font, btn, label,
                        s->input_state.mouse_x,
                        s->input_state.mouse_y,
                        s->input_state.mouse_pressed)) {
            cart_add(s, s->products[i].id);
        }

        /* Draw price below button */
        char price_str[32];
        snprintf(price_str, sizeof(price_str), "Rs %.0f", s->products[i].price);
        draw_text(r, s->font, price_str,
                  btn.x + 4, btn.y + btn.h - 22,
                  (SDL_Color){200, 230, 200, 255});

        item_index++;
    }

    SDL_RenderSetClipRect(r, NULL);
}

/* ─────────────────────────────────────────────────────────────
   Cart panel: list items, qty, line total, tap [-] to decrement
   ───────────────────────────────────────────────────────────── */
internal void
draw_cart(AppState *s, SDL_Renderer *r, SDL_Rect area)
{
    int pad    = 8;
    int scroll = s->ui_state.cart_scroll_y;

    /* Header — not scrolled */
    draw_text(r, s->font, "CART",
              area.x + pad, area.y + 8, (SDL_Color){255, 220, 80, 255});

    SDL_RenderSetClipRect(r, &area);

    int y = area.y + 44 - scroll;

    if (s->cart_items_count == 0) {
        draw_text(r, s->font, "Empty",
                  area.x + pad, y, (SDL_Color){120, 120, 120, 255});
        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    for (i32 i = 0; i < s->cart_items_count; i++) {
        Product *p = product_by_id(s, s->cart_items[i].product_id);
        if (!p) continue;

        float line_total = p->price * (float)s->cart_items[i].qty;

        /* [-] button */
        SDL_Rect minus_btn = {area.x + pad, y, 36, 36};
        if (draw_button(r, s->font, minus_btn, "-",
                        s->input_state.mouse_x,
                        s->input_state.mouse_y,
                        s->input_state.mouse_pressed)) {
            cart_decrement(s, i);
            /* index may now be invalid if item was removed; restart render next frame */
            break;
        }

        /* product name + qty */
        char name_str[64];
        snprintf(name_str, sizeof(name_str), "%s x%d",
                 p->key_name, s->cart_items[i].qty);
        draw_text(r, s->font, name_str,
                  area.x + pad + 42, y + 8,
                  (SDL_Color){220, 220, 220, 255});

        /* line total — right-aligned inside cart panel */
        char total_str[32];
        snprintf(total_str, sizeof(total_str), "%.0f", line_total);
        draw_text(r, s->font, total_str,
                  area.x + area.w - 80, y + 8,
                  (SDL_Color){180, 255, 180, 255});

        y += 44;
    }

    SDL_RenderSetClipRect(r, NULL);
}

/* ─────────────────────────────────────────────────────────────
   Totals panel: subtotal, discount, tax, grand total
   ───────────────────────────────────────────────────────────── */
internal void
draw_totals(AppState *s, SDL_Renderer *r, SDL_Rect area)
{
    CartTotals t = cart_calc_totals(s);

    int x   = area.x + 10;
    int y   = area.y + 8;
    int lh  = 26; /* line height */

    char buf[64];

    snprintf(buf, sizeof(buf), "Subtotal:   Rs %.2f", t.subtotal);
    draw_text(r, s->font, buf, x, y, (SDL_Color){200, 200, 200, 255});
    y += lh;

    if (t.discount_total > 0.0f) {
        snprintf(buf, sizeof(buf), "Discount:  -Rs %.2f", t.discount_total);
        draw_text(r, s->font, buf, x, y, (SDL_Color){255, 160, 80, 255});
        y += lh;
    }

    if (TAX_RATE > 0.0f) {
        snprintf(buf, sizeof(buf), "Tax (%.0f%%):  Rs %.2f",
                 TAX_RATE * 100.0f, t.tax);
        draw_text(r, s->font, buf, x, y, (SDL_Color){200, 200, 200, 255});
        y += lh;
    }

    /* Divider */
    SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
    SDL_RenderDrawLine(r, x, y, area.x + area.w - 10, y);
    y += 4;

    snprintf(buf, sizeof(buf), "TOTAL:      Rs %.2f", t.grand_total);
    draw_text(r, s->font, buf, x, y, (SDL_Color){80, 230, 80, 255});
}

/* ─────────────────────────────────────────────────────────────
   Payment panel
   ───────────────────────────────────────────────────────────── */
internal void
draw_payment(AppState *s, SDL_Renderer *r, SDL_Rect area)
{
    int bw = 180, bh = 70;
    int gap = 20;
    int y   = area.y + (area.h - bh) / 2;

    SDL_Rect cash_btn = {area.x + gap,               y, bw, bh};
    SDL_Rect card_btn = {area.x + gap + bw + gap,     y, bw, bh};
    SDL_Rect clr_btn  = {area.x + area.w - bw - gap,  y, bw, bh};

    i8 has_items = s->cart_items_count > 0;

    if (has_items && draw_button(r, s->font, cash_btn, "CASH",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->ui_state.overlay = OVERLAY_CASH_PAYMENT;
        textbuf_clear(&s->ui_state.numpad);
    }

    if (has_items && draw_button(r, s->font, card_btn, "CARD",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->ui_state.overlay = OVERLAY_CARD_PAYMENT;
    }

    if (draw_button(r, s->font, clr_btn, "CLEAR CART",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->cart_items_count = 0;
        s->ui_state.overlay = OVERLAY_NONE;
    }
}

/* ─────────────────────────────────────────────────────────────
   Build a Transaction from the current cart + payment info
   ───────────────────────────────────────────────────────────── */
internal void
finalise_transaction(AppState *s, PaymentMethod method, float tendered)
{
    Transaction *t = &s->last_txn;
    memset(t, 0, sizeof(*t));

    t->id             = s->next_txn_id++;
    t->timestamp      = (i64)time(NULL);
    t->payment_method = method;
    t->tendered       = tendered;
    t->item_count     = 0;

    snprintf(t->receipt_ref, sizeof(t->receipt_ref), "TXN-%05lld", (long long)t->id);

    for (i32 i = 0; i < s->cart_items_count; i++) {
        Product *p = product_by_id(s, s->cart_items[i].product_id);
        if (!p) continue;

        TxnItem *ti    = &t->items[t->item_count++];
        ti->product_id = p->id;
        ti->qty        = s->cart_items[i].qty;
        ti->unit_price = p->price;
        ti->discount_percent = s->cart_items[i].discount_percent;

        float line = p->price * (float)s->cart_items[i].qty;
        float disc = line * (s->cart_items[i].discount_percent / 100.0f);
        ti->line_total = line - disc;

        t->subtotal       += line;
        t->discount_total += disc;
    }

    float after_disc  = t->subtotal - t->discount_total;
    t->tax            = after_disc * TAX_RATE;
    t->grand_total    = after_disc + t->tax;
    t->change_due     = (method == PAY_CASH) ? (tendered - t->grand_total) : 0.0f;
    t->status         = TXN_COMPLETE;

    /* Clear cart */
    s->cart_items_count = 0;

    printf("[POS] Transaction %s complete. Total: %.2f, Change: %.2f\n",
           t->receipt_ref, t->grand_total, t->change_due);

    /* Persist to local DB (offline-safe — queued for sync) */
    if (db_is_open(&s->db)) {
        db_save_transaction(&s->db, t);
    }

    /* Open cash drawer on cash payments */
    if (method == PAY_CASH && printer_is_open(&s->printer)) {
        printer_open_drawer(&s->printer);
    }

    /* Print receipt */
    if (printer_is_open(&s->printer)) {
        if (printer_print_receipt(&s->printer, t, s->products, s->product_count) != 0) {
            ui_show_alert(&s->ui_state, "Printer error — check connection", 4000);
        }
    } else {
        ui_show_alert(&s->ui_state, "Printer offline — receipt not printed", 4000);
    }
}

/* ─────────────────────────────────────────────────────────────
   Numeric keypad widget — draws a 3x4 numpad inside bounds.
   Returns 1 if the user pressed ENTER/OK (value ready in buf).
   Returns -1 if user pressed CANCEL/BACK.
   ───────────────────────────────────────────────────────────── */
internal i8
draw_numpad(AppState *s, SDL_Renderer *r,
            SDL_Rect area,
            const char *prompt,
            char *buf, i32 *buf_len, i32 buf_cap)
{
    /* Darken overlay behind the numpad area */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_Rect screen = {0, 0, area.x + area.w + 200, area.y + area.h + 200};
    SDL_RenderFillRect(r, &screen);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    /* Panel background */
    draw_panel(r, area, (SDL_Color){40,44,52,255}, (SDL_Color){100,180,255,255});

    /* Prompt */
    draw_text(r, s->font, prompt,
              area.x + 16, area.y + 12, (SDL_Color){200,220,255,255});

    /* Display buffer — large enough for "Rs " + buf contents */
    char display[TEXT_BUF_LEN + 8];
    if (*buf_len == 0)
        snprintf(display, sizeof(display), "Rs 0");
    else {
        snprintf(display, sizeof(display), "Rs %s", buf);
    }
    draw_text(r, s->font, display,
              area.x + 16, area.y + 44, (SDL_Color){80,255,160,255});

    /* Key layout */
    static const char *keys[] = {
        "7","8","9",
        "4","5","6",
        "1","2","3",
        "0","00","<"
    };

    int cols = 3;
    int kw = 90, kh = 64, gap = 10;
    int grid_x = area.x + 16;
    int grid_y = area.y + 84;
    i8 ret = 0;

    for (int i = 0; i < 12; i++) {
        int col = i % cols;
        int row = i / cols;
        SDL_Rect kb = {
            grid_x + col * (kw + gap),
            grid_y + row * (kh + gap),
            kw, kh
        };
        if (draw_button(r, s->font, kb, keys[i],
                        s->input_state.mouse_x, s->input_state.mouse_y,
                        s->input_state.mouse_pressed)) {
            if (keys[i][0] == '<') {
                /* backspace */
                if (*buf_len > 0) buf[--(*buf_len)] = '\0';
            } else {
                /* append digits */
                const char *k = keys[i];
                while (*k && *buf_len < buf_cap - 1) {
                    buf[(*buf_len)++] = *k++;
                    buf[*buf_len] = '\0';
                }
            }
        }
    }

    /* OK and Cancel buttons below the grid */
    int btn_y = grid_y + 4 * (kh + gap) + gap;
    int btn_w = (cols * (kw + gap) - gap) / 2 - gap / 2;

    SDL_Rect ok_btn  = {grid_x,               btn_y, btn_w, kh};
    SDL_Rect can_btn = {grid_x + btn_w + gap, btn_y, btn_w, kh};

    if (draw_button(r, s->font, ok_btn, "OK",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ret = 1;
    }
    if (draw_button(r, s->font, can_btn, "Cancel",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ret = -1;
    }

    return ret;
}

/* ─────────────────────────────────────────────────────────────
   Cash payment overlay
   ───────────────────────────────────────────────────────────── */
internal void
draw_overlay_cash(AppState *s, SDL_Renderer *r, int w, int h)
{
    CartTotals t = cart_calc_totals(s);
    char prompt[64];
    snprintf(prompt, sizeof(prompt),
             "Enter cash tendered (Total: Rs %.0f)", t.grand_total);

    int pw = 380, ph = 560;
    SDL_Rect pad = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    i8 res = draw_numpad(s, r, pad, prompt,
                         s->ui_state.numpad.buf,
                         &s->ui_state.numpad.len,
                         (i32)sizeof(s->ui_state.numpad.buf));

    if (res == 1) {
        float tendered = (float)atoi(s->ui_state.numpad.buf);
        if (tendered >= t.grand_total) {
            finalise_transaction(s, PAY_CASH, tendered);
            s->ui_state.overlay = OVERLAY_RECEIPT;
        }
        /* if not enough cash, stay open — user sees the display */
    } else if (res == -1) {
        s->ui_state.overlay = OVERLAY_NONE;
    }
}

/* ─────────────────────────────────────────────────────────────
   Card payment overlay — simplified prompt, confirm completes sale
   ───────────────────────────────────────────────────────────── */
internal void
draw_overlay_card(AppState *s, SDL_Renderer *r, int w, int h)
{
    CartTotals t = cart_calc_totals(s);
    int pw = 420, ph = 260;
    SDL_Rect panel = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    /* Semi-transparent background */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_panel(r, panel, (SDL_Color){40,44,52,255}, (SDL_Color){100,180,255,255});

    draw_text(r, s->font, "CARD PAYMENT",
              panel.x + 16, panel.y + 16, (SDL_Color){200,220,255,255});

    char total_str[64];
    snprintf(total_str, sizeof(total_str), "Total: Rs %.2f", t.grand_total);
    draw_text(r, s->font, total_str,
              panel.x + 16, panel.y + 52, (SDL_Color){80,255,160,255});

    draw_text(r, s->font, "Tap / Insert / Swipe card...",
              panel.x + 16, panel.y + 88, (SDL_Color){200,200,200,255});

    int bw = 160, bh = 56, gap = 16;
    int btn_y = panel.y + ph - bh - gap;

    SDL_Rect confirm = {panel.x + gap,                 btn_y, bw, bh};
    SDL_Rect cancel  = {panel.x + pw - bw - gap,        btn_y, bw, bh};

    if (draw_button(r, s->font, confirm, "CONFIRM",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        finalise_transaction(s, PAY_CARD, t.grand_total);
        s->ui_state.overlay = OVERLAY_RECEIPT;
    }

    if (draw_button(r, s->font, cancel, "Cancel",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->ui_state.overlay = OVERLAY_NONE;
    }
}

/* ─────────────────────────────────────────────────────────────
   Receipt overlay — shown after a completed transaction
   ───────────────────────────────────────────────────────────── */
internal void
draw_overlay_receipt(AppState *s, SDL_Renderer *r, int w, int h)
{
    Transaction *t = &s->last_txn;

    int pw = 460, ph = 540;
    SDL_Rect panel = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 210);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_panel(r, panel, (SDL_Color){30,36,30,255}, (SDL_Color){80,200,80,255});

    int x = panel.x + 16;
    int y = panel.y + 12;
    int lh = 28;
    char buf[128];

    draw_text(r, s->font, "--- RECEIPT ---",
              x, y, (SDL_Color){80, 255, 80, 255}); y += lh + 4;

    draw_text(r, s->font, t->receipt_ref,
              x, y, (SDL_Color){200, 200, 200, 255}); y += lh;

    /* Timestamp */
    time_t ts = (time_t)t->timestamp;
    char   time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&ts));
    draw_text(r, s->font, time_str,
              x, y, (SDL_Color){160, 160, 160, 255}); y += lh + 4;

    /* Line items */
    for (i32 i = 0; i < t->item_count; i++) {
        Product *p = product_by_id(s, t->items[i].product_id);
        const char *name = p ? p->key_name : "?";
        snprintf(buf, sizeof(buf), "%s x%d  Rs %.0f",
                 name, t->items[i].qty, t->items[i].line_total);
        draw_text(r, s->font, buf,
                  x, y, (SDL_Color){200, 200, 200, 255}); y += lh;
    }

    y += 4;
    SDL_SetRenderDrawColor(r, 80, 100, 80, 255);
    SDL_RenderDrawLine(r, x, y, panel.x + pw - 16, y); y += 8;

    if (t->discount_total > 0.0f) {
        snprintf(buf, sizeof(buf), "Discount: -Rs %.2f", t->discount_total);
        draw_text(r, s->font, buf, x, y, (SDL_Color){255,160,80,255}); y += lh;
    }

    snprintf(buf, sizeof(buf), "TOTAL:    Rs %.2f", t->grand_total);
    draw_text(r, s->font, buf, x, y, (SDL_Color){80, 255, 80, 255}); y += lh;

    if (t->payment_method == PAY_CASH) {
        snprintf(buf, sizeof(buf), "Cash:     Rs %.2f", t->tendered);
        draw_text(r, s->font, buf, x, y, (SDL_Color){200,200,200,255}); y += lh;
        snprintf(buf, sizeof(buf), "Change:   Rs %.2f", t->change_due);
        draw_text(r, s->font, buf, x, y, (SDL_Color){80,230,255,255}); y += lh;
    } else {
        draw_text(r, s->font, "Paid by CARD", x, y, (SDL_Color){200,200,200,255}); y += lh;
    }

    y += 8;

    int bw = 180, bh = 52, gap = 16;
    SDL_Rect close_btn = {panel.x + (pw - bw) / 2, panel.y + ph - bh - gap, bw, bh};

    if (draw_button(r, s->font, close_btn, "New Sale",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->ui_state.overlay      = OVERLAY_NONE;
        s->ui_state.current_view = VIEW_CATEGORIES;
    }
    (void)y;
}

/* ─────────────────────────────────────────────────────────────
   Alert / toast notification
   ───────────────────────────────────────────────────────────── */
void ui_show_alert(UIState *ui, const char *msg, u32 duration_ms)
{
    snprintf(ui->alert_msg, sizeof(ui->alert_msg), "%s", msg);
    ui->alert_ttl_ms = duration_ms;
}

internal void
draw_alert_toast(AppState *s, SDL_Renderer *r, int w, u32 dt_ms)
{
    if (s->ui_state.alert_ttl_ms == 0) return;

    /* Tick down */
    if (dt_ms >= s->ui_state.alert_ttl_ms) {
        s->ui_state.alert_ttl_ms = 0;
        return;
    }
    s->ui_state.alert_ttl_ms -= dt_ms;

    /* Fade out in last 500 ms */
    u8 alpha = 220;
    if (s->ui_state.alert_ttl_ms < 500)
        alpha = (u8)(220 * s->ui_state.alert_ttl_ms / 500);

    int pw = 440, ph = 72;
    SDL_Rect panel = {(w - pw) / 2, 20, pw, ph};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 180, 40, 40, alpha);
    SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawColor(r, 255, 120, 120, alpha);
    SDL_RenderDrawRect(r, &panel);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_text(r, s->font, s->ui_state.alert_msg,
              panel.x + 16, panel.y + (ph - 20) / 2,
              (SDL_Color){255, 255, 255, 255});
}

/* ─────────────────────────────────────────────────────────────
   LOGIN SCREEN
   ───────────────────────────────────────────────────────────── */

internal void
draw_screen_login(AppState *s, SDL_Renderer *r, int w, int h)
{
    /* Dark background */
    SDL_SetRenderDrawColor(r, 14, 16, 24, 255);
    SDL_Rect bg = {0, 0, w, h};
    SDL_RenderFillRect(r, &bg);

    /* Store name */
    draw_text(r, s->font, s->db.path[0] ? "Point of Sale" : "Point of Sale",
              w / 2 - 80, 36, (SDL_Color){120, 150, 200, 255});

    /* Centred numpad */
    int pw = 380, ph = 520;
    SDL_Rect pad = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    i8 res = draw_numpad(s, r, pad, "Enter your PIN",
                         s->ui_state.numpad.buf,
                         &s->ui_state.numpad.len,
                         (i32)sizeof(s->ui_state.numpad.buf));

    if (res == 1) {
        /* Look up which user owns this PIN */
        const User *u = auth_find_by_pin(s->users, s->user_count,
                                         s->ui_state.numpad.buf);
        if (u) {
            s->session.logged_in     = 1;
            s->session.user          = *u;
            s->session.login_time_ms = SDL_GetTicks();
            s->ui_state.screen       = SCREEN_POS;
            textbuf_clear(&s->ui_state.numpad);
        } else {
            textbuf_clear(&s->ui_state.numpad);
            ui_show_alert(&s->ui_state, "Unknown PIN — try again", 2500);
        }
    }
    /* Cancel on the login numpad does nothing (no way back from login) */
}

/* ─────────────────────────────────────────────────────────────
   POS SCREEN (existing logic, now behind login)
   ───────────────────────────────────────────────────────────── */

/* Toolbar at the top of POS screen: shows user, logout, edit-mode */
internal void
draw_pos_toolbar(AppState *s, SDL_Renderer *r, int w)
{
    SDL_Rect bar = {0, 0, w, 44};
    SDL_SetRenderDrawColor(r, 30, 30, 45, 255);
    SDL_RenderFillRect(r, &bar);

    char user_label[96];
    snprintf(user_label, sizeof(user_label), "User: %s (%s)",
             s->session.user.username,
             s->session.user.role == ROLE_ADMIN ? "Admin" : "Cashier");
    draw_text(r, s->font, user_label, 12, 12, (SDL_Color){160, 200, 255, 255});

    int bw = 120, bh = 34, gap = 8;

    /* Logout button */
    SDL_Rect logout_btn = {w - bw - gap, 5, bw, bh};
    if (draw_button(r, s->font, logout_btn, "Log Out",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        auth_logout(&s->session);
        s->ui_state.screen  = SCREEN_LOGIN;
        s->ui_state.overlay = OVERLAY_NONE;
        s->cart_items_count = 0;
        /* Reload user list in case it changed while logged in */
        if (db_is_open(&s->db))
            s->user_count = db_load_users(&s->db, s->users, MAX_USERS);
    }

    /* Edit Mode button — admin only */
    if (s->session.user.role == ROLE_ADMIN) {
        SDL_Rect edit_btn = {w - (bw + gap) * 2, 5, bw, bh};
        if (draw_button_ex(r, s->font, edit_btn, "Edit Mode",
                           (SDL_Color){80, 40, 120, 255},
                           (SDL_Color){220, 180, 255, 255},
                           s->input_state.mouse_x,
                           s->input_state.mouse_y,
                           s->input_state.mouse_pressed)) {
            s->ui_state.screen     = SCREEN_EDIT;
            s->ui_state.overlay    = OVERLAY_NONE;
            s->ui_state.edit_panel = EDIT_PANEL_CATEGORIES;
        }
    }
}

internal void
draw_screen_pos(AppState *s, SDL_Renderer *r, int w, int h, u32 dt_ms)
{
    /* toolbar height = 44, shrink layout area */
    int toolbar_h = 44;
    int content_h = h - toolbar_h;

    /* Recompute layout in a shifted rect */
    Layout L;
    {
        int lw = w, lh = content_h;
        /* layout.c computes from (0,0) — offset y after */
        L = mark_layout_bounds(lw, lh);
        L.products.y  += toolbar_h;
        L.cart.y      += toolbar_h;
        L.totals.y    += toolbar_h;
        L.payments.y  += toolbar_h;
    }
    s->ui_layout = L;

#if DEBUG
    draw_panel(r, L.products, (SDL_Color){40,40,40,255}, (SDL_Color){70,70,70,255});
    draw_panel(r, L.cart,     (SDL_Color){35,35,35,255}, (SDL_Color){70,70,70,255});
    draw_panel(r, L.totals,   (SDL_Color){30,30,30,255}, (SDL_Color){70,70,70,255});
    draw_panel(r, L.payments, (SDL_Color){25,25,25,255}, (SDL_Color){70,70,70,255});
#endif

    draw_pos_toolbar(s, r, w);

    /* Mouse-wheel scroll */
    if (s->input_state.scroll_y != 0) {
        int mx = s->input_state.mouse_x;
        int my = s->input_state.mouse_y;
        int delta = s->input_state.scroll_y * 40;

        if (mx >= L.products.x && mx < L.products.x + L.products.w &&
            my >= L.products.y && my < L.products.y + L.products.h) {
            s->ui_state.product_scroll_y += delta;
            if (s->ui_state.product_scroll_y < 0) s->ui_state.product_scroll_y = 0;
        } else if (mx >= L.cart.x && mx < L.cart.x + L.cart.w &&
                   my >= L.cart.y && my < L.cart.y + L.cart.h) {
            s->ui_state.cart_scroll_y += delta;
            if (s->ui_state.cart_scroll_y < 0) s->ui_state.cart_scroll_y = 0;
        }
        s->input_state.scroll_y = 0;
    }

    if (s->ui_state.current_view == VIEW_CATEGORIES)
        draw_category_grid(s, r, L.products);
    else
        draw_product_grid(s, r, L.products);

    draw_cart(s, r, L.cart);
    draw_totals(s, r, L.totals);
    draw_payment(s, r, L.payments);

    /* Overlays */
    switch (s->ui_state.overlay) {
        case OVERLAY_CASH_PAYMENT: draw_overlay_cash(s, r, w, h); break;
        case OVERLAY_CARD_PAYMENT: draw_overlay_card(s, r, w, h); break;
        case OVERLAY_RECEIPT:      draw_overlay_receipt(s, r, w, h); break;
        default: break;
    }

    draw_alert_toast(s, r, w, dt_ms);
}

/* ─────────────────────────────────────────────────────────────
   EDIT MODE SCREEN
   ───────────────────────────────────────────────────────────── */

/* Helper: load edit fields from a Category */
internal void
edit_load_category(UIState *ui, const Category *c)
{
    textbuf_clear(&ui->edit_name);
    textbuf_append(&ui->edit_name, c->category_name);
    ui->edit_btn_r = c->btn_color.r;
    ui->edit_btn_g = c->btn_color.g;
    ui->edit_btn_b = c->btn_color.b;
    ui->edit_txt_r = c->text_color.r;
    ui->edit_txt_g = c->text_color.g;
    ui->edit_txt_b = c->text_color.b;
}

/* Helper: load edit fields from a Product */
internal void
edit_load_product(UIState *ui, const Product *p)
{
    textbuf_clear(&ui->edit_name);
    textbuf_append(&ui->edit_name, p->key_name);
    char pricebuf[32];
    snprintf(pricebuf, sizeof(pricebuf), "%.2f", p->price);
    textbuf_clear(&ui->edit_price);
    textbuf_append(&ui->edit_price, pricebuf);
    ui->edit_category_id = p->category_id;
    ui->edit_btn_r = p->btn_color.r;
    ui->edit_btn_g = p->btn_color.g;
    ui->edit_btn_b = p->btn_color.b;
    ui->edit_txt_r = p->text_color.r;
    ui->edit_txt_g = p->text_color.g;
    ui->edit_txt_b = p->text_color.b;
}

/* ── Text-input row: label + on-screen keyboard via SDL_StartTextInput ── */
/* For now: a simple "virtual keyboard" approach using a small on-screen
   row of clickable chars + backspace is out of scope for this pass.
   Instead we draw the current buffer and accept SDL keyboard input
   via a simple flag (text input fields highlight when selected). */

/* Draw a labelled text field.
 *
 * Touchscreen mode (s->input_touchscreen == 1):
 *   Tapping sets ui->active_text_buf = tb and opens the OSK.
 *   The OSK itself writes characters; this widget just shows them.
 *
 * Keyboard mode (s->input_touchscreen == 0):
 *   Tapping focuses this field (sets ui->active_text_buf = tb and
 *   calls SDL_StartTextInput).  SDL TEXTINPUT / KEYDOWN events are
 *   consumed in immediate_mode_ui_draw before any drawing happens.
 *
 * Returns 1 the frame this field becomes focused.
 */
internal i8
draw_text_field(AppState *s, SDL_Renderer *r,
                int x, int y, int w, int h,
                const char *label, TextBuf *tb)
{
    UIState *ui       = &s->ui_state;
    i8       is_active = (ui->active_text_buf == tb);

    /* Label */
    draw_text(r, s->font, label, x, y - 20, (SDL_Color){180, 180, 180, 255});

    SDL_Rect field = {x, y, w, h};
    SDL_Color border = is_active ? (SDL_Color){100, 180, 255, 255}
                                 : (SDL_Color){ 80,  80,  80, 255};
    draw_panel(r, field, (SDL_Color){40, 44, 52, 255}, border);

    /* Content + blinking cursor */
    char display[TEXT_BUF_LEN + 2];
    if (tb->len == 0 && !is_active)
        snprintf(display, sizeof(display), " ");
    else
        snprintf(display, sizeof(display), "%s%s", tb->buf, is_active ? "_" : "");
    draw_text(r, s->font, display, x + 6, y + (h - 20) / 2,
              (SDL_Color){220, 220, 220, 255});

    /* Tap / click detection */
    i8 hot = s->input_state.mouse_x >= x && s->input_state.mouse_x <= x + w &&
             s->input_state.mouse_y >= y && s->input_state.mouse_y <= y + h;
    i8 tapped = hot && s->input_state.mouse_pressed;

    if (tapped && !is_active) {
        ui->active_text_buf = tb;
        if (!s->input_touchscreen) {
            /* Hardware keyboard mode: let SDL route text events here */
            SDL_StartTextInput();
        }
        return 1;
    }

    return 0;
}

/* ── Colour swatch button ── */
internal void
draw_colour_swatch(SDL_Renderer *r, int x, int y, int sz,
                   u8 cr, u8 cg, u8 cb)
{
    SDL_Rect sq = {x, y, sz, sz};
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderFillRect(r, &sq);
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderDrawRect(r, &sq);
}

/* ── Colour picker overlay: RGB sliders ── */
internal void
draw_overlay_colour_picker(AppState *s, SDL_Renderer *r, int w, int h)
{
    int pw = 400, ph = 340;
    SDL_Rect panel = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_panel(r, panel, (SDL_Color){35, 38, 48, 255}, (SDL_Color){100, 180, 255, 255});

    int x = panel.x + 16;
    int y = panel.y + 16;
    const char *title = (s->ui_state.colour_pick.target == 0)
                        ? "Button Colour" : "Text Colour";
    draw_text(r, s->font, title, x, y, (SDL_Color){200, 220, 255, 255});
    y += 32;

    u8 *rp, *gp, *bp;
    if (s->ui_state.colour_pick.target == 0) {
        rp = &s->ui_state.edit_btn_r;
        gp = &s->ui_state.edit_btn_g;
        bp = &s->ui_state.edit_btn_b;
    } else {
        rp = &s->ui_state.edit_txt_r;
        gp = &s->ui_state.edit_txt_g;
        bp = &s->ui_state.edit_txt_b;
    }

    /* For each channel: [-] label [+] bar */
    const char *labels[] = {"R:", "G:", "B:"};
    u8 *channels[] = {rp, gp, bp};
    SDL_Color bar_colours[] = {
        {200, 60,  60,  255},
        {60,  200, 60,  255},
        {60,  60,  200, 255}
    };

    for (int ci = 0; ci < 3; ci++) {
        draw_text(r, s->font, labels[ci], x, y + 10, (SDL_Color){200,200,200,255});

        SDL_Rect minus = {x + 24, y, 36, 36};
        SDL_Rect plus  = {x + 24 + 36 + 8, y, 36, 36};

        if (draw_button(r, s->font, minus, "-",
                        s->input_state.mouse_x, s->input_state.mouse_y,
                        s->input_state.mouse_pressed)) {
            if (*channels[ci] >= 16) *channels[ci] -= 16;
            else *channels[ci] = 0;
        }
        if (draw_button(r, s->font, plus, "+",
                        s->input_state.mouse_x, s->input_state.mouse_y,
                        s->input_state.mouse_pressed)) {
            if (*channels[ci] <= 239) *channels[ci] += 16;
            else *channels[ci] = 255;
        }

        /* Colour bar showing current value */
        int bar_x = x + 24 + 36 + 8 + 36 + 12;
        int bar_w = pw - (bar_x - panel.x) - 16 - 28;
        SDL_Rect bar_bg = {bar_x, y + 8, bar_w, 20};
        SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
        SDL_RenderFillRect(r, &bar_bg);
        int fill_w = (int)((float)bar_w * (*channels[ci]) / 255.0f);
        SDL_Rect bar_fill = {bar_x, y + 8, fill_w, 20};
        SDL_SetRenderDrawColor(r, bar_colours[ci].r, bar_colours[ci].g,
                               bar_colours[ci].b, 255);
        SDL_RenderFillRect(r, &bar_fill);

        /* Numeric label */
        char vbuf[8]; snprintf(vbuf, sizeof(vbuf), "%d", *channels[ci]);
        draw_text(r, s->font, vbuf, bar_x + bar_w + 4, y + 8,
                  (SDL_Color){200, 200, 200, 255});

        y += 52;
    }

    /* Preview swatch */
    draw_colour_swatch(r, x, y + 8, 48, *rp, *gp, *bp);
    draw_text(r, s->font, "Preview", x + 56, y + 20, (SDL_Color){160,160,160,255});
    y += 64;

    int bw = 120, bh = 40, gap = 16;
    SDL_Rect ok  = {panel.x + gap, panel.y + ph - bh - gap, bw, bh};
    SDL_Rect can = {panel.x + pw - bw - gap, panel.y + ph - bh - gap, bw, bh};

    if (draw_button(r, s->font, ok, "OK",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->ui_state.overlay = OVERLAY_NONE;
    }
    if (draw_button(r, s->font, can, "Cancel",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->ui_state.overlay = OVERLAY_NONE;
    }
    (void)y;
}

/* ── Confirm-delete overlay ── */
internal void
draw_overlay_confirm_delete(AppState *s, SDL_Renderer *r, int w, int h)
{
    int pw = 420, ph = 180;
    SDL_Rect panel = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_panel(r, panel, (SDL_Color){50, 30, 30, 255}, (SDL_Color){200, 80, 80, 255});
    draw_text(r, s->font, s->ui_state.confirm_msg,
              panel.x + 16, panel.y + 24, (SDL_Color){255, 200, 200, 255});

    int bw = 140, bh = 44, gap = 16;
    SDL_Rect yes = {panel.x + gap,             panel.y + ph - bh - gap, bw, bh};
    SDL_Rect no  = {panel.x + pw - bw - gap,   panel.y + ph - bh - gap, bw, bh};

    if (draw_button_ex(r, s->font, yes, "Delete",
                       (SDL_Color){160, 30, 30, 255},
                       (SDL_Color){255, 220, 220, 255},
                       s->input_state.mouse_x, s->input_state.mouse_y,
                       s->input_state.mouse_pressed)) {
        /* Perform the deletion */
        if (db_is_open(&s->db)) {
            switch (s->ui_state.confirm_delete_type) {
                case 0: db_delete_category(&s->db, s->ui_state.confirm_delete_id); break;
                case 1: db_delete_product(&s->db, s->ui_state.confirm_delete_id); break;
                case 2: db_delete_discount_preset(&s->db, s->ui_state.confirm_delete_id); break;
                case 3: db_delete_user(&s->db, s->ui_state.confirm_delete_id); break;
                default: break;
            }
            app_reload_menu(s);
            if (s->ui_state.confirm_delete_type == 3)
                s->user_count = db_load_users(&s->db, s->users, MAX_USERS);
        }
        s->ui_state.overlay = OVERLAY_NONE;
    }

    if (draw_button(r, s->font, no, "Cancel",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->ui_state.overlay = OVERLAY_NONE;
    }
}

/* ── Category editor overlay ── */
internal void
draw_overlay_edit_category(AppState *s, SDL_Renderer *r, int w, int h)
{
    int pw = 440, ph = 380;
    SDL_Rect panel = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_panel(r, panel, (SDL_Color){35, 38, 48, 255}, (SDL_Color){100, 180, 255, 255});

    const char *title = (s->ui_state.edit_selected_id == 0) ? "New Category" : "Edit Category";
    draw_text(r, s->font, title,
              panel.x + 16, panel.y + 16, (SDL_Color){200, 220, 255, 255});

    UIState *ui = &s->ui_state;
    int fx = panel.x + 16, fw = pw - 32;
    int y = panel.y + 52;

    /* Name field */
    draw_text_field(s, r, fx, y, fw, 40, "Category Name", &ui->edit_name);
    y += 70;

    /* Colour swatches + edit buttons */
    draw_text(r, s->font, "Button Colour:", fx, y, (SDL_Color){160,160,160,255});
    draw_colour_swatch(r, fx + 140, y, 32,
                       ui->edit_btn_r, ui->edit_btn_g, ui->edit_btn_b);
    SDL_Rect edit_btn_col = {fx + 180, y - 4, 100, 36};
    if (draw_button(r, s->font, edit_btn_col, "Change",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ui->colour_pick.target = 0;
        ui->overlay = OVERLAY_EDIT_COLOUR;
    }
    y += 48;

    draw_text(r, s->font, "Text Colour:", fx, y, (SDL_Color){160,160,160,255});
    draw_colour_swatch(r, fx + 140, y, 32,
                       ui->edit_txt_r, ui->edit_txt_g, ui->edit_txt_b);
    SDL_Rect edit_txt_col = {fx + 180, y - 4, 100, 36};
    if (draw_button(r, s->font, edit_txt_col, "Change",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ui->colour_pick.target = 1;
        ui->overlay = OVERLAY_EDIT_COLOUR;
    }
    y += 56;

    int bw = 130, bh = 44, gap = 12;
    SDL_Rect save_btn   = {panel.x + gap,              panel.y + ph - bh - gap, bw, bh};
    SDL_Rect cancel_btn = {panel.x + pw - bw - gap,    panel.y + ph - bh - gap, bw, bh};

    if (draw_button_ex(r, s->font, save_btn, "Save",
                       (SDL_Color){30, 100, 50, 255},
                       (SDL_Color){200, 255, 210, 255},
                       s->input_state.mouse_x, s->input_state.mouse_y,
                       s->input_state.mouse_pressed)) {
        if (ui->edit_name.len > 0) {
            Category c = {0};
            c.id = ui->edit_selected_id;
            snprintf(c.category_name, sizeof(c.category_name), "%s", ui->edit_name.buf);
            c.btn_color  = COLOR4(ui->edit_btn_r, ui->edit_btn_g, ui->edit_btn_b);
            c.text_color = COLOR4(ui->edit_txt_r, ui->edit_txt_g, ui->edit_txt_b);
            c.active     = 1;
            if (db_is_open(&s->db)) {
                db_save_category(&s->db, &c);
                app_reload_menu(s);
            }
            ui->active_text_buf = NULL;
            ui->overlay = OVERLAY_NONE;
        } else {
            ui_show_alert(ui, "Name cannot be empty", 2000);
        }
    }

    if (draw_button(r, s->font, cancel_btn, "Cancel",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ui->active_text_buf = NULL;
        ui->overlay = OVERLAY_NONE;
    }
    (void)y;
}

/* ── Product editor overlay ── */
internal void
draw_overlay_edit_product(AppState *s, SDL_Renderer *r, int w, int h)
{
    int pw = 480, ph = 520;
    SDL_Rect panel = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_panel(r, panel, (SDL_Color){35, 38, 48, 255}, (SDL_Color){100, 180, 255, 255});

    const char *title = (s->ui_state.edit_selected_id == 0) ? "New Product" : "Edit Product";
    draw_text(r, s->font, title,
              panel.x + 16, panel.y + 16, (SDL_Color){200, 220, 255, 255});

    UIState *ui = &s->ui_state;
    int fx = panel.x + 16, fw = pw - 32;
    int y = panel.y + 52;

    draw_text_field(s, r, fx, y, fw, 40, "Product Name", &ui->edit_name);
    y += 70;

    draw_text_field(s, r, fx, y, fw / 2, 40, "Price (Rs)", &ui->edit_price);
    y += 70;

    /* Category selector */
    draw_text(r, s->font, "Category:", fx, y, (SDL_Color){160,160,160,255});
    int cat_bw = 140, cat_bh = 36, cat_gap = 6;
    int cx = fx + 100;
    for (i32 ci = 0; ci < s->category_count; ci++) {
        SDL_Color fill = (s->categories[ci].id == ui->edit_category_id)
            ? (SDL_Color){60, 120, 180, 255}
            : (SDL_Color){50, 50, 70, 255};
        SDL_Rect cb = {cx, y - 6, cat_bw, cat_bh};
        if (draw_button_ex(r, s->font, cb,
                           s->categories[ci].category_name,
                           fill, (SDL_Color){220, 220, 255, 255},
                           s->input_state.mouse_x, s->input_state.mouse_y,
                           s->input_state.mouse_pressed)) {
            ui->edit_category_id = s->categories[ci].id;
        }
        cx += cat_bw + cat_gap;
        if (cx + cat_bw > panel.x + pw - 16) { cx = fx + 100; y += cat_bh + cat_gap; }
    }
    y += cat_bh + 16;

    /* Colours */
    draw_text(r, s->font, "Button Colour:", fx, y, (SDL_Color){160,160,160,255});
    draw_colour_swatch(r, fx + 140, y, 32,
                       ui->edit_btn_r, ui->edit_btn_g, ui->edit_btn_b);
    SDL_Rect ebc = {fx + 180, y - 4, 100, 36};
    if (draw_button(r, s->font, ebc, "Change",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ui->colour_pick.target = 0; ui->overlay = OVERLAY_EDIT_COLOUR;
    }
    y += 48;

    draw_text(r, s->font, "Text Colour:", fx, y, (SDL_Color){160,160,160,255});
    draw_colour_swatch(r, fx + 140, y, 32,
                       ui->edit_txt_r, ui->edit_txt_g, ui->edit_txt_b);
    SDL_Rect etc = {fx + 180, y - 4, 100, 36};
    if (draw_button(r, s->font, etc, "Change",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ui->colour_pick.target = 1; ui->overlay = OVERLAY_EDIT_COLOUR;
    }
    y += 56;

    int bw = 130, bh = 44, gap = 12;
    SDL_Rect save_btn   = {panel.x + gap,              panel.y + ph - bh - gap, bw, bh};
    SDL_Rect cancel_btn = {panel.x + pw - bw - gap,    panel.y + ph - bh - gap, bw, bh};

    if (draw_button_ex(r, s->font, save_btn, "Save",
                       (SDL_Color){30, 100, 50, 255},
                       (SDL_Color){200, 255, 210, 255},
                       s->input_state.mouse_x, s->input_state.mouse_y,
                       s->input_state.mouse_pressed)) {
        if (ui->edit_name.len > 0) {
            Product p = {0};
            p.id          = ui->edit_selected_id;
            p.category_id = ui->edit_category_id;
            snprintf(p.key_name, sizeof(p.key_name), "%s", ui->edit_name.buf);
            p.price       = (float)atof(ui->edit_price.buf);
            p.btn_color   = COLOR4(ui->edit_btn_r, ui->edit_btn_g, ui->edit_btn_b);
            p.text_color  = COLOR4(ui->edit_txt_r, ui->edit_txt_g, ui->edit_txt_b);
            p.active      = 1;
            if (db_is_open(&s->db)) {
                db_save_product(&s->db, &p);
                app_reload_menu(s);
            }
            ui->active_text_buf = NULL;
            ui->overlay = OVERLAY_NONE;
        } else {
            ui_show_alert(ui, "Name cannot be empty", 2000);
        }
    }

    if (draw_button(r, s->font, cancel_btn, "Cancel",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ui->active_text_buf = NULL;
        ui->overlay = OVERLAY_NONE;
    }
    (void)y;
}

/* ── Discount preset editor overlay ── */
internal void
draw_overlay_edit_discount(AppState *s, SDL_Renderer *r, int w, int h)
{
    int pw = 380, ph = 260;
    SDL_Rect panel = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_panel(r, panel, (SDL_Color){35, 38, 48, 255}, (SDL_Color){100, 180, 255, 255});

    const char *title = (s->ui_state.edit_selected_id == 0)
                        ? "New Discount" : "Edit Discount";
    draw_text(r, s->font, title,
              panel.x + 16, panel.y + 16, (SDL_Color){200, 220, 255, 255});

    UIState *ui = &s->ui_state;
    int fx = panel.x + 16, fw = pw - 32;
    int y = panel.y + 52;

    draw_text_field(s, r, fx, y, fw, 40, "Name (e.g. Staff 10%)", &ui->edit_name);
    y += 70;

    draw_text_field(s, r, fx, y, fw / 2, 40, "Percent (0-100)", &ui->edit_price);
    y += 70;

    int bw = 130, bh = 44, gap = 12;
    SDL_Rect save_btn   = {panel.x + gap,              panel.y + ph - bh - gap, bw, bh};
    SDL_Rect cancel_btn = {panel.x + pw - bw - gap,    panel.y + ph - bh - gap, bw, bh};

    if (draw_button_ex(r, s->font, save_btn, "Save",
                       (SDL_Color){30, 100, 50, 255},
                       (SDL_Color){200, 255, 210, 255},
                       s->input_state.mouse_x, s->input_state.mouse_y,
                       s->input_state.mouse_pressed)) {
        if (ui->edit_name.len > 0) {
            DiscountPreset dp = {0};
            dp.id      = ui->edit_selected_id;
            snprintf(dp.name, sizeof(dp.name), "%s", ui->edit_name.buf);
            dp.percent = (float)atof(ui->edit_price.buf);
            dp.active  = 1;
            if (db_is_open(&s->db)) {
                db_save_discount_preset(&s->db, &dp);
                app_reload_menu(s);
            }
            ui->active_text_buf = NULL;
            ui->overlay = OVERLAY_NONE;
        } else {
            ui_show_alert(ui, "Name cannot be empty", 2000);
        }
    }

    if (draw_button(r, s->font, cancel_btn, "Cancel",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ui->active_text_buf = NULL;
        ui->overlay = OVERLAY_NONE;
    }
    (void)y;
}

/* ── User editor overlay ── */
internal void
draw_overlay_edit_user(AppState *s, SDL_Renderer *r, int w, int h)
{
    int pw = 400, ph = 360;
    SDL_Rect panel = {(w - pw) / 2, (h - ph) / 2, pw, ph};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_Rect full = {0, 0, w, h};
    SDL_RenderFillRect(r, &full);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    draw_panel(r, panel, (SDL_Color){35, 38, 48, 255}, (SDL_Color){100, 180, 255, 255});

    const char *title = (s->ui_state.edit_selected_id == 0) ? "New User" : "Edit User";
    draw_text(r, s->font, title,
              panel.x + 16, panel.y + 16, (SDL_Color){200, 220, 255, 255});

    UIState *ui = &s->ui_state;
    int fx = panel.x + 16, fw = pw - 32;
    int y = panel.y + 52;

    draw_text_field(s, r, fx, y, fw, 40, "Username", &ui->edit_name);
    y += 70;

    /* PIN field: input goes to ui->edit_pin but display shows asterisks.
       We draw the field manually and wire the tap to set active_text_buf. */
    {
        i8 pin_focused = (ui->active_text_buf == &ui->edit_pin);
        draw_text(r, s->font, "PIN (digits)", fx, y - 20, (SDL_Color){180,180,180,255});
        SDL_Rect pfield = {fx, y, fw / 2, 40};
        SDL_Color pborder = pin_focused ? (SDL_Color){100,180,255,255}
                                        : (SDL_Color){ 80, 80, 80,255};
        draw_panel(r, pfield, (SDL_Color){40,44,52,255}, pborder);
        /* Show asterisks for entered chars */
        char stars[TEXT_BUF_LEN + 2] = {0};
        for (i32 pi = 0; pi < ui->edit_pin.len && pi < TEXT_BUF_LEN; pi++)
            stars[pi] = '*';
        if (pin_focused) { stars[ui->edit_pin.len] = '_'; stars[ui->edit_pin.len+1] = '\0'; }
        if (stars[0] == '\0') snprintf(stars, sizeof(stars), " ");
        draw_text(r, s->font, stars, fx + 6, y + (40 - 20) / 2,
                  (SDL_Color){220,220,220,255});
        /* Tap to focus */
        i8 phot = s->input_state.mouse_x >= fx && s->input_state.mouse_x <= fx + fw/2 &&
                  s->input_state.mouse_y >= y  && s->input_state.mouse_y <= y + 40;
        if (phot && s->input_state.mouse_pressed && !pin_focused) {
            ui->active_text_buf = &ui->edit_pin;
            if (!s->input_touchscreen) SDL_StartTextInput();
        }
    }
    y += 70;

    /* Role selector */
    draw_text(r, s->font, "Role:", fx, y + 8, (SDL_Color){160,160,160,255});
    SDL_Rect cashier_btn = {fx + 70,  y, 120, 36};
    SDL_Rect admin_btn   = {fx + 200, y, 120, 36};

    SDL_Color cashier_fill = (ui->edit_role == ROLE_CASHIER)
        ? (SDL_Color){40, 100, 160, 255} : (SDL_Color){50,50,70,255};
    SDL_Color admin_fill   = (ui->edit_role == ROLE_ADMIN)
        ? (SDL_Color){80, 40, 120, 255}  : (SDL_Color){50,50,70,255};

    if (draw_button_ex(r, s->font, cashier_btn, "Cashier",
                       cashier_fill, (SDL_Color){220,220,255,255},
                       s->input_state.mouse_x, s->input_state.mouse_y,
                       s->input_state.mouse_pressed))
        ui->edit_role = ROLE_CASHIER;

    if (draw_button_ex(r, s->font, admin_btn, "Admin",
                       admin_fill, (SDL_Color){220,200,255,255},
                       s->input_state.mouse_x, s->input_state.mouse_y,
                       s->input_state.mouse_pressed))
        ui->edit_role = ROLE_ADMIN;
    y += 56;

    int bw = 130, bh = 44, gap = 12;
    SDL_Rect save_btn   = {panel.x + gap,              panel.y + ph - bh - gap, bw, bh};
    SDL_Rect cancel_btn = {panel.x + pw - bw - gap,    panel.y + ph - bh - gap, bw, bh};

    if (draw_button_ex(r, s->font, save_btn, "Save",
                       (SDL_Color){30, 100, 50, 255},
                       (SDL_Color){200, 255, 210, 255},
                       s->input_state.mouse_x, s->input_state.mouse_y,
                       s->input_state.mouse_pressed)) {
        if (ui->edit_name.len > 0) {
            User u = {0};
            u.id   = ui->edit_selected_id;
            snprintf(u.username, sizeof(u.username), "%s", ui->edit_name.buf);
            if (ui->edit_pin.len > 0) {
                auth_hash_pin(ui->edit_pin.buf, u.pin_hash);
            } else if (ui->edit_selected_id != 0) {
                /* keep existing hash — lookup from current users list */
                for (i32 i = 0; i < s->user_count; i++) {
                    if (s->users[i].id == ui->edit_selected_id) {
                        snprintf(u.pin_hash, sizeof(u.pin_hash), "%s",
                                 s->users[i].pin_hash);
                        break;
                    }
                }
            }
            u.role   = (UserRole)ui->edit_role;
            u.active = 1;
            if (db_is_open(&s->db)) {
                db_save_user(&s->db, &u);
                s->user_count = db_load_users(&s->db, s->users, MAX_USERS);
            }
            ui->active_text_buf = NULL;
            ui->overlay = OVERLAY_NONE;
        } else {
            ui_show_alert(ui, "Username cannot be empty", 2000);
        }
    }

    if (draw_button(r, s->font, cancel_btn, "Cancel",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        ui->active_text_buf = NULL;
        ui->overlay = OVERLAY_NONE;
    }
    (void)y;
}

/* ── Edit-mode text input: handle SDL keyboard events ── */
/* Called from the main loop before drawing. Routes keypresses to the
   active TextBuf field based on the current overlay. */
/* (For now we rely on the text fields showing a cursor; actual SDL
   text input events are forwarded via input_handle_event which does
   nothing with them yet — this is a TODO for hardware keyboard support.
   On a touch screen the on-screen numpad handles numeric fields.) */

/* ── Edit mode main screen ── */
internal void
draw_screen_edit(AppState *s, SDL_Renderer *r, int w, int h, u32 dt_ms)
{
    /* Background */
    SDL_SetRenderDrawColor(r, 20, 20, 32, 255);
    SDL_Rect bg = {0, 0, w, h};
    SDL_RenderFillRect(r, &bg);

    /* Header bar */
    SDL_Rect header = {0, 0, w, 50};
    SDL_SetRenderDrawColor(r, 30, 28, 50, 255);
    SDL_RenderFillRect(r, &header);

    draw_text(r, s->font, "Edit Mode",
              16, 14, (SDL_Color){200, 180, 255, 255});

    /* Back to POS button */
    int bw = 140, bh = 38, gap = 10;
    SDL_Rect back_btn = {w - bw - gap, 6, bw, bh};
    if (draw_button(r, s->font, back_btn, "< Back to POS",
                    s->input_state.mouse_x, s->input_state.mouse_y,
                    s->input_state.mouse_pressed)) {
        s->ui_state.screen  = SCREEN_POS;
        s->ui_state.overlay = OVERLAY_NONE;
    }

    /* Tab bar */
    int tab_y = 56;
    int tab_h = 40;
    int tab_w = 160;
    const char *tab_labels[] = {"Categories", "Products", "Discounts", "Users"};
    EditPanel   tab_panels[] = {EDIT_PANEL_CATEGORIES, EDIT_PANEL_PRODUCTS,
                                EDIT_PANEL_DISCOUNTS,  EDIT_PANEL_USERS};
    for (int ti = 0; ti < 4; ti++) {
        SDL_Rect tb = {gap + ti * (tab_w + gap), tab_y, tab_w, tab_h};
        i8 active = (s->ui_state.edit_panel == tab_panels[ti]);
        SDL_Color fill = active ? (SDL_Color){60, 80, 140, 255}
                                : (SDL_Color){40, 44, 60,  255};
        if (draw_button_ex(r, s->font, tb, tab_labels[ti],
                           fill, (SDL_Color){220, 220, 255, 255},
                           s->input_state.mouse_x, s->input_state.mouse_y,
                           s->input_state.mouse_pressed)) {
            s->ui_state.edit_panel = tab_panels[ti];
        }
    }

    /* Content area below tabs */
    int content_y = tab_y + tab_h + 12;
    int list_x    = gap;
    int row_h     = 50;
    int row_gap   = 6;
    int add_bw    = 120, add_bh = 36;

    /* ── CATEGORIES panel ── */
    if (s->ui_state.edit_panel == EDIT_PANEL_CATEGORIES) {
        /* Add button */
        SDL_Rect add_btn = {w - add_bw - gap, content_y, add_bw, add_bh};
        if (draw_button_ex(r, s->font, add_btn, "+ Add",
                           (SDL_Color){30, 100, 50, 255},
                           (SDL_Color){200, 255, 210, 255},
                           s->input_state.mouse_x, s->input_state.mouse_y,
                           s->input_state.mouse_pressed)) {
            s->ui_state.edit_selected_id = 0;
            textbuf_clear(&s->ui_state.edit_name);
            s->ui_state.edit_btn_r = 60; s->ui_state.edit_btn_g = 60; s->ui_state.edit_btn_b = 60;
            s->ui_state.edit_txt_r = 255; s->ui_state.edit_txt_g = 255; s->ui_state.edit_txt_b = 255;
            s->ui_state.overlay = OVERLAY_EDIT_CATEGORY;
        }

        int ry = content_y + add_bh + gap;
        for (i32 i = 0; i < s->category_count; i++) {
            SDL_Rect row = {list_x, ry, w - list_x * 2, row_h};
            SDL_Color row_fill = {35, 38, 50, 255};
            draw_panel(r, row, row_fill, (SDL_Color){60, 60, 80, 255});

            /* Colour swatch */
            draw_colour_swatch(r, list_x + 6, ry + 9, 32,
                               s->categories[i].btn_color.r,
                               s->categories[i].btn_color.g,
                               s->categories[i].btn_color.b);

            draw_text(r, s->font, s->categories[i].category_name,
                      list_x + 48, ry + 14, (SDL_Color){220, 220, 220, 255});

            /* Edit button */
            SDL_Rect edit_b = {w - 200, ry + 7, 80, 36};
            if (draw_button(r, s->font, edit_b, "Edit",
                            s->input_state.mouse_x, s->input_state.mouse_y,
                            s->input_state.mouse_pressed)) {
                s->ui_state.edit_selected_id = s->categories[i].id;
                edit_load_category(&s->ui_state, &s->categories[i]);
                s->ui_state.overlay = OVERLAY_EDIT_CATEGORY;
            }

            /* Delete button */
            SDL_Rect del_b = {w - 110, ry + 7, 80, 36};
            if (draw_button_ex(r, s->font, del_b, "Delete",
                               (SDL_Color){120, 30, 30, 255},
                               (SDL_Color){255, 180, 180, 255},
                               s->input_state.mouse_x, s->input_state.mouse_y,
                               s->input_state.mouse_pressed)) {
                s->ui_state.confirm_delete_id   = s->categories[i].id;
                s->ui_state.confirm_delete_type = 0;
                snprintf(s->ui_state.confirm_msg,
                         sizeof(s->ui_state.confirm_msg),
                         "Delete category \"%s\"?",
                         s->categories[i].category_name);
                s->ui_state.overlay = OVERLAY_CONFIRM_DELETE;
            }
            ry += row_h + row_gap;
        }
    }

    /* ── PRODUCTS panel ── */
    else if (s->ui_state.edit_panel == EDIT_PANEL_PRODUCTS) {
        SDL_Rect add_btn = {w - add_bw - gap, content_y, add_bw, add_bh};
        if (draw_button_ex(r, s->font, add_btn, "+ Add",
                           (SDL_Color){30, 100, 50, 255},
                           (SDL_Color){200, 255, 210, 255},
                           s->input_state.mouse_x, s->input_state.mouse_y,
                           s->input_state.mouse_pressed)) {
            s->ui_state.edit_selected_id = 0;
            textbuf_clear(&s->ui_state.edit_name);
            textbuf_clear(&s->ui_state.edit_price);
            s->ui_state.edit_category_id =
                (s->category_count > 0) ? s->categories[0].id : 0;
            s->ui_state.edit_btn_r = 60; s->ui_state.edit_btn_g = 60; s->ui_state.edit_btn_b = 60;
            s->ui_state.edit_txt_r = 255; s->ui_state.edit_txt_g = 255; s->ui_state.edit_txt_b = 255;
            s->ui_state.overlay = OVERLAY_EDIT_PRODUCT;
        }

        int ry = content_y + add_bh + gap;
        SDL_RenderSetClipRect(r, &(SDL_Rect){0, content_y, w, h - content_y});
        for (i32 i = 0; i < s->product_count; i++) {
            SDL_Rect row = {list_x, ry - s->ui_state.product_scroll_y,
                            w - list_x * 2, row_h};
            if (row.y + row.h < content_y || row.y > h) { ry += row_h + row_gap; continue; }
            draw_panel(r, row, (SDL_Color){35, 38, 50, 255}, (SDL_Color){60, 60, 80, 255});

            draw_colour_swatch(r, list_x + 6, row.y + 9, 32,
                               s->products[i].btn_color.r,
                               s->products[i].btn_color.g,
                               s->products[i].btn_color.b);

            char pline[80];
            snprintf(pline, sizeof(pline), "%s  Rs %.0f",
                     s->products[i].key_name, s->products[i].price);
            draw_text(r, s->font, pline, list_x + 48, row.y + 14,
                      (SDL_Color){220, 220, 220, 255});

            SDL_Rect edit_b = {w - 200, row.y + 7, 80, 36};
            if (draw_button(r, s->font, edit_b, "Edit",
                            s->input_state.mouse_x, s->input_state.mouse_y,
                            s->input_state.mouse_pressed)) {
                s->ui_state.edit_selected_id = s->products[i].id;
                edit_load_product(&s->ui_state, &s->products[i]);
                s->ui_state.overlay = OVERLAY_EDIT_PRODUCT;
            }

            SDL_Rect del_b = {w - 110, row.y + 7, 80, 36};
            if (draw_button_ex(r, s->font, del_b, "Delete",
                               (SDL_Color){120, 30, 30, 255},
                               (SDL_Color){255, 180, 180, 255},
                               s->input_state.mouse_x, s->input_state.mouse_y,
                               s->input_state.mouse_pressed)) {
                s->ui_state.confirm_delete_id   = s->products[i].id;
                s->ui_state.confirm_delete_type = 1;
                snprintf(s->ui_state.confirm_msg,
                         sizeof(s->ui_state.confirm_msg),
                         "Delete product \"%s\"?", s->products[i].key_name);
                s->ui_state.overlay = OVERLAY_CONFIRM_DELETE;
            }
            ry += row_h + row_gap;
        }
        SDL_RenderSetClipRect(r, NULL);

        /* scroll */
        if (s->input_state.scroll_y != 0) {
            s->ui_state.product_scroll_y += s->input_state.scroll_y * 40;
            if (s->ui_state.product_scroll_y < 0) s->ui_state.product_scroll_y = 0;
            s->input_state.scroll_y = 0;
        }
    }

    /* ── DISCOUNTS panel ── */
    else if (s->ui_state.edit_panel == EDIT_PANEL_DISCOUNTS) {
        SDL_Rect add_btn = {w - add_bw - gap, content_y, add_bw, add_bh};
        if (draw_button_ex(r, s->font, add_btn, "+ Add",
                           (SDL_Color){30, 100, 50, 255},
                           (SDL_Color){200, 255, 210, 255},
                           s->input_state.mouse_x, s->input_state.mouse_y,
                           s->input_state.mouse_pressed)) {
            s->ui_state.edit_selected_id = 0;
            textbuf_clear(&s->ui_state.edit_name);
            textbuf_clear(&s->ui_state.edit_price);
            s->ui_state.overlay = OVERLAY_EDIT_DISCOUNT;
        }

        int ry = content_y + add_bh + gap;
        for (i32 i = 0; i < s->discount_preset_count; i++) {
            SDL_Rect row = {list_x, ry, w - list_x * 2, row_h};
            draw_panel(r, row, (SDL_Color){35, 38, 50, 255}, (SDL_Color){60, 60, 80, 255});

            char dline[80];
            snprintf(dline, sizeof(dline), "%s  %.0f%%",
                     s->discount_presets[i].name, s->discount_presets[i].percent);
            draw_text(r, s->font, dline, list_x + 12, ry + 14,
                      (SDL_Color){220, 220, 220, 255});

            SDL_Rect edit_b = {w - 200, ry + 7, 80, 36};
            if (draw_button(r, s->font, edit_b, "Edit",
                            s->input_state.mouse_x, s->input_state.mouse_y,
                            s->input_state.mouse_pressed)) {
                s->ui_state.edit_selected_id = s->discount_presets[i].id;
                textbuf_clear(&s->ui_state.edit_name);
                textbuf_append(&s->ui_state.edit_name, s->discount_presets[i].name);
                char pbuf[32];
                snprintf(pbuf, sizeof(pbuf), "%.1f", s->discount_presets[i].percent);
                textbuf_clear(&s->ui_state.edit_price);
                textbuf_append(&s->ui_state.edit_price, pbuf);
                s->ui_state.overlay = OVERLAY_EDIT_DISCOUNT;
            }

            SDL_Rect del_b = {w - 110, ry + 7, 80, 36};
            if (draw_button_ex(r, s->font, del_b, "Delete",
                               (SDL_Color){120, 30, 30, 255},
                               (SDL_Color){255, 180, 180, 255},
                               s->input_state.mouse_x, s->input_state.mouse_y,
                               s->input_state.mouse_pressed)) {
                s->ui_state.confirm_delete_id   = s->discount_presets[i].id;
                s->ui_state.confirm_delete_type = 2;
                snprintf(s->ui_state.confirm_msg,
                         sizeof(s->ui_state.confirm_msg),
                         "Delete discount \"%s\"?", s->discount_presets[i].name);
                s->ui_state.overlay = OVERLAY_CONFIRM_DELETE;
            }
            ry += row_h + row_gap;
        }
    }

    /* ── USERS panel ── */
    else if (s->ui_state.edit_panel == EDIT_PANEL_USERS) {
        SDL_Rect add_btn = {w - add_bw - gap, content_y, add_bw, add_bh};
        if (draw_button_ex(r, s->font, add_btn, "+ Add",
                           (SDL_Color){30, 100, 50, 255},
                           (SDL_Color){200, 255, 210, 255},
                           s->input_state.mouse_x, s->input_state.mouse_y,
                           s->input_state.mouse_pressed)) {
            s->ui_state.edit_selected_id = 0;
            textbuf_clear(&s->ui_state.edit_name);
            textbuf_clear(&s->ui_state.edit_pin);
            s->ui_state.edit_role = ROLE_CASHIER;
            s->ui_state.overlay = OVERLAY_EDIT_USER;
        }

        int ry = content_y + add_bh + gap;
        for (i32 i = 0; i < s->user_count; i++) {
            SDL_Rect row = {list_x, ry, w - list_x * 2, row_h};
            draw_panel(r, row, (SDL_Color){35, 38, 50, 255}, (SDL_Color){60, 60, 80, 255});

            char uline[96];
            snprintf(uline, sizeof(uline), "%s  [%s]",
                     s->users[i].username,
                     s->users[i].role == ROLE_ADMIN ? "Admin" : "Cashier");
            draw_text(r, s->font, uline, list_x + 12, ry + 14,
                      (SDL_Color){220, 220, 220, 255});

            SDL_Rect edit_b = {w - 200, ry + 7, 80, 36};
            if (draw_button(r, s->font, edit_b, "Edit",
                            s->input_state.mouse_x, s->input_state.mouse_y,
                            s->input_state.mouse_pressed)) {
                s->ui_state.edit_selected_id = s->users[i].id;
                textbuf_clear(&s->ui_state.edit_name);
                textbuf_append(&s->ui_state.edit_name, s->users[i].username);
                textbuf_clear(&s->ui_state.edit_pin);  /* don't pre-fill PIN */
                s->ui_state.edit_role = s->users[i].role;
                s->ui_state.overlay = OVERLAY_EDIT_USER;
            }

            SDL_Rect del_b = {w - 110, ry + 7, 80, 36};
            /* Prevent deleting yourself */
            i8 is_self = (s->users[i].id == s->session.user.id);
            if (!is_self && draw_button_ex(r, s->font, del_b, "Delete",
                               (SDL_Color){120, 30, 30, 255},
                               (SDL_Color){255, 180, 180, 255},
                               s->input_state.mouse_x, s->input_state.mouse_y,
                               s->input_state.mouse_pressed)) {
                s->ui_state.confirm_delete_id   = s->users[i].id;
                s->ui_state.confirm_delete_type = 3;
                snprintf(s->ui_state.confirm_msg,
                         sizeof(s->ui_state.confirm_msg),
                         "Delete user \"%s\"?", s->users[i].username);
                s->ui_state.overlay = OVERLAY_CONFIRM_DELETE;
            }
            ry += row_h + row_gap;
        }
    }

    /* Overlays on top */
    switch (s->ui_state.overlay) {
        case OVERLAY_EDIT_CATEGORY:   draw_overlay_edit_category(s, r, w, h); break;
        case OVERLAY_EDIT_PRODUCT:    draw_overlay_edit_product(s, r, w, h);  break;
        case OVERLAY_EDIT_DISCOUNT:   draw_overlay_edit_discount(s, r, w, h); break;
        case OVERLAY_EDIT_COLOUR:     draw_overlay_colour_picker(s, r, w, h); break;
        case OVERLAY_EDIT_USER:       draw_overlay_edit_user(s, r, w, h);     break;
        case OVERLAY_CONFIRM_DELETE:  draw_overlay_confirm_delete(s, r, w, h); break;
        default: break;
    }

    draw_alert_toast(s, r, w, dt_ms);
}

/* ─────────────────────────────────────────────────────────────
   Top-level frame draw
   ───────────────────────────────────────────────────────────── */
void immediate_mode_ui_draw(AppState *app_state, SDL_Renderer *renderer, int w, int h, u32 dt_ms)
{
    UIState    *ui = &app_state->ui_state;
    InputState *in = &app_state->input_state;

    /* ── Hardware keyboard routing (keyboard mode only) ──────── */
    if (!app_state->input_touchscreen && ui->active_text_buf) {
        /* Append typed characters */
        if (in->text_input[0] != '\0')
            textbuf_append(ui->active_text_buf, in->text_input);
        /* Backspace */
        if (in->key_backspace)
            textbuf_backspace(ui->active_text_buf);
        /* Enter / Return closes the field (same as Done on OSK) */
        if (in->key_enter) {
            SDL_StopTextInput();
            ui->active_text_buf = NULL;
        }
        input_clear_text(in);
    }

    /* ── Screen dispatch ─────────────────────────────────────── */
    switch (ui->screen) {
        case SCREEN_LOGIN:
            draw_screen_login(app_state, renderer, w, h);
            draw_alert_toast(app_state, renderer, w, dt_ms);
            break;
        case SCREEN_POS:
            draw_screen_pos(app_state, renderer, w, h, dt_ms);
            break;
        case SCREEN_EDIT:
            draw_screen_edit(app_state, renderer, w, h, dt_ms);
            break;
    }

    /* ── On-screen keyboard (touchscreen mode, drawn last = on top) ── */
    if (app_state->input_touchscreen && ui->active_text_buf) {
        draw_osk(renderer, app_state->font, in, ui, w, h);
    }
}
