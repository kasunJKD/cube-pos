#ifndef POS_CONFIG_H
#define POS_CONFIG_H

/*
 * INI-style config file parser.
 * Reads pos.conf and fills PosConfig.
 * Unknown keys are silently ignored.
 */

typedef struct {
    /* [store] */
    char store_name[64];
    char store_address[128];
    char currency[8];

    /* [printer] */
    char printer_device[64];

    /* [display] */
    int  display_width;
    int  display_height;
    int  fullscreen;

    /* [sync] */
    int  sync_enabled;
    char sync_host[128];
    int  sync_port;
    char sync_path[128];
    char sync_api_key[128];
    int  sync_interval_sec;

    /* [db] */
    char db_path[256];

    /* [input] */
    int  input_touchscreen;   /* 1 = on-screen keyboard pops up on tap
                                 0 = hardware keyboard via SDL text input */
} PosConfig;

/* Fill *cfg with defaults, then load and override from file.
   Returns 0 if file was read (even partially), -1 if file not found
   (defaults are still applied — safe to continue). */
int config_load(PosConfig *cfg, const char *path);

#endif /* POS_CONFIG_H */
