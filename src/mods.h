/**
 * Mods: files that replace the game's own (src/mods.c).
 */
#ifndef RKY_MODS_H
#define RKY_MODS_H

#include <wchar.h>

#define RKY_MAX_MODS 64

typedef struct {
    char name[64];
    char path[520];
    int enabled;
    int files;      /* files it replaces (0 while switched off) */
} RkyModInfo;

/* Find the mods (exe_dir ends in a separator; save_dir does not) and index
 * their files. */
void rky_mods_init(const char *exe_dir, const char *save_dir);

/* Clear the cache copies that changed and hook the runtime's paths. Once,
 * after rky_mods_init and rky_mods_add_file, before the game starts. */
void rky_mods_finish(const char *save_dir);

/* The file a mod serves for rel (a path below the disc root), as a host
 * path in out; 0 when no mod replaces it. */
int rky_mods_host_path(const char *rel, wchar_t *out, int size);

/* Serve path in place of the game's file rel, over any mod's (the PC
 * wording, src/pcpatch.c). After rky_mods_init. */
void rky_mods_add_file(const char *rel, const char *path, const char *tag);

/* The mods found, in load order. */
int rky_mods_list(const RkyModInfo **mods);

/* Switch mod i on or off for the next start (the "disabled" file). */
int rky_mod_set_enabled(int i, int on);

#endif
