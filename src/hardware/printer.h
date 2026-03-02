#ifndef PRINTER_H
#define PRINTER_H

/*
 * ESC-POS thermal receipt printer + cash drawer driver.
 *
 * Works with any ESC-POS compatible printer accessible as a character
 * device on Linux (USB: /dev/usb/lp0, serial: /dev/ttyUSB0, etc.).
 *
 * Cash drawer:  most drawers are connected to the printer's drawer port
 *               and triggered with ESC p 0 <on_time> <off_time>.
 *
 * Thread safety: not required — call only from the main loop.
 */

#include "../core/core.h"

/* Maximum path length for the printer device */
#define PRINTER_DEV_MAX 64

typedef struct {
    int  fd;                          /* file descriptor, -1 = not open  */
    char device[PRINTER_DEV_MAX];     /* e.g. "/dev/usb/lp0"             */
    char store_name[64];              /* printed at top of every receipt  */
    char store_address[128];
    char currency[8];                 /* e.g. "Rs" or "LKR"              */
} Printer;

/* Open the printer device. Returns 0 on success, -1 on error. */
int  printer_open(Printer *p, const char *device);

/* Close the printer device. */
void printer_close(Printer *p);

/* Returns 1 if printer fd is valid, 0 otherwise. */
int  printer_is_open(const Printer *p);

/* Print a receipt for the given completed transaction.
   Includes store header, items, totals, payment and footer.
   Returns 0 on success, -1 on write error. */
int  printer_print_receipt(Printer *p, const Transaction *t,
                           Product *products, int product_count);

/* Open the cash drawer (send ESC-POS drawer pulse).
   Returns 0 on success, -1 on error. */
int  printer_open_drawer(Printer *p);

#endif /* PRINTER_H */
