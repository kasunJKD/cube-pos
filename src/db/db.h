#ifndef POS_DB_H
#define POS_DB_H

#include <sqlite3.h>
#include "../core/core.h"

typedef struct {
    sqlite3 *handle;
    char     path[256];
    i64      last_error_code;
} PosDB;

/* ── Open / close ──────────────────────────────────────────── */
int  db_open(PosDB *db, const char *path);
void db_close(PosDB *db);
int  db_is_open(const PosDB *db);

/* ── Transactions ──────────────────────────────────────────── */
int  db_load_next_txn_id(PosDB *db, i64 *out_id);
int  db_save_transaction(PosDB *db, const Transaction *t);
int  db_mark_synced(PosDB *db, i64 txn_id);
int  db_get_unsynced(PosDB *db, i64 *out_ids, int cap);
int  db_load_transaction(PosDB *db, i64 txn_id, Transaction *out);

/* ── Users ─────────────────────────────────────────────────── */
/* Load all active users into out[]. Returns count loaded (max cap). */
int  db_load_users(PosDB *db, User *out, int cap);
/* Save or update a user. id==0 → INSERT (id is set on return). */
int  db_save_user(PosDB *db, User *u);
/* Delete user by id. Returns 0 on success. */
int  db_delete_user(PosDB *db, i32 id);
/* Lookup by username; returns 0 and fills *out if found. */
int  db_find_user(PosDB *db, const char *username, User *out);
/* Seed a default admin user if no users exist yet. */
int  db_seed_default_admin(PosDB *db);

/* ── Categories (DB-backed) ────────────────────────────────── */
int  db_load_categories(PosDB *db, Category *out, int cap);
int  db_save_category(PosDB *db, Category *c);   /* id==0 → INSERT */
int  db_delete_category(PosDB *db, i32 id);
/* Seed from static arrays if table is empty. */
int  db_seed_categories(PosDB *db, Category *src, int count);

/* ── Products (DB-backed) ──────────────────────────────────── */
int  db_load_products(PosDB *db, Product *out, int cap);
int  db_save_product(PosDB *db, Product *p);     /* id==0 → INSERT */
int  db_delete_product(PosDB *db, i32 id);
int  db_seed_products(PosDB *db, Product *src, int count);

/* ── Discount presets ──────────────────────────────────────── */
int  db_load_discount_presets(PosDB *db, DiscountPreset *out, int cap);
int  db_save_discount_preset(PosDB *db, DiscountPreset *dp);
int  db_delete_discount_preset(PosDB *db, i32 id);

#endif /* POS_DB_H */
