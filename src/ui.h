/**
 * Menu drawing in the look of Rocky's own menus (src/ui.c), shared by the
 * PC OPTIONS overlay and the launcher: a gold title, rows of label and
 * value, the chosen row on a gold bar.
 */
#ifndef RKY_UI_H
#define RKY_UI_H

#include <stdint.h>

/* Menu input, from keys, a controller or the mouse. */
enum { RKY_IN_UP = 1, RKY_IN_DOWN, RKY_IN_LEFT, RKY_IN_RIGHT, RKY_IN_OK, RKY_IN_CLOSE };

/* The PC OPTIONS page (src/options_menu.c), also shown by the launcher.
 * live: over the running game (changes reach the renderer at once). */
struct RkySettings;
void rky_options_page_open(int live);
void rky_options_page_draw(uint32_t *out, int w, int h, const uint32_t *backdrop,
                           int x0, int y0, int pw, int ph);
int rky_options_page_input(int in);    /* 1: closed */
int rky_options_page_click(int row);   /* 1: closed */

typedef struct {
    const char *label;
    const char *value;      /* NULL: a centred button row (Back, Play ...) */
    int gap_before;         /* a little space above this row */
    int dim;                /* greyed out (not available) */
} RkyUiRow;

/* Draw a menu into out (w x h, 0xAARRGGBB). backdrop: a picture to darken
 * behind it (w x h), or NULL for the launcher's own background. The menu is
 * laid out in the 4:3 area (x0, y0, pw, ph). note: a line under the rows
 * (status, errors); footer: the key help at the bottom. Either may be NULL. */
void rky_ui_menu(uint32_t *out, int w, int h, const uint32_t *backdrop,
                 int x0, int y0, int pw, int ph, const char *title,
                 const RkyUiRow *rows, int n, int sel, const char *note,
                 const char *footer);

/* The row under a point of the window, or -1 (mouse), for the last layout
 * drawn. */
int rky_ui_row_at(int x, int y);

#endif
