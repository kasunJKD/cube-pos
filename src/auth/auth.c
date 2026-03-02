#include "auth.h"
#include "sha256.h"
#include <string.h>

void auth_hash_pin(const char *pin, char out[65])
{
    sha256_hex(pin, strlen(pin), out);
}

int auth_verify_pin(const User *u, const char *pin)
{
    /* Empty pin_hash means no PIN required (open account) */
    if (u->pin_hash[0] == '\0') return 1;

    char hash[65];
    auth_hash_pin(pin, hash);
    return (strncmp(hash, u->pin_hash, 64) == 0) ? 1 : 0;
}

const User *auth_find_by_pin(const User *users, int count, const char *pin)
{
    if (!pin || pin[0] == '\0') return NULL;
    char hash[65];
    auth_hash_pin(pin, hash);
    for (int i = 0; i < count; i++) {
        if (!users[i].active) continue;
        if (strncmp(hash, users[i].pin_hash, 64) == 0)
            return &users[i];
    }
    return NULL;
}

void auth_logout(Session *sess)
{
    memset(sess, 0, sizeof(*sess));
}
