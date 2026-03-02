#include "printer.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ─── ESC-POS byte sequences ────────────────────────────────── */
#define ESC  0x1B
#define GS   0x1D
#define LF   0x0A
#define FF   0x0C

/* Initialize / reset printer */
static const unsigned char CMD_INIT[]     = {ESC, '@'};

/* Text alignment */
static const unsigned char CMD_LEFT[]     = {ESC, 'a', 0};
static const unsigned char CMD_CENTER[]   = {ESC, 'a', 1};

/* Emphasis on/off */
static const unsigned char CMD_BOLD_ON[]  = {ESC, 'E', 1};
static const unsigned char CMD_BOLD_OFF[] = {ESC, 'E', 0};

/* Double height + width on/off */
static const unsigned char CMD_DBLSZ_ON[] = {GS,  '!', 0x11};
static const unsigned char CMD_DBLSZ_OFF[]= {GS,  '!', 0x00};

/* Cut paper (full cut) */
static const unsigned char CMD_CUT[]      = {GS,  'V', 0x00};

/* Cash drawer: pin 2, 50ms on, 50ms off */
static const unsigned char CMD_DRAWER[]   = {ESC, 'p', 0x00, 0x19, 0x19};

/* ─── Low-level write helper ─────────────────────────────────── */
static int
printer_write(Printer *p, const void *buf, size_t len)
{
    if (p->fd < 0) return -1;
    ssize_t written = write(p->fd, buf, len);
    return (written == (ssize_t)len) ? 0 : -1;
}

static int
printer_write_str(Printer *p, const char *s)
{
    return printer_write(p, s, strlen(s));
}

static int
printer_lf(Printer *p)
{
    unsigned char c = LF;
    return printer_write(p, &c, 1);
}

/* ─── Public API ─────────────────────────────────────────────── */

int printer_open(Printer *p, const char *device)
{
    strncpy(p->device, device, PRINTER_DEV_MAX - 1);
    p->device[PRINTER_DEV_MAX - 1] = '\0';

    p->fd = open(device, O_WRONLY | O_NOCTTY);
    if (p->fd < 0) {
        perror("[printer] open");
        return -1;
    }

    /* Reset printer on open */
    if (printer_write(p, CMD_INIT, sizeof(CMD_INIT)) != 0) {
        close(p->fd);
        p->fd = -1;
        return -1;
    }

    printf("[printer] opened %s\n", device);
    return 0;
}

void printer_close(Printer *p)
{
    if (p->fd >= 0) {
        close(p->fd);
        p->fd = -1;
    }
}

int printer_is_open(const Printer *p)
{
    return p->fd >= 0;
}

int printer_open_drawer(Printer *p)
{
    if (p->fd < 0) {
        fprintf(stderr, "[printer] drawer: printer not open\n");
        return -1;
    }
    int r = printer_write(p, CMD_DRAWER, sizeof(CMD_DRAWER));
    if (r != 0)
        perror("[printer] drawer pulse");
    else
        printf("[printer] cash drawer opened\n");
    return r;
}

int printer_print_receipt(Printer *p, const Transaction *t,
                          Product *products, int product_count)
{
    if (p->fd < 0) {
        fprintf(stderr, "[printer] receipt: printer not open\n");
        return -1;
    }

    char buf[256];
    int  r = 0;

    /* ── Header ── */
    r |= printer_write(p, CMD_INIT,     sizeof(CMD_INIT));
    r |= printer_write(p, CMD_CENTER,   sizeof(CMD_CENTER));
    r |= printer_write(p, CMD_DBLSZ_ON, sizeof(CMD_DBLSZ_ON));
    r |= printer_write_str(p, p->store_name[0] ? p->store_name : "POS SYSTEM");
    r |= printer_lf(p);
    r |= printer_write(p, CMD_DBLSZ_OFF, sizeof(CMD_DBLSZ_OFF));

    if (p->store_address[0]) {
        r |= printer_write_str(p, p->store_address);
        r |= printer_lf(p);
    }

    r |= printer_lf(p);
    r |= printer_write(p, CMD_LEFT, sizeof(CMD_LEFT));

    /* Divider */
    r |= printer_write_str(p, "--------------------------------\n");

    /* Transaction ref + timestamp */
    r |= printer_write_str(p, t->receipt_ref);
    r |= printer_lf(p);

    time_t ts = (time_t)t->timestamp;
    char   time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&ts));
    r |= printer_write_str(p, time_str);
    r |= printer_lf(p);

    r |= printer_write_str(p, "--------------------------------\n");

    /* ── Line items ── */
    const char *currency = p->currency[0] ? p->currency : "Rs";

    for (int i = 0; i < t->item_count; i++) {
        const char *name = "Unknown";
        for (int j = 0; j < product_count; j++) {
            if (products[j].id == t->items[i].product_id) {
                name = products[j].key_name;
                break;
            }
        }

        /* "Name                   x2" */
        snprintf(buf, sizeof(buf), "%-20s x%d\n", name, t->items[i].qty);
        r |= printer_write_str(p, buf);

        /* "  unit_price  line_total" */
        snprintf(buf, sizeof(buf), "  %s %-8.0f    %s %.2f\n",
                 currency, t->items[i].unit_price,
                 currency, t->items[i].line_total);
        r |= printer_write_str(p, buf);

        if (t->items[i].discount_percent > 0.0f) {
            snprintf(buf, sizeof(buf), "  Discount: %.0f%%\n",
                     t->items[i].discount_percent);
            r |= printer_write_str(p, buf);
        }
    }

    /* ── Totals ── */
    r |= printer_write_str(p, "--------------------------------\n");

    if (t->discount_total > 0.0f) {
        snprintf(buf, sizeof(buf), "Discount:       %s -%.2f\n",
                 currency, t->discount_total);
        r |= printer_write_str(p, buf);
    }

    if (t->tax > 0.0f) {
        snprintf(buf, sizeof(buf), "Tax:            %s  %.2f\n",
                 currency, t->tax);
        r |= printer_write_str(p, buf);
    }

    r |= printer_write(p, CMD_BOLD_ON, sizeof(CMD_BOLD_ON));
    snprintf(buf, sizeof(buf), "TOTAL:          %s  %.2f\n",
             currency, t->grand_total);
    r |= printer_write_str(p, buf);
    r |= printer_write(p, CMD_BOLD_OFF, sizeof(CMD_BOLD_OFF));

    /* ── Payment ── */
    r |= printer_write_str(p, "--------------------------------\n");

    if (t->payment_method == PAY_CASH) {
        snprintf(buf, sizeof(buf), "Cash:           %s  %.2f\n",
                 currency, t->tendered);
        r |= printer_write_str(p, buf);
        snprintf(buf, sizeof(buf), "Change:         %s  %.2f\n",
                 currency, t->change_due);
        r |= printer_write_str(p, buf);
    } else {
        r |= printer_write_str(p, "Payment:        CARD\n");
    }

    /* ── Footer ── */
    r |= printer_write_str(p, "--------------------------------\n");
    r |= printer_write(p, CMD_CENTER, sizeof(CMD_CENTER));
    r |= printer_write_str(p, "Thank you!\n");
    r |= printer_lf(p);
    r |= printer_lf(p);
    r |= printer_lf(p);

    /* Cut */
    r |= printer_write(p, CMD_CUT, sizeof(CMD_CUT));

    if (r != 0)
        perror("[printer] receipt write error");
    else
        printf("[printer] receipt printed: %s\n", t->receipt_ref);

    return r;
}
