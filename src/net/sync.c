#include "sync.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* POSIX socket headers */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <time.h>

/* ─── Socket helpers ─────────────────────────────────────────── */

static int
socket_set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int
socket_set_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

/* Connect with a timeout (milliseconds). Returns fd >= 0 on success. */
static int
tcp_connect_timeout(const char *host, int port, int timeout_ms)
{
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints = {0};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return -1;
    }

    socket_set_nonblocking(fd);

    int r = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (r == 0) {
        /* Connected immediately */
        socket_set_blocking(fd);
        return fd;
    }

    if (errno != EINPROGRESS) {
        close(fd);
        return -1;
    }

    /* Wait for connection with timeout */
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    r = select(fd + 1, NULL, &wfds, NULL, &tv);
    if (r <= 0) {
        close(fd);
        return -1; /* timeout or error */
    }

    /* Verify connection succeeded */
    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err != 0) {
        close(fd);
        return -1;
    }

    socket_set_blocking(fd);

    /* Set send/recv timeout to 5 s */
    struct timeval rto = {5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rto, sizeof(rto));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &rto, sizeof(rto));

    return fd;
}

/* Send all bytes. Returns 0 on success. */
static int
send_all(int fd, const char *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

/* Read response status line (first line). Returns HTTP status code, or -1. */
static int
read_http_status(int fd)
{
    char line[256];
    int  pos = 0;
    char c;

    while (pos < (int)sizeof(line) - 1) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) break;
        if (c == '\n') break;
        if (c != '\r') line[pos++] = c;
    }
    line[pos] = '\0';

    /* "HTTP/1.1 200 OK" */
    int code = -1;
    if (strncmp(line, "HTTP/", 5) == 0) {
        char *sp = strchr(line, ' ');
        if (sp) code = atoi(sp + 1);
    }
    return code;
}

/* ─── JSON builder ───────────────────────────────────────────── */

/* Minimal JSON serialisation of a Transaction.
   Writes into buf[cap], returns bytes written (without NUL). */
static int
txn_to_json(const Transaction *t, Product *products, int product_count,
             char *buf, int cap)
{
    int n = 0;

    n += snprintf(buf+n, cap-n,
        "{\"id\":%lld,\"timestamp\":%lld,"
        "\"receipt_ref\":\"%s\","
        "\"payment_method\":%d,"
        "\"subtotal\":%.2f,\"discount\":%.2f,"
        "\"tax\":%.2f,\"total\":%.2f,"
        "\"tendered\":%.2f,\"change\":%.2f,"
        "\"items\":[",
        (long long)t->id, (long long)t->timestamp,
        t->receipt_ref,
        (int)t->payment_method,
        (double)t->subtotal, (double)t->discount_total,
        (double)t->tax, (double)t->grand_total,
        (double)t->tendered, (double)t->change_due);

    for (int i = 0; i < t->item_count && n < cap-128; i++) {
        const char *name = "unknown";
        for (int j = 0; j < product_count; j++) {
            if (products[j].id == t->items[i].product_id) {
                name = products[j].key_name;
                break;
            }
        }
        if (i > 0) n += snprintf(buf+n, cap-n, ",");
        n += snprintf(buf+n, cap-n,
            "{\"product_id\":%d,\"name\":\"%s\","
            "\"qty\":%d,\"unit_price\":%.2f,"
            "\"discount_pct\":%.2f,\"line_total\":%.2f}",
            t->items[i].product_id, name,
            t->items[i].qty, (double)t->items[i].unit_price,
            (double)t->items[i].discount_percent,
            (double)t->items[i].line_total);
    }

    n += snprintf(buf+n, cap-n, "]}");
    return n;
}

/* ─── Public API ─────────────────────────────────────────────── */

int net_is_reachable(const char *host, int port, int timeout_ms)
{
    int fd = tcp_connect_timeout(host, port, timeout_ms);
    if (fd >= 0) {
        close(fd);
        return 1;
    }
    return 0;
}

int sync_try(NetConfig *cfg, PosDB *db, Product *products, int product_count)
{
    if (!cfg->enabled) return 0;
    if (!db_is_open(db)) return 0;

    /* Quick network probe (2 s timeout to keep UI responsive) */
    if (!net_is_reachable(cfg->host, cfg->port, 2000)) {
        printf("[sync] offline — skipping\n");
        return 0;
    }

    /* Get pending IDs */
#define SYNC_BATCH 32
    i64 pending[SYNC_BATCH];
    int count = db_get_unsynced(db, pending, SYNC_BATCH);
    if (count == 0) return 0;

    printf("[sync] %d transaction(s) to upload\n", count);

    int synced = 0;
    char json_buf[4096];
    char http_req[5120];

    for (int i = 0; i < count; i++) {
        Transaction t;
        if (db_load_transaction(db, pending[i], &t) != 0) continue;

        int jlen = txn_to_json(&t, products, product_count,
                               json_buf, (int)sizeof(json_buf));
        if (jlen <= 0) continue;

        /* Build HTTP/1.1 POST request */
        int rlen = snprintf(http_req, sizeof(http_req),
            "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Authorization: Bearer %s\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%.*s",
            cfg->path, cfg->host, jlen, cfg->api_key, jlen, json_buf);

        int fd = tcp_connect_timeout(cfg->host, cfg->port, 3000);
        if (fd < 0) {
            printf("[sync] connect failed for txn %lld\n", (long long)pending[i]);
            break; /* offline — stop trying */
        }

        int ok = (send_all(fd, http_req, (size_t)rlen) == 0);
        int status_code = -1;
        if (ok) status_code = read_http_status(fd);
        close(fd);

        if (status_code >= 200 && status_code < 300) {
            db_mark_synced(db, pending[i]);
            synced++;
            printf("[sync] synced txn %lld (HTTP %d)\n",
                   (long long)pending[i], status_code);
        } else {
            printf("[sync] txn %lld failed (HTTP %d)\n",
                   (long long)pending[i], status_code);
        }
    }

    printf("[sync] done: %d/%d uploaded\n", synced, count);
    return synced;
}
