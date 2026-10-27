/**
 * The launcher window before the game (src/launcher.c).
 */
#ifndef RKY_LAUNCHER_H
#define RKY_LAUNCHER_H

#include <stddef.h>

struct RkySettings;

/* Run the launcher. game_dir holds the game's folder (have_game: found) and
 * gets the new one if the player installs from the Game Files page.
 * 1: play; 0: quit. */
int rky_launcher_run(struct RkySettings *s, const char *exe_dir, const char *save_dir,
                     char *game_dir, size_t game_n, int have_game);

/* The PC OPTIONS page's settings (src/options_menu.c). */
void rky_options_init(struct RkySettings *s);

#endif
