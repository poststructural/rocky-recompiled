/**
 * Mods: files that replace the game's own (src/mods.c).
 */
#ifndef RKY_MODS_H
#define RKY_MODS_H

#define RKY_MAX_MODS 64

typedef struct {
    char name[64];
    char path[520];
    int enabled;
    int files;      /* files it replaces (0 while switched off) */
} RkyModInfo;

/* Find the mods (exe_dir ends in a separator; save_dir does not), index their
 * files, clear the cache copies that changed, and hook the runtime's paths.
 * Before the game starts. */
void rky_mods_init(const char *exe_dir, const char *save_dir);

/* The mods found, in load order. */
int rky_mods_list(const RkyModInfo **mods);

/* Switch mod i on or off for the next start (the "disabled" file). */
int rky_mod_set_enabled(int i, int on);

#endif
