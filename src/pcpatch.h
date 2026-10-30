/**
 * PC wording: Rocky's own text reworded for a PC (src/pcpatch.c).
 */
#ifndef RKY_PCPATCH_H
#define RKY_PCPATCH_H

/* Build (or reuse) the reworded FEND.PAK in save_dir\pcpatch from the
 * game's or a mod's copy and serve it in place of the disc's. After
 * rky_mods_init and once the game's folder is known; before the game
 * starts. */
void rky_pcpatch_init(const char *game_dir, const char *save_dir);

#endif
