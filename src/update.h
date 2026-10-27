/**
 * Updates from the project's GitLab releases (src/update.c).
 */
#ifndef RKY_UPDATE_H
#define RKY_UPDATE_H

enum {
    RKY_UPD_IDLE,        /* not checked yet */
    RKY_UPD_CHECKING,
    RKY_UPD_CURRENT,     /* this is the newest release */
    RKY_UPD_AVAILABLE,   /* a newer one: rky_update_latest() */
    RKY_UPD_DOWNLOADING, /* rky_update_progress() */
    RKY_UPD_READY,       /* installed; restart to use it */
    RKY_UPD_FAILED       /* rky_update_error() */
};

/* Start a check in the background (no-op while one runs). */
void rky_update_check(void);
/* Download, verify and install the newer release in the background. */
void rky_update_install(const char *exe_dir);
int rky_update_state(void);
const char *rky_update_latest(void);    /* its version, e.g. "1.0.1" */
const char *rky_update_notes(void);     /* first line of its notes */
int rky_update_progress(void);          /* 0..100 while downloading */
const char *rky_update_error(void);

/* At start-up: finish a previous update (remove the .old files it left). */
void rky_update_cleanup(const char *exe_dir);
/* Start the new Rocky.exe after an update and return (the caller exits). */
int rky_update_restart(const char *exe_dir);

#endif
