#include "db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─── Schema ────────────────────────────────────────────────── */
static const char *SCHEMA_SQL =
    /* Users */
    "CREATE TABLE IF NOT EXISTS users ("
    "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  username TEXT    NOT NULL UNIQUE,"
    "  pin_hash TEXT    NOT NULL DEFAULT '',"
    "  role     INTEGER NOT NULL DEFAULT 0,"   /* 0=CASHIER 1=ADMIN */
    "  active   INTEGER NOT NULL DEFAULT 1"
    ");"

    /* Categories (DB-backed, replaces static arrays) */
    "CREATE TABLE IF NOT EXISTS categories ("
    "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  category_name TEXT    NOT NULL,"
    "  btn_r INTEGER NOT NULL DEFAULT 60,"
    "  btn_g INTEGER NOT NULL DEFAULT 60,"
    "  btn_b INTEGER NOT NULL DEFAULT 60,"
    "  txt_r INTEGER NOT NULL DEFAULT 255,"
    "  txt_g INTEGER NOT NULL DEFAULT 255,"
    "  txt_b INTEGER NOT NULL DEFAULT 255,"
    "  sort_order    INTEGER NOT NULL DEFAULT 0,"
    "  active        INTEGER NOT NULL DEFAULT 1"
    ");"

    /* Products */
    "CREATE TABLE IF NOT EXISTS products ("
    "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  category_id INTEGER NOT NULL REFERENCES categories(id),"
    "  key_name    TEXT    NOT NULL,"
    "  price       REAL    NOT NULL,"
    "  btn_r INTEGER NOT NULL DEFAULT 60,"
    "  btn_g INTEGER NOT NULL DEFAULT 60,"
    "  btn_b INTEGER NOT NULL DEFAULT 60,"
    "  txt_r INTEGER NOT NULL DEFAULT 255,"
    "  txt_g INTEGER NOT NULL DEFAULT 255,"
    "  txt_b INTEGER NOT NULL DEFAULT 255,"
    "  sort_order  INTEGER NOT NULL DEFAULT 0,"
    "  active      INTEGER NOT NULL DEFAULT 1"
    ");"

    /* Discount presets */
    "CREATE TABLE IF NOT EXISTS discount_presets ("
    "  id      INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name    TEXT    NOT NULL,"
    "  percent REAL    NOT NULL,"
    "  active  INTEGER NOT NULL DEFAULT 1"
    ");"

    /* Transactions */
    "CREATE TABLE IF NOT EXISTS transactions ("
    "  id             INTEGER PRIMARY KEY,"
    "  timestamp      INTEGER NOT NULL,"
    "  subtotal       REAL    NOT NULL,"
    "  discount_total REAL    NOT NULL DEFAULT 0,"
    "  tax            REAL    NOT NULL DEFAULT 0,"
    "  grand_total    REAL    NOT NULL,"
    "  tendered       REAL    NOT NULL DEFAULT 0,"
    "  change_due     REAL    NOT NULL DEFAULT 0,"
    "  payment_method INTEGER NOT NULL,"
    "  status         INTEGER NOT NULL DEFAULT 0,"
    "  receipt_ref    TEXT    NOT NULL"
    ");"

    "CREATE TABLE IF NOT EXISTS transaction_items ("
    "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  txn_id          INTEGER NOT NULL REFERENCES transactions(id),"
    "  product_id      INTEGER NOT NULL,"
    "  qty             INTEGER NOT NULL,"
    "  unit_price      REAL    NOT NULL,"
    "  discount_pct    REAL    NOT NULL DEFAULT 0,"
    "  line_total      REAL    NOT NULL"
    ");"

    "CREATE TABLE IF NOT EXISTS sync_queue ("
    "  txn_id  INTEGER PRIMARY KEY REFERENCES transactions(id),"
    "  status  INTEGER NOT NULL DEFAULT 0"
    ");";

/* ─── Helpers ────────────────────────────────────────────────── */
static int db_exec(PosDB *db, const char *sql)
{
    char *errmsg = NULL;
    int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] %s\n  SQL: %.80s\n", errmsg, sql);
        db->last_error_code = rc;
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

