/**
 * Finding the game's files, and installing them from the player's disc
 * (src/install.c).
 */
#ifndef RKY_INSTALL_H
#define RKY_INSTALL_H

#include <stddef.h>
#include <stdint.h>

/* 1 and the folder in out: the game was found (see install.c for where). */
int rky_find_game(const char *arg, const char *save_dir, const char *exe_dir,
                  char *out, size_t n);

/* Ask for the disc and install it; 1 and the folder in out on success, 0 if
 * the player cancelled. */
int rky_first_run(const char *save_dir, char *out, size_t n);

/* Progress of an install: total 0 = unknown length (unpacking an archive). */
typedef void (*rky_progress_fn)(uint64_t done, uint64_t total, void *ud);

/* Install from src (an ISO, a .zip/.7z holding one, or an extracted
 * default.xbe) into save_dir; the game's folder goes to game_dir. */
int rky_install(const char *src, const char *save_dir, char *game_dir, size_t n,
                rky_progress_fn progress, void *ud);

/* 1: dir/default.xbe is the USA retail XBE this build was made from. */
int rky_check_xbe(const char *dir);

/* Why the last call failed, for the player. */
const char *rky_install_error(void);

#endif
