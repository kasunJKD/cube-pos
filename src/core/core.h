#ifndef CORE_H
#define CORE_H

#include "../pos_util.h"

#define MAX_CART_ITEMS    32
#define MAX_CATEGORIES   128
#define MAX_PRODUCTS     512
#define MAX_DISC_PRESETS  32
#define MAX_USERS         32
#define NAME_LEN          64
#define HASH_LEN          65   /* SHA-256 hex + NUL */

/* ── Colour stored as packed RGBA ───────────────────────────── */
typedef struct { u8 r, g, b, a; } Color4;

#define COLOR4(r,g,b)   ((Color4){(r),(g),(b),255})
#define COLOR4_DEFAULT  COLOR4(60,60,60)

/* ── Role ───────────────────────────────────────────────────── */
typedef enum {
    ROLE_CASHIER = 0,
    ROLE_ADMIN   = 1
} UserRole;

/* ── User ───────────────────────────────────────────────────── */
typedef struct {
    i32      id;
    char     username[NAME_LEN];
    char     pin_hash[HASH_LEN];   /* SHA-256(pin) hex, or "" for no-pin */
    UserRole role;
    i8       active;               /* 0 = disabled */
} User;

/* ── Discount preset ────────────────────────────────────────── */
typedef struct {
    i32   id;
    char  name[NAME_LEN];          /* e.g. "Staff 10%" */
    float percent;                 /* 0–100 */
    i8    active;
} DiscountPreset;

/* ── Category (DB-backed, custom colour + label) ────────────── */
typedef struct {
    i32    id;
    char   category_name[NAME_LEN];
    Color4 btn_color;     /* button fill colour */
    Color4 text_color;    /* label text colour  */
    i32    sort_order;
    i8     active;
} Category;

/* ── Product (DB-backed, custom colour + label) ─────────────── */
typedef struct {
    i32    id;
    i32    category_id;
    char   key_name[NAME_LEN];
    float  price;
    Color4 btn_color;
    Color4 text_color;
    i32    sort_order;
    i8     active;
} Product;

typedef struct {
	i32 product_id;
	i32 qty;
	float discount_percent;
} CartItem;

typedef enum {
	VIEW_CATEGORIES,
	VIEW_PRODUCTS
} ProductView;

typedef enum {
    PAY_CASH = 0,
    PAY_CARD = 1
} PaymentMethod;

typedef enum {
    TXN_PENDING  = 0,  /* in cart, not finalised */
    TXN_COMPLETE = 1,  /* paid, receipt printed */
    TXN_SYNCED   = 2   /* uploaded to server */
} TxnStatus;

#define MAX_TXN_ITEMS 32

typedef struct {
    i32           product_id;
    i32           qty;
    float         unit_price;
    float         discount_percent;
    float         line_total;        /* (unit_price * qty) * (1 - disc/100) */
} TxnItem;

typedef struct {
    i64           id;                /* local sequential ID */
    i64           timestamp;         /* Unix seconds (time(NULL)) */
    TxnItem       items[MAX_TXN_ITEMS];
    i32           item_count;
    float         subtotal;
    float         discount_total;
    float         tax;
    float         grand_total;
    float         tendered;          /* cash given by customer */
    float         change_due;        /* tendered - grand_total (cash only) */
    PaymentMethod payment_method;
    TxnStatus     status;
    char          receipt_ref[32];   /* e.g. "TXN-00042" */
} Transaction;

/* static product/category data defined in product.c */
extern Category categories[];
extern Product  products[];
extern int      product_count;
extern int      category_count;

#endif
