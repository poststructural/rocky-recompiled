/**
 * PC OPTIONS: a settings screen over the paused game, in the look of
 * Rocky's own menus (a gold title over the frozen frame, the chosen row on a
 * gold bar).
 *
 * F1, or Back + Start on a controller, opens it anywhere. Up/Down choose a
 * row, Left/Right change it, Esc / B / F1 close it. Changes take effect at
 * once where the renderer allows (picture shape, render scale, filtering,
 * V-Sync, frame counter) and otherwise when the screen closes (window mode
 * and size); every change is written to settings.ini.
 *
 * The presenter's host overlay (nv2a_overlay_wanted/nv2a_overlay_frame)
 * holds the game at its frame while this draws. The text is drawn with
 * Windows' GDI into a bitmap; the window's events arrive through
 * nv2a_window_event_hook on the main thread and are queued for the
 * renderer's thread, which owns the menu.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "nv2a_window.h"
#include "settings.h"
#include "ui.h"

extern float rky_aspect;                  /* recomp_manual.c */
extern int rky_skip_intro;
extern void rky_apply_aspect(float aspect);
extern void nv2a_quality_get(int *render_scale, int *anisotropy, int *shadow_scale, int *msaa);

/* ── Options ───────────────────────────────────────────────── */

enum { OPT_MODE, OPT_RES, OPT_ASPECT, OPT_SCALE, OPT_FILTER, OPT_VSYNC, OPT_FPS,
       OPT_INTRO, OPT_BACK, OPT_COUNT };

static const char *const MODE_NAMES[] = { "Windowed", "Fullscreen", "Borderless" };
static const int RES[][2] = { { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 },
                              { 3440, 1440 }, { 3840, 2160 }, { 1280, 960 }, { 1920, 1440 } };
#define RES_COUNT ((int)(sizeof RES / sizeof RES[0]))
static const double ASPECTS[] = { 0.0, 4.0 / 3.0, 16.0 / 9.0, 16.0 / 10.0, 21.0 / 9.0, 32.0 / 9.0 };
static const char *const ASPECT_NAMES[] = { "Auto", "4:3", "16:9", "16:10", "21:9", "32:9" };
static const char *const ASPECT_KEYS[] = { "auto", "4:3", "16:9", "16:10", "21:9", "32:9" };
static const int FILTERS[] = { 0, 2, 4, 8, 16 };
static const char *const FILTER_NAMES[] = { "Console", "2x Anisotropic", "4x Anisotropic",
                                            "8x Anisotropic", "16x Anisotropic" };
static const char *const VSYNC_NAMES[] = { "Off", "On", "Adaptive" };

static RkySettings *s_set;
static int s_mode, s_res, s_aspect_i, s_scale, s_filter_i, s_vsync, s_fps, s_intro;
static int s_sel;
static int s_window_dirty;
/* 1 over the running game (changes reach the renderer at once); 0 in the
 * launcher, where they are only saved and apply when the game starts. */
static int s_live;

static int nearest_res(int w, int h)
{
    for (int i = 0; i < RES_COUNT; i++)
        if (RES[i][0] == w && RES[i][1] == h)
            return i;
    return 2;
}

static int s_set_filter = -1;   /* the filter chosen in this run, or -1 */

static void load_values(void)
{
    int scale, aniso, shadow, msaa;
    if (s_live) {
        nv2a_quality_get(&scale, &aniso, &shadow, &msaa);
    } else {
        const char *f = s_set->texture_filter;
        aniso = s_set_filter >= 0 ? s_set_filter
              : !strcmp(f, "original") || !strcmp(f, "console") ? 0
              : !strcmp(f, "smooth") ? 16 : atoi(f);
    }
    s_mode = s_set->mode;
    s_res = nearest_res(s_set->width, s_set->height);
    s_aspect_i = 0;
    for (int i = 1; i < 6; i++)
        if (s_set->aspect > 0 && s_set->aspect > ASPECTS[i] - 0.01 && s_set->aspect < ASPECTS[i] + 0.01)
            s_aspect_i = i;
    s_scale = s_set->render_scale;           /* 0 = auto */
    s_filter_i = 0;
    for (int i = 0; i < 5; i++)
        if (FILTERS[i] == aniso)
            s_filter_i = i;
    s_vsync = s_set->vsync;
    s_fps = s_set->show_fps;
    s_intro = s_set->skip_intro;
}

