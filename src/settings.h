/* settings.ini for the Vulkan build (src/settings.c). docs/SETTINGS.md lists
 * every key, for the launcher and for players editing the file by hand. */
#ifndef RKY_SETTINGS_H
#define RKY_SETTINGS_H

enum { RKY_WINDOWED = 0, RKY_FULLSCREEN = 1, RKY_BORDERLESS = 2 };

typedef struct RkySettings {
    /* [display] */
    int width, height;      /* window size; 0x0 = the monitor's resolution */
    int mode;               /* RKY_WINDOWED / RKY_FULLSCREEN / RKY_BORDERLESS */
    double aspect;          /* picture shape; 0 = the monitor's */
    int vsync;              /* 0 off, 1 on, 2 adaptive */
    int show_fps;
    int hud_stretch;        /* 0 = 2D keeps its 4:3 shape on wide pictures */
    /* [graphics] */
    int render_scale;       /* 0 = auto: enough for the output resolution */
    int max_render_scale;   /* cap for auto */
    int msaa;               /* 1, 2, 4, 8 */
    char texture_filter[16];
    /* [game] */
    int skip_intro;
    /* [launcher] */
    int skip_launcher;      /* straight into the game */
    int update_check;       /* look for a new release when the launcher opens */
    char path[4200];        /* where the file was read from */
} RkySettings;

/* exe_dir ends in a separator; save_dir does not. */
void rky_settings_load(RkySettings *s, const char *exe_dir, const char *save_dir);

/* Hand the renderer its settings (NV2A_* switches; set ones are kept). */
void rky_settings_apply_env(const RkySettings *s);

/* The render scale for a picture of out_w x out_h pixels: the smallest whole
 * multiple of 640x480 at least as large on both axes, so the picture is never
 * magnified (the framebuffer stays 4:3 and is drawn wider for widescreen). */
int rky_settings_auto_scale(int out_w, int out_h, int cap);

/* Write one key into settings.ini at path, keeping the rest of the file. */
int rky_settings_set(const char *path, const char *section, const char *key,
                     const char *value);

#endif