static sqlite3_stmt *db_prep(PosDB *db, const char *sql)
{
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &s, NULL) != SQLITE_OK) {
        fprintf(stderr, "[db] prepare: %s\n  SQL: %.80s\n",
                sqlite3_errmsg(db->handle), sql);
        return NULL;
    }
    return s;
}

static int db_step_done(sqlite3_stmt *s)
{
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ─── Open / Close ───────────────────────────────────────────── */
int db_open(PosDB *db, const char *path)
{
    strncpy(db->path, path, sizeof(db->path)-1);
    db->last_error_code = SQLITE_OK;

    if (sqlite3_open(path, &db->handle) != SQLITE_OK) {
        fprintf(stderr, "[db] open: %s\n", sqlite3_errmsg(db->handle));
        db->handle = NULL;
        return -1;
    }
    db_exec(db, "PRAGMA journal_mode=WAL;");
    db_exec(db, "PRAGMA synchronous=NORMAL;");
    db_exec(db, "PRAGMA foreign_keys=ON;");
    if (db_exec(db, SCHEMA_SQL) != 0) {
        sqlite3_close(db->handle); db->handle = NULL; return -1;
    }
    printf("[db] opened: %s\n", path);
    return 0;
}

void db_close(PosDB *db)
{
    if (db->handle) { sqlite3_close(db->handle); db->handle = NULL; }
}

int db_is_open(const PosDB *db) { return db->handle != NULL; }

/* ─── Transaction ID ─────────────────────────────────────────── */
int db_load_next_txn_id(PosDB *db, i64 *out_id)
{
    sqlite3_stmt *s = db_prep(db,
        "SELECT COALESCE(MAX(id),0)+1 FROM transactions;");
    if (!s) return -1;
    *out_id = (sqlite3_step(s) == SQLITE_ROW)
              ? (i64)sqlite3_column_int64(s, 0) : 1;
    sqlite3_finalize(s);
    return 0;
}

/* ─── Save Transaction ───────────────────────────────────────── */
int db_save_transaction(PosDB *db, const Transaction *t)
{
    if (!db->handle) return -1;
    db_exec(db, "BEGIN;");

    sqlite3_stmt *s = db_prep(db,
        "INSERT OR REPLACE INTO transactions"
        "(id,timestamp,subtotal,discount_total,tax,grand_total,"
        " tendered,change_due,payment_method,status,receipt_ref)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?);");
    if (!s) { db_exec(db,"ROLLBACK;"); return -1; }

    sqlite3_bind_int64(s,1,(sqlite3_int64)t->id);
    sqlite3_bind_int64(s,2,(sqlite3_int64)t->timestamp);
    sqlite3_bind_double(s,3,(double)t->subtotal);
    sqlite3_bind_double(s,4,(double)t->discount_total);
    sqlite3_bind_double(s,5,(double)t->tax);
    sqlite3_bind_double(s,6,(double)t->grand_total);
    sqlite3_bind_double(s,7,(double)t->tendered);
    sqlite3_bind_double(s,8,(double)t->change_due);
    sqlite3_bind_int(s,9,(int)t->payment_method);
    sqlite3_bind_int(s,10,(int)t->status);
    sqlite3_bind_text(s,11,t->receipt_ref,-1,SQLITE_STATIC);

    if (db_step_done(s) != 0) { db_exec(db,"ROLLBACK;"); return -1; }

    for (int i = 0; i < t->item_count; i++) {
        sqlite3_stmt *si = db_prep(db,
            "INSERT INTO transaction_items"
            "(txn_id,product_id,qty,unit_price,discount_pct,line_total)"
            " VALUES(?,?,?,?,?,?);");
        if (!si) { db_exec(db,"ROLLBACK;"); return -1; }
        sqlite3_bind_int64(si,1,(sqlite3_int64)t->id);
        sqlite3_bind_int(si,2,t->items[i].product_id);
        sqlite3_bind_int(si,3,t->items[i].qty);
        sqlite3_bind_double(si,4,(double)t->items[i].unit_price);
        sqlite3_bind_double(si,5,(double)t->items[i].discount_percent);
        sqlite3_bind_double(si,6,(double)t->items[i].line_total);
        if (db_step_done(si) != 0) { db_exec(db,"ROLLBACK;"); return -1; }
    }

    sqlite3_stmt *sq = db_prep(db,
        "INSERT OR IGNORE INTO sync_queue(txn_id,status) VALUES(?,0);");
    if (sq) { sqlite3_bind_int64(sq,1,(sqlite3_int64)t->id); db_step_done(sq); }

    db_exec(db,"COMMIT;");
    printf("[db] saved txn %lld\n",(long long)t->id);
    return 0;
}

int db_mark_synced(PosDB *db, i64 txn_id)
{
    sqlite3_stmt *s = db_prep(db,
        "UPDATE sync_queue SET status=1 WHERE txn_id=?;");
    if (!s) return -1;
    sqlite3_bind_int64(s,1,(sqlite3_int64)txn_id);
    return db_step_done(s);
}

int db_get_unsynced(PosDB *db, i64 *out_ids, int cap)
{
    if (!db->handle || !out_ids || cap<=0) return 0;
    sqlite3_stmt *s = db_prep(db,
        "SELECT txn_id FROM sync_queue WHERE status=0 ORDER BY txn_id LIMIT ?;");
    if (!s) return 0;
    sqlite3_bind_int(s,1,cap);
    int n=0;
    while (sqlite3_step(s)==SQLITE_ROW && n<cap)
        out_ids[n++]=(i64)sqlite3_column_int64(s,0);
    sqlite3_finalize(s);
    return n;
}

int db_load_transaction(PosDB *db, i64 txn_id, Transaction *out)
{
    if (!db->handle || !out) return -1;
    memset(out,0,sizeof(*out));
    sqlite3_stmt *s = db_prep(db,
        "SELECT id,timestamp,subtotal,discount_total,tax,grand_total,"
        "       tendered,change_due,payment_method,status,receipt_ref"
        " FROM transactions WHERE id=?;");
    if (!s) return -1;
    sqlite3_bind_int64(s,1,(sqlite3_int64)txn_id);
    if (sqlite3_step(s)!=SQLITE_ROW) { sqlite3_finalize(s); return -1; }
    out->id             =(i64)sqlite3_column_int64(s,0);
    out->timestamp      =(i64)sqlite3_column_int64(s,1);
    out->subtotal       =(float)sqlite3_column_double(s,2);
    out->discount_total =(float)sqlite3_column_double(s,3);
    out->tax            =(float)sqlite3_column_double(s,4);
    out->grand_total    =(float)sqlite3_column_double(s,5);
    out->tendered       =(float)sqlite3_column_double(s,6);
    out->change_due     =(float)sqlite3_column_double(s,7);
    out->payment_method =(PaymentMethod)sqlite3_column_int(s,8);
    out->status         =(TxnStatus)sqlite3_column_int(s,9);
    const char *ref=(const char*)sqlite3_column_text(s,10);
    if (ref) strncpy(out->receipt_ref,ref,sizeof(out->receipt_ref)-1);
    sqlite3_finalize(s);

    sqlite3_stmt *si = db_prep(db,
        "SELECT product_id,qty,unit_price,discount_pct,line_total"
        " FROM transaction_items WHERE txn_id=? ORDER BY id;");
    if (!si) return 0;
    sqlite3_bind_int64(si,1,(sqlite3_int64)txn_id);
    while (sqlite3_step(si)==SQLITE_ROW && out->item_count<MAX_TXN_ITEMS) {
        TxnItem *it=&out->items[out->item_count++];
        it->product_id      =sqlite3_column_int(si,0);
        it->qty             =sqlite3_column_int(si,1);
        it->unit_price      =(float)sqlite3_column_double(si,2);
        it->discount_percent=(float)sqlite3_column_double(si,3);
        it->line_total      =(float)sqlite3_column_double(si,4);
    }
    sqlite3_finalize(si);
    return 0;
}

/* ─── Users ──────────────────────────────────────────────────── */
int db_load_users(PosDB *db, User *out, int cap)
{
    if (!db->handle) return 0;
    sqlite3_stmt *s = db_prep(db,
        "SELECT id,username,pin_hash,role,active FROM users"
        " WHERE active=1 ORDER BY id;");
    if (!s) return 0;
    int n=0;
    while (sqlite3_step(s)==SQLITE_ROW && n<cap) {
        User *u=&out[n++];
        u->id     =sqlite3_column_int(s,0);
        const char *nm=(const char*)sqlite3_column_text(s,1);
        const char *ph=(const char*)sqlite3_column_text(s,2);
        if (nm) strncpy(u->username,nm,NAME_LEN-1);
        if (ph) strncpy(u->pin_hash,ph,HASH_LEN-1);
        u->role   =(UserRole)sqlite3_column_int(s,3);
        u->active =(i8)sqlite3_column_int(s,4);
    }
    sqlite3_finalize(s);
    return n;
}

int db_save_user(PosDB *db, User *u)
{
    if (!db->handle) return -1;
    if (u->id == 0) {
        sqlite3_stmt *s = db_prep(db,
            "INSERT INTO users(username,pin_hash,role,active)"
            " VALUES(?,?,?,?);");
        if (!s) return -1;
        sqlite3_bind_text(s,1,u->username,-1,SQLITE_STATIC);
        sqlite3_bind_text(s,2,u->pin_hash,-1,SQLITE_STATIC);
        sqlite3_bind_int(s,3,(int)u->role);
        sqlite3_bind_int(s,4,(int)u->active);
        if (db_step_done(s)!=0) return -1;
        u->id=(i32)sqlite3_last_insert_rowid(db->handle);
    } else {
        sqlite3_stmt *s = db_prep(db,
            "UPDATE users SET username=?,pin_hash=?,role=?,active=?"
            " WHERE id=?;");
        if (!s) return -1;
        sqlite3_bind_text(s,1,u->username,-1,SQLITE_STATIC);
        sqlite3_bind_text(s,2,u->pin_hash,-1,SQLITE_STATIC);
        sqlite3_bind_int(s,3,(int)u->role);
        sqlite3_bind_int(s,4,(int)u->active);
        sqlite3_bind_int(s,5,(int)u->id);
        if (db_step_done(s)!=0) return -1;
    }
    return 0;
}

int db_delete_user(PosDB *db, i32 id)
{
    /* Soft delete */
    sqlite3_stmt *s = db_prep(db,
        "UPDATE users SET active=0 WHERE id=?;");
    if (!s) return -1;
    sqlite3_bind_int(s,1,(int)id);
    return db_step_done(s);
}

int db_find_user(PosDB *db, const char *username, User *out)
{
    sqlite3_stmt *s = db_prep(db,
        "SELECT id,username,pin_hash,role,active FROM users"
        " WHERE username=? AND active=1 LIMIT 1;");
    if (!s) return -1;
    sqlite3_bind_text(s,1,username,-1,SQLITE_STATIC);
    if (sqlite3_step(s)!=SQLITE_ROW) { sqlite3_finalize(s); return -1; }
    out->id    =sqlite3_column_int(s,0);
    const char *nm=(const char*)sqlite3_column_text(s,1);
    const char *ph=(const char*)sqlite3_column_text(s,2);
    if (nm) strncpy(out->username,nm,NAME_LEN-1);
    if (ph) strncpy(out->pin_hash,ph,HASH_LEN-1);
    out->role  =(UserRole)sqlite3_column_int(s,3);
    out->active=(i8)sqlite3_column_int(s,4);
    sqlite3_finalize(s);
    return 0;
}

int db_seed_default_admin(PosDB *db)
{
    /* Only seeds if no users exist at all */
    sqlite3_stmt *chk = db_prep(db,"SELECT COUNT(*) FROM users;");
    if (!chk) return -1;
    int cnt=0;
    if (sqlite3_step(chk)==SQLITE_ROW) cnt=sqlite3_column_int(chk,0);
    sqlite3_finalize(chk);
    if (cnt>0) return 0;

    /* Default admin: username=admin  pin=1234  (hash stored as plain for seed;
       login module will compare directly for the default account) */
    User u={0};
    strncpy(u.username,"admin",NAME_LEN-1);
    /* Store PIN as plain text SHA256 of "1234":
       03ac674216f3e15c761ee1a5e255f067953623c8b388b4459e13f978d7c846f4 */
    /* SHA-256("1234") — 64 hex chars + NUL fits exactly in pin_hash[65] */
    snprintf(u.pin_hash, sizeof(u.pin_hash), "%s",
        "03ac674216f3e15c761ee1a5e255f067953623c8b388b4459e13f978d7c846f4");
    u.role   = ROLE_ADMIN;
    u.active = 1;
    return db_save_user(db, &u);
}

/* ─── Categories ─────────────────────────────────────────────── */
int db_load_categories(PosDB *db, Category *out, int cap)
{
    if (!db->handle) return 0;
    sqlite3_stmt *s = db_prep(db,
        "SELECT id,category_name,btn_r,btn_g,btn_b,txt_r,txt_g,txt_b,"
        "       sort_order,active"
        " FROM categories WHERE active=1 ORDER BY sort_order,id;");
    if (!s) return 0;
    int n=0;
    while (sqlite3_step(s)==SQLITE_ROW && n<cap) {
        Category *c=&out[n++];
        memset(c,0,sizeof(*c));
        c->id          =sqlite3_column_int(s,0);
        const char *nm =(const char*)sqlite3_column_text(s,1);
        if (nm) strncpy(c->category_name,nm,NAME_LEN-1);
        c->btn_color.r =(u8)sqlite3_column_int(s,2);
        c->btn_color.g =(u8)sqlite3_column_int(s,3);
        c->btn_color.b =(u8)sqlite3_column_int(s,4);
        c->btn_color.a =255;
        c->text_color.r=(u8)sqlite3_column_int(s,5);
        c->text_color.g=(u8)sqlite3_column_int(s,6);
        c->text_color.b=(u8)sqlite3_column_int(s,7);
        c->text_color.a=255;
        c->sort_order  =sqlite3_column_int(s,8);
        c->active      =(i8)sqlite3_column_int(s,9);
    }
    sqlite3_finalize(s);
    return n;
}

int db_save_category(PosDB *db, Category *c)
{
    if (!db->handle) return -1;
    if (c->id == 0) {
        sqlite3_stmt *s = db_prep(db,
            "INSERT INTO categories"
            "(category_name,btn_r,btn_g,btn_b,txt_r,txt_g,txt_b,sort_order,active)"
            " VALUES(?,?,?,?,?,?,?,?,?);");
        if (!s) return -1;
        sqlite3_bind_text(s,1,c->category_name,-1,SQLITE_STATIC);
        sqlite3_bind_int(s,2,c->btn_color.r);
        sqlite3_bind_int(s,3,c->btn_color.g);
        sqlite3_bind_int(s,4,c->btn_color.b);
        sqlite3_bind_int(s,5,c->text_color.r);
        sqlite3_bind_int(s,6,c->text_color.g);
        sqlite3_bind_int(s,7,c->text_color.b);
        sqlite3_bind_int(s,8,c->sort_order);
        sqlite3_bind_int(s,9,c->active);
        if (db_step_done(s)!=0) return -1;
        c->id=(i32)sqlite3_last_insert_rowid(db->handle);
    } else {
        sqlite3_stmt *s = db_prep(db,
            "UPDATE categories SET category_name=?,btn_r=?,btn_g=?,btn_b=?,"
            "  txt_r=?,txt_g=?,txt_b=?,sort_order=?,active=? WHERE id=?;");
        if (!s) return -1;
        sqlite3_bind_text(s,1,c->category_name,-1,SQLITE_STATIC);
        sqlite3_bind_int(s,2,c->btn_color.r);
        sqlite3_bind_int(s,3,c->btn_color.g);
        sqlite3_bind_int(s,4,c->btn_color.b);
        sqlite3_bind_int(s,5,c->text_color.r);
        sqlite3_bind_int(s,6,c->text_color.g);
        sqlite3_bind_int(s,7,c->text_color.b);
        sqlite3_bind_int(s,8,c->sort_order);
        sqlite3_bind_int(s,9,c->active);
        sqlite3_bind_int(s,10,c->id);
        if (db_step_done(s)!=0) return -1;
    }
    return 0;
}

int db_delete_category(PosDB *db, i32 id)
{
    sqlite3_stmt *s = db_prep(db,
        "UPDATE categories SET active=0 WHERE id=?;");
    if (!s) return -1;
    sqlite3_bind_int(s,1,(int)id);
    return db_step_done(s);
}

int db_seed_categories(PosDB *db, Category *src, int count)
{
    sqlite3_stmt *chk = db_prep(db,"SELECT COUNT(*) FROM categories;");
    if (!chk) return -1;
    int n=0;
    if (sqlite3_step(chk)==SQLITE_ROW) n=sqlite3_column_int(chk,0);
    sqlite3_finalize(chk);
    if (n>0) return 0;
    for (int i=0;i<count;i++) {
        Category c=src[i]; c.id=0;
        db_save_category(db,&c);
    }
    return 0;
}

/* ─── Products ───────────────────────────────────────────────── */
int db_load_products(PosDB *db, Product *out, int cap)
{
    if (!db->handle) return 0;
    sqlite3_stmt *s = db_prep(db,
        "SELECT id,category_id,key_name,price,"
        "       btn_r,btn_g,btn_b,txt_r,txt_g,txt_b,sort_order,active"
        " FROM products WHERE active=1 ORDER BY sort_order,id;");
    if (!s) return 0;
    int n=0;
    while (sqlite3_step(s)==SQLITE_ROW && n<cap) {
        Product *p=&out[n++];
        memset(p,0,sizeof(*p));
        p->id          =sqlite3_column_int(s,0);
        p->category_id =sqlite3_column_int(s,1);
        const char *nm =(const char*)sqlite3_column_text(s,2);
        if (nm) strncpy(p->key_name,nm,NAME_LEN-1);
        p->price       =(float)sqlite3_column_double(s,3);
        p->btn_color.r =(u8)sqlite3_column_int(s,4);
        p->btn_color.g =(u8)sqlite3_column_int(s,5);
        p->btn_color.b =(u8)sqlite3_column_int(s,6);
        p->btn_color.a =255;
        p->text_color.r=(u8)sqlite3_column_int(s,7);
        p->text_color.g=(u8)sqlite3_column_int(s,8);
        p->text_color.b=(u8)sqlite3_column_int(s,9);
        p->text_color.a=255;
        p->sort_order  =sqlite3_column_int(s,10);
        p->active      =(i8)sqlite3_column_int(s,11);
    }
    sqlite3_finalize(s);
    return n;
}

int db_save_product(PosDB *db, Product *p)
{
    if (!db->handle) return -1;
    if (p->id == 0) {
        sqlite3_stmt *s = db_prep(db,
            "INSERT INTO products"
            "(category_id,key_name,price,btn_r,btn_g,btn_b,"
            " txt_r,txt_g,txt_b,sort_order,active)"
            " VALUES(?,?,?,?,?,?,?,?,?,?,?);");
        if (!s) return -1;
        sqlite3_bind_int(s,1,p->category_id);
        sqlite3_bind_text(s,2,p->key_name,-1,SQLITE_STATIC);
        sqlite3_bind_double(s,3,(double)p->price);
        sqlite3_bind_int(s,4,p->btn_color.r);
        sqlite3_bind_int(s,5,p->btn_color.g);
        sqlite3_bind_int(s,6,p->btn_color.b);
        sqlite3_bind_int(s,7,p->text_color.r);
        sqlite3_bind_int(s,8,p->text_color.g);
        sqlite3_bind_int(s,9,p->text_color.b);
        sqlite3_bind_int(s,10,p->sort_order);
        sqlite3_bind_int(s,11,p->active);
        if (db_step_done(s)!=0) return -1;
        p->id=(i32)sqlite3_last_insert_rowid(db->handle);
    } else {
        sqlite3_stmt *s = db_prep(db,
            "UPDATE products SET category_id=?,key_name=?,price=?,"
            "  btn_r=?,btn_g=?,btn_b=?,txt_r=?,txt_g=?,txt_b=?,"
            "  sort_order=?,active=? WHERE id=?;");
        if (!s) return -1;
        sqlite3_bind_int(s,1,p->category_id);
        sqlite3_bind_text(s,2,p->key_name,-1,SQLITE_STATIC);
        sqlite3_bind_double(s,3,(double)p->price);
        sqlite3_bind_int(s,4,p->btn_color.r);
        sqlite3_bind_int(s,5,p->btn_color.g);
        sqlite3_bind_int(s,6,p->btn_color.b);
        sqlite3_bind_int(s,7,p->text_color.r);
        sqlite3_bind_int(s,8,p->text_color.g);
        sqlite3_bind_int(s,9,p->text_color.b);
        sqlite3_bind_int(s,10,p->sort_order);
        sqlite3_bind_int(s,11,p->active);
        sqlite3_bind_int(s,12,p->id);
        if (db_step_done(s)!=0) return -1;
    }
    return 0;
}

int db_delete_product(PosDB *db, i32 id)
{
    sqlite3_stmt *s = db_prep(db,
        "UPDATE products SET active=0 WHERE id=?;");
    if (!s) return -1;
    sqlite3_bind_int(s,1,(int)id);
    return db_step_done(s);
}

int db_seed_products(PosDB *db, Product *src, int count)
{
    sqlite3_stmt *chk = db_prep(db,"SELECT COUNT(*) FROM products;");
    if (!chk) return -1;
    int n=0;
    if (sqlite3_step(chk)==SQLITE_ROW) n=sqlite3_column_int(chk,0);
    sqlite3_finalize(chk);
    if (n>0) return 0;

    /* We need to remap category IDs since seeded categories may have new IDs */
    /* Simple approach: load categories to build id map */
    Category cats[MAX_CATEGORIES];
    int nc = db_load_categories(db, cats, MAX_CATEGORIES);

    for (int i=0;i<count;i++) {
        Product p=src[i]; p.id=0;
        /* Remap category_id by sort_order position */
        int old_cat_pos = p.category_id - 1; /* 1-based seed → 0-based */
        if (old_cat_pos >= 0 && old_cat_pos < nc)
            p.category_id = cats[old_cat_pos].id;
        db_save_product(db,&p);
    }
    return 0;
}

/* ─── Discount presets ───────────────────────────────────────── */
int db_load_discount_presets(PosDB *db, DiscountPreset *out, int cap)
{
    if (!db->handle) return 0;
    sqlite3_stmt *s = db_prep(db,
        "SELECT id,name,percent,active FROM discount_presets"
        " WHERE active=1 ORDER BY id;");
    if (!s) return 0;
    int n=0;
    while (sqlite3_step(s)==SQLITE_ROW && n<cap) {
        DiscountPreset *dp=&out[n++];
        dp->id      =sqlite3_column_int(s,0);
        const char *nm=(const char*)sqlite3_column_text(s,1);
        if (nm) strncpy(dp->name,nm,NAME_LEN-1);
        dp->percent =(float)sqlite3_column_double(s,2);
        dp->active  =(i8)sqlite3_column_int(s,3);
    }
    sqlite3_finalize(s);
    return n;
}

int db_save_discount_preset(PosDB *db, DiscountPreset *dp)
{
    if (!db->handle) return -1;
    if (dp->id == 0) {
        sqlite3_stmt *s = db_prep(db,
            "INSERT INTO discount_presets(name,percent,active) VALUES(?,?,?);");
        if (!s) return -1;
        sqlite3_bind_text(s,1,dp->name,-1,SQLITE_STATIC);
        sqlite3_bind_double(s,2,(double)dp->percent);
        sqlite3_bind_int(s,3,(int)dp->active);
        if (db_step_done(s)!=0) return -1;
        dp->id=(i32)sqlite3_last_insert_rowid(db->handle);
    } else {
        sqlite3_stmt *s = db_prep(db,
            "UPDATE discount_presets SET name=?,percent=?,active=? WHERE id=?;");
        if (!s) return -1;
        sqlite3_bind_text(s,1,dp->name,-1,SQLITE_STATIC);
        sqlite3_bind_double(s,2,(double)dp->percent);
        sqlite3_bind_int(s,3,(int)dp->active);
        sqlite3_bind_int(s,4,(int)dp->id);
        if (db_step_done(s)!=0) return -1;
    }
    return 0;
}

int db_delete_discount_preset(PosDB *db, i32 id)
{
    sqlite3_stmt *s = db_prep(db,
        "UPDATE discount_presets SET active=0 WHERE id=?;");
    if (!s) return -1;
    sqlite3_bind_int(s,1,(int)id);
    return db_step_done(s);
}