static void row_text(int i, char *label, char *value, size_t n)
{
    static const char *const LABELS[] = { "Display Mode", "Window Size", "Aspect Ratio",
                                          "Render Resolution", "Texture Filtering", "V-Sync",
                                          "FPS Counter", "Skip Intro Movies", "Back" };
    snprintf(label, 64, "%s", LABELS[i]);
    value[0] = 0;
    switch (i) {
    case OPT_MODE:   snprintf(value, n, "%s", MODE_NAMES[s_mode]); break;
    case OPT_RES:    snprintf(value, n, "%d x %d", RES[s_res][0], RES[s_res][1]); break;
    case OPT_ASPECT: snprintf(value, n, "%s", ASPECT_NAMES[s_aspect_i]); break;
    case OPT_SCALE:
        if (s_scale) snprintf(value, n, "%dx  (%d x %d)", s_scale, 640 * s_scale, 480 * s_scale);
        else snprintf(value, n, "Auto");
        break;
    case OPT_FILTER: snprintf(value, n, "%s", FILTER_NAMES[s_filter_i]); break;
    case OPT_VSYNC:  snprintf(value, n, "%s", VSYNC_NAMES[s_vsync]); break;
    case OPT_FPS:    snprintf(value, n, "%s", s_fps ? "On" : "Off"); break;
    case OPT_INTRO:  snprintf(value, n, "%s", s_intro ? "On" : "Off"); break;
    }
}

/* Render scale for Auto: enough for the picture in the current window. */
static int auto_scale(void)
{
    int w = 1920, h = 1080, x0, y0, dw, dh;
    if (s_mode != RKY_WINDOWED) {
        w = GetSystemMetrics(SM_CXSCREEN);
        h = GetSystemMetrics(SM_CYSCREEN);
    } else {
        w = RES[s_res][0];
        h = RES[s_res][1];
    }
    nv2a_present_picture_rect(w, h, &x0, &y0, &dw, &dh);
    return rky_settings_auto_scale(dw, dh, s_set->max_render_scale ? s_set->max_render_scale : 6);
}

static void save(const char *section, const char *key, const char *value)
{
    rky_settings_set(s_set->path, section, key, value);
}

