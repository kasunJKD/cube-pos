#ifndef POS_SYNC_H
#define POS_SYNC_H

/*
 * Offline-first network sync for the POS system.
 *
 * Strategy:
 *   - Every completed transaction is saved to the local SQLite DB with
 *     sync_status=PENDING.
 *   - sync_try() is called periodically from the main loop (e.g. every
 *     30 seconds or on each sale).  It does nothing if offline.
 *   - If a TCP connection to the configured host:port succeeds, each
 *     pending transaction is serialised as a minimal JSON payload and
 *     HTTP-POSTed.  On HTTP 2xx the sync_queue row is marked SYNCED.
 *   - No libcurl dependency — uses raw POSIX sockets + blocking I/O
 *     on a best-effort basis.  The main loop is never stalled: the
 *     socket has a short connect timeout (3 s) and a send/recv timeout
 *     (5 s).
 *
 * Configuration: set fields in NetConfig before calling sync_try().
 */

#include "../core/core.h"
#include "../db/db.h"

#define SYNC_MAX_HOST 128
#define SYNC_MAX_PATH 128

typedef struct {
    char  host[SYNC_MAX_HOST];   /* e.g. "api.example.com"  */
    int   port;                  /* e.g. 443 (HTTPS not supported yet: use 80) */
    char  path[SYNC_MAX_PATH];   /* e.g. "/pos/transactions" */
    char  api_key[128];          /* sent as Authorization: Bearer <key> */
    int   enabled;               /* 0 = sync disabled */
} NetConfig;

/*
 * Try to sync all PENDING transactions to the server.
 * Non-blocking in the sense that it returns immediately if:
 *   - config.enabled == 0
 *   - network probe (TCP connect) fails (offline)
 * Otherwise it iterates pending IDs and posts each one.
 *
 * Returns number of transactions successfully synced (>=0), or -1 on
 * fatal config error.
 */
int sync_try(NetConfig *cfg, PosDB *db, Product *products, int product_count);

/*
 * Quick TCP reachability probe.
 * Returns 1 if host:port is reachable within timeout_ms, 0 otherwise.
 */
int net_is_reachable(const char *host, int port, int timeout_ms);

#endif /* POS_SYNC_H */
