#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Set sensible defaults */
static void
config_defaults(PosConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->store_name,      "POS System",          sizeof(cfg->store_name) - 1);
    strncpy(cfg->store_address,   "",                    sizeof(cfg->store_address) - 1);
    strncpy(cfg->currency,        "Rs",                  sizeof(cfg->currency) - 1);
    strncpy(cfg->printer_device,  "/dev/usb/lp0",        sizeof(cfg->printer_device) - 1);
    cfg->display_width      = 1280;
    cfg->display_height     = 720;
    cfg->fullscreen         = 0;
    cfg->sync_enabled       = 0;
    strncpy(cfg->sync_host,       "localhost",           sizeof(cfg->sync_host) - 1);
    cfg->sync_port          = 8080;
    strncpy(cfg->sync_path,       "/pos/transactions",   sizeof(cfg->sync_path) - 1);
    strncpy(cfg->sync_api_key,    "CHANGE_ME",           sizeof(cfg->sync_api_key) - 1);
    cfg->sync_interval_sec  = 30;
    strncpy(cfg->db_path,         "pos.db",              sizeof(cfg->db_path) - 1);
    cfg->input_touchscreen  = 1;   /* default: touchscreen OSK */
}

/* Trim leading + trailing whitespace in-place */
static char*
trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end+1) = '\0';
    return s;
}

int config_load(PosConfig *cfg, const char *path)
{
    config_defaults(cfg);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("[config] '%s' not found — using defaults\n", path);
        return -1;
    }

    char line[512];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);

        /* Skip comments and blank lines */
        if (*p == '#' || *p == ';' || *p == '\0') continue;

        /* Section header */
        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) {
                *end = '\0';
                strncpy(section, p + 1, sizeof(section) - 1);
            }
            continue;
        }

        /* key = value */
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        /* Strip inline comment from value */
        char *comment = strchr(val, '#');
        if (comment) { *comment = '\0'; val = trim(val); }

        /* Dispatch by section + key */
        if (strcmp(section, "store") == 0) {
            if      (strcmp(key, "name")     == 0) strncpy(cfg->store_name,    val, sizeof(cfg->store_name)-1);
            else if (strcmp(key, "address")  == 0) strncpy(cfg->store_address, val, sizeof(cfg->store_address)-1);
            else if (strcmp(key, "currency") == 0) strncpy(cfg->currency,      val, sizeof(cfg->currency)-1);
        }
        else if (strcmp(section, "printer") == 0) {
            if (strcmp(key, "device") == 0) strncpy(cfg->printer_device, val, sizeof(cfg->printer_device)-1);
        }
        else if (strcmp(section, "display") == 0) {
            if      (strcmp(key, "width")      == 0) cfg->display_width  = atoi(val);
            else if (strcmp(key, "height")     == 0) cfg->display_height = atoi(val);
            else if (strcmp(key, "fullscreen") == 0) cfg->fullscreen     = atoi(val);
        }
        else if (strcmp(section, "sync") == 0) {
            if      (strcmp(key, "enabled")  == 0) cfg->sync_enabled      = atoi(val);
            else if (strcmp(key, "host")     == 0) strncpy(cfg->sync_host,    val, sizeof(cfg->sync_host)-1);
            else if (strcmp(key, "port")     == 0) cfg->sync_port          = atoi(val);
            else if (strcmp(key, "path")     == 0) strncpy(cfg->sync_path,    val, sizeof(cfg->sync_path)-1);
            else if (strcmp(key, "api_key")  == 0) strncpy(cfg->sync_api_key, val, sizeof(cfg->sync_api_key)-1);
            else if (strcmp(key, "interval") == 0) cfg->sync_interval_sec  = atoi(val);
        }
        else if (strcmp(section, "db") == 0) {
            if (strcmp(key, "path") == 0) strncpy(cfg->db_path, val, sizeof(cfg->db_path)-1);
        }
        else if (strcmp(section, "input") == 0) {
            if (strcmp(key, "touchscreen") == 0) cfg->input_touchscreen = atoi(val);
        }
    }

    fclose(f);
    printf("[config] loaded: %s\n", path);
    return 0;
}