static void change(int dir)
{
    char buf[32];
    int scale, aniso, shadow, msaa;

    switch (s_sel) {
    case OPT_MODE:
        s_mode = (s_mode + 3 + dir) % 3;
        save("display", "mode", s_mode == RKY_WINDOWED ? "windowed"
                              : s_mode == RKY_FULLSCREEN ? "exclusive" : "fullscreen");
        s_set->mode = s_mode;
        s_window_dirty = 1;
        break;
    case OPT_RES:
        s_res = (s_res + RES_COUNT + dir) % RES_COUNT;
        snprintf(buf, sizeof buf, "%dx%d", RES[s_res][0], RES[s_res][1]);
        save("display", "resolution", buf);
        s_set->width = RES[s_res][0];
        s_set->height = RES[s_res][1];
        s_window_dirty = 1;
        break;
    case OPT_ASPECT: {
        double a;
        s_aspect_i = (s_aspect_i + 6 + dir) % 6;
        save("display", "aspect", ASPECT_KEYS[s_aspect_i]);
        s_set->aspect = ASPECTS[s_aspect_i];
        a = ASPECTS[s_aspect_i];
        if (a <= 0) {
            int w = s_mode == RKY_WINDOWED ? RES[s_res][0] : GetSystemMetrics(SM_CXSCREEN);
            int h = s_mode == RKY_WINDOWED ? RES[s_res][1] : GetSystemMetrics(SM_CYSCREEN);
            a = h ? (double)w / h : 16.0 / 9.0;
        }
        if (a < 4.0 / 3.0) a = 4.0 / 3.0;
        if (s_live)
            rky_apply_aspect((float)a);
        break;
    }
    case OPT_SCALE:
        s_scale = (s_scale + 9 + dir) % 9;   /* 0 = auto, 1..8 */
        if (s_scale) snprintf(buf, sizeof buf, "%d", s_scale);
        save("graphics", "render_scale", s_scale ? buf : "auto");
        s_set->render_scale = s_scale;
        if (s_live) {
            nv2a_quality_get(&scale, &aniso, &shadow, &msaa);
            nv2a_set_quality(s_scale ? s_scale : auto_scale(), aniso, shadow, msaa);
        }
        break;
    case OPT_FILTER:
        s_filter_i = (s_filter_i + 5 + dir) % 5;
        if (FILTERS[s_filter_i]) snprintf(buf, sizeof buf, "%d", FILTERS[s_filter_i]);
        save("graphics", "texture_filter", FILTERS[s_filter_i] ? buf : "original");
        s_set_filter = FILTERS[s_filter_i];
        if (s_live) {
            nv2a_quality_get(&scale, &aniso, &shadow, &msaa);
            nv2a_set_quality(scale, FILTERS[s_filter_i], shadow, msaa);
        }
        break;
    case OPT_VSYNC:
        s_vsync = (s_vsync + 3 + dir) % 3;
        save("display", "vsync", s_vsync == 0 ? "off" : s_vsync == 1 ? "on" : "adaptive");
        s_set->vsync = s_vsync;
        if (s_live)
            nv2a_present_set_vsync(s_vsync);
        break;
    case OPT_FPS:
        s_fps = !s_fps;
        save("display", "show_fps", s_fps ? "on" : "off");
        s_set->show_fps = s_fps;
        if (s_live)
            nv2a_present_set_show_fps(s_fps != 0);
        break;
    case OPT_INTRO:
        s_intro = !s_intro;
        save("game", "skip_intro", s_intro ? "on" : "off");
        s_set->skip_intro = s_intro;
        rky_skip_intro = s_intro;
        break;
    }
}

/* ── Input (main thread -> renderer thread) ────────────────── */

enum { IN_UP = RKY_IN_UP, IN_DOWN = RKY_IN_DOWN, IN_LEFT = RKY_IN_LEFT, IN_RIGHT = RKY_IN_RIGHT,
       IN_OK = RKY_IN_OK, IN_CLOSE = RKY_IN_CLOSE };
static volatile LONG s_open_req;
static volatile LONG s_queue[32];
static volatile LONG s_qhead, s_qtail;
static volatile int s_open;
static int s_back_held;

static void push(int in)
{
    LONG t = s_qtail;
    if (((t + 1) & 31) == (s_qhead & 31))
        return;
    s_queue[t & 31] = in;
    InterlockedExchange(&s_qtail, (t + 1) & 31);
}

static int pop(void)
{
    LONG h = s_qhead;
    if (h == s_qtail)
        return 0;
    int in = (int)s_queue[h & 31];
    InterlockedExchange(&s_qhead, (h + 1) & 31);
    return in;
}

void nv2a_window_event_hook(const void *sdl_event)
{
    const SDL_Event *ev = sdl_event;
    if (ev->type == SDL_KEYDOWN) {
        SDL_Keycode k = ev->key.keysym.sym;
        if (k == SDLK_F1 && !ev->key.repeat) {
            if (s_open) push(IN_CLOSE);
            else InterlockedExchange(&s_open_req, 1);
            return;
        }
        if (!s_open)
            return;
        switch (k) {
        case SDLK_UP: case SDLK_w:        push(IN_UP); break;
        case SDLK_DOWN: case SDLK_s:      push(IN_DOWN); break;
        case SDLK_LEFT: case SDLK_a:      push(IN_LEFT); break;
        case SDLK_RIGHT: case SDLK_d:     push(IN_RIGHT); break;
        case SDLK_RETURN: case SDLK_SPACE: case SDLK_k: push(IN_OK); break;
        case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_l: push(IN_CLOSE); break;
        }
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN || ev->type == SDL_CONTROLLERBUTTONUP) {
        int down = ev->type == SDL_CONTROLLERBUTTONDOWN;
        int b = ev->cbutton.button;
        if (b == SDL_CONTROLLER_BUTTON_BACK)
            s_back_held = down;
        if (!down)
            return;
        if (!s_open) {
            if (b == SDL_CONTROLLER_BUTTON_START && s_back_held)
                InterlockedExchange(&s_open_req, 1);
            return;
        }
        switch (b) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    push(IN_UP); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  push(IN_DOWN); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  push(IN_LEFT); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: push(IN_RIGHT); break;
        case SDL_CONTROLLER_BUTTON_A:          push(IN_OK); break;
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_START:      push(IN_CLOSE); break;
        }
    }
}

