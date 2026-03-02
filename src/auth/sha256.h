#ifndef POS_SHA256_H
#define POS_SHA256_H
/* Minimal public-domain SHA-256. No external deps. */
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buf[64];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t digest[32]);

/* Convenience: hash data → 64-char hex string + NUL into out[65] */
void sha256_hex(const char *data, size_t len, char out[65]);

#endif
