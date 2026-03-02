#ifndef POS_AUTH_H
#define POS_AUTH_H

/*
 * Authentication + session management.
 *
 * Login flow:
 *   1. App starts → SCREEN_LOGIN (PIN-entry screen, no user list shown)
 *   2. Operator enters their PIN on the numpad and presses OK.
 *   3. auth_find_by_pin() scans all users → returns the matching User.
 *   4. Session is set; ROLE_ADMIN gets "Edit Mode" in the POS toolbar.
 *   5. On log-out or timeout → SCREEN_LOGIN.
 *
 * PIN is stored as SHA-256(pin_string) in hex.
 */

#include "../core/core.h"
#include "../db/db.h"

typedef struct {
    i8   logged_in;
    User user;           /* copy of the logged-in user record */
    u32  login_time_ms;  /* SDL_GetTicks() at login */
} Session;

/* Hash a PIN string → hex. out must be char[65]. */
void auth_hash_pin(const char *pin, char out[65]);

/* Verify pin against a specific user. Returns 1 if correct, 0 if wrong. */
int  auth_verify_pin(const User *u, const char *pin);

/* Scan a users array for the one whose PIN matches.
   Returns a pointer into the array on success, NULL if no match.
   Only active users are considered. */
const User *auth_find_by_pin(const User *users, int count, const char *pin);

/* Log out — clears session. */
void auth_logout(Session *sess);

#endif /* POS_AUTH_H */