/* ── The page (overlay and launcher) ───────────────────────── */

void rky_options_page_open(int live)
{
    s_live = live;
    s_sel = 0;
    s_window_dirty = 0;
    load_values();
}

void rky_options_page_draw(uint32_t *out, int w, int h, const uint32_t *backdrop,
                           int x0, int y0, int pw, int ph)
{
    static char labels[OPT_COUNT][64], values[OPT_COUNT][64];
    RkyUiRow rows[OPT_COUNT];
    for (int i = 0; i < OPT_COUNT; i++) {
        row_text(i, labels[i], values[i], sizeof values[i]);
        rows[i].label = labels[i];
        rows[i].value = i == OPT_BACK ? NULL : values[i];
        rows[i].gap_before = i == OPT_BACK;
        rows[i].dim = i == OPT_RES && s_mode != RKY_WINDOWED;
    }
    rky_ui_menu(out, w, h, backdrop, x0, y0, pw, ph, "PC OPTIONS", rows, OPT_COUNT, s_sel,
                s_live ? NULL : "Changes apply when the game starts.",
                "Up/Down: choose    Left/Right: change    Esc / B: back");
}

/* 1: the page closed. */
int rky_options_page_input(int in)
{
    if (in == IN_UP) s_sel = (s_sel + OPT_COUNT - 1) % OPT_COUNT;
    else if (in == IN_DOWN) s_sel = (s_sel + 1) % OPT_COUNT;
    else if (in == IN_LEFT && s_sel != OPT_BACK) change(-1);
    else if ((in == IN_RIGHT || in == IN_OK) && s_sel != OPT_BACK) change(1);
    else if (in == IN_CLOSE || (in == IN_OK && s_sel == OPT_BACK)) {
        if (s_live && s_window_dirty)
            nv2a_window_request(s_mode == RKY_WINDOWED ? RES[s_res][0] : 0,
                                s_mode == RKY_WINDOWED ? RES[s_res][1] : 0, s_mode);
        return 1;
    }
    return 0;
}

/* Choose row r (a mouse click in the launcher), then act as OK. */
int rky_options_page_click(int r)
{
    if (r < 0 || r >= OPT_COUNT)
        return 0;
    s_sel = r;
    return rky_options_page_input(IN_OK);
}

/* ── Overlay hooks (renderer thread) ───────────────────────── */

void rky_options_init(RkySettings *s)
{
    s_set = s;
}

bool nv2a_overlay_wanted(void)
{
    return s_set && InterlockedExchange(&s_open_req, 0);
}

bool nv2a_overlay_frame(uint32_t *px, int w, int h, const uint32_t *backdrop)
{
    int in, x0, y0, pw, ph;
    if (!s_open) {
        s_open = 1;
        rky_options_page_open(1);
        while (pop()) {}
    }
    while ((in = pop())) {
        if (rky_options_page_input(in)) {
            s_open = 0;
            return false;
        }
    }
    nv2a_present_picture_rect(w, h, &x0, &y0, &pw, &ph);
    /* Lay out on the 4:3 middle of the picture, as the game's menus are. */
    pw = ph * 4 / 3 < pw ? ph * 4 / 3 : pw;
    x0 = (w - pw) / 2;
    rky_options_page_draw(px, w, h, backdrop, x0, y0, pw, ph);
    Sleep(15);
    return true;
}
