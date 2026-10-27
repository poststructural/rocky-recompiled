/*
 * settings.ini: the player's options for the Vulkan build.
 *
 * Read once at start-up, before the window and the GPU model exist. The
 * values that depend on the display (the window size, the picture shape, the
 * render scale) are resolved by main_vk.c once it knows the monitor; the rest
 * map straight onto the runtime switch that does the job (NV2A_MSAA and so
 * on). Environment variables always win, for testing. A missing file is
 * written out with the defaults and a comment per key. docs/SETTINGS.md is
 * the reference the launcher follows.
 *
 * Where: settings.ini beside the executable when it is writable (a portable
 * install), else in the save directory.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"

static void set_default(const char *name, const char *value)
{
    if (getenv(name))
        return;
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 0);
#endif
}

static const char DEFAULT_INI[] =
    "; Rocky (static recompilation) settings.\n"
    "; Delete this file to get the defaults back. Reference: docs/SETTINGS.md\n"
    "\n"
    "[display]\n"
    "; windowed, fullscreen (borderless, at the monitor's resolution) or exclusive\n"
    "; F11 or Alt+Enter switches between windowed and fullscreen in game.\n"
    "mode = windowed\n"
    "; Window size: 1280x720, 1920x1080, 2560x1440, 3840x2160 ... or native\n"
    "; (the monitor's resolution). Fullscreen always uses the monitor's.\n"
    "resolution = 1920x1080\n"
    "; Picture shape: 4:3 (original), 16:9, 16:10, 21:9, 32:9, or auto (the\n"
    "; monitor's). Wider shapes show more of the world at the sides.\n"
    "aspect = auto\n"
    "; on (no tearing), off, or adaptive\n"
    "vsync = on\n"
    "; on shows the frame rate (F10 toggles it in game)\n"
    "show_fps = off\n"
    "\n"
    "[graphics]\n"
    "; Internal resolution as a multiple of the console's 640x480, 1 to 8, or\n"
    "; auto: enough for the output resolution (3 at 1080p, 4 at 1440p, 6 at 4K).\n"
    "render_scale = auto\n"
    "; Upper limit for auto, to keep slower GPUs at 60 fps\n"
    "max_render_scale = 6\n"
    "; smooth (filtered, mipmapped), original (as the Xbox drew it), or\n"
    "; 2, 4, 8 or 16 (anisotropic)\n"
    "texture_filter = smooth\n"
    "\n"
    "[game]\n"
    "; on skips the publisher logos and intro movie at start-up\n"
    "skip_intro = off\n"
    "\n"
    "[launcher]\n"
    "; on starts the game straight away (Rocky.exe --launcher shows it anyway)\n"
    "skip_launcher = off\n"
    "; on looks for a new release on GitLab when the launcher opens\n"
    "update_check = on\n";

static int truthy(const char *v)
{
    return !strcmp(v, "1") || !strcmp(v, "on") || !strcmp(v, "true") ||
           !strcmp(v, "yes");
}

static void lower(char *p)
{
    for (; *p; p++)
        *p = (char)tolower((unsigned char)*p);
}

static double parse_aspect(const char *v)
{
    double a, b;
    if (!strcmp(v, "auto") || !strcmp(v, "native"))
        return 0;
    if (sscanf(v, "%lf:%lf", &a, &b) == 2 && a > 0 && b > 0)
        return a / b;
    if (sscanf(v, "%lf", &a) == 1 && a >= 1.0 && a <= 4.0)
        return a;
    return 4.0 / 3.0;
}

static void apply(RkySettings *s, const char *section, const char *key,
                  char *val)
{
    int v = atoi(val);

    lower(val);
    if (!strcmp(section, "display")) {
        if (!strcmp(key, "mode")) {
            s->mode = !strcmp(val, "windowed") ? RKY_WINDOWED
                    : !strcmp(val, "exclusive") ? RKY_FULLSCREEN
                    : RKY_BORDERLESS;
        } else if (!strcmp(key, "resolution")) {
            int w, h;
            if (!strcmp(val, "native"))
                s->width = s->height = 0;
            else if (sscanf(val, "%dx%d", &w, &h) == 2 && w >= 640 && h >= 480)
                s->width = w, s->height = h;
        } else if (!strcmp(key, "aspect")) {
            s->aspect = parse_aspect(val);
        } else if (!strcmp(key, "vsync")) {
            s->vsync = (!strcmp(val, "adaptive") || v == 2) ? 2 : (truthy(val) ? 1 : 0);
        } else if (!strcmp(key, "show_fps")) {
            s->show_fps = truthy(val);
        }
        /* Keys from the first settings.ini, still honoured. */
        else if (!strcmp(key, "width") && v >= 320)  s->width = v;
        else if (!strcmp(key, "height") && v >= 240) s->height = v;
        else if (!strcmp(key, "fullscreen"))
            s->mode = v == 1 ? RKY_BORDERLESS : v == 2 ? RKY_BORDERLESS : RKY_WINDOWED;
        else if (!strcmp(key, "hud"))
            s->hud_stretch = !strcmp(val, "stretch");
        else if (!strcmp(key, "widescreen"))
            s->aspect = truthy(val) ? 16.0 / 9.0 : 4.0 / 3.0;
    } else if (!strcmp(section, "graphics")) {
        if (!strcmp(key, "render_scale"))
            s->render_scale = (v >= 1 && v <= 8) ? v : 0;
        else if (!strcmp(key, "max_render_scale") && v >= 1 && v <= 8)
            s->max_render_scale = v;
        else if (!strcmp(key, "msaa") && (v == 1 || v == 2 || v == 4 || v == 8))
            s->msaa = v;
        else if (!strcmp(key, "texture_filter"))
            snprintf(s->texture_filter, sizeof s->texture_filter, "%s", val);
    } else if (!strcmp(section, "game")) {
        if (!strcmp(key, "skip_intro"))
            s->skip_intro = truthy(val);
    } else if (!strcmp(section, "launcher")) {
        if (!strcmp(key, "skip_launcher"))
            s->skip_launcher = truthy(val);
        else if (!strcmp(key, "update_check"))
            s->update_check = truthy(val);
    }
}

static char *trim(char *p)
{
    char *e;
    while (isspace((unsigned char)*p))
        p++;
    e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1]))
        *--e = 0;
    return p;
}

/* Change one key in settings.ini, keeping every other line (and the
 * comments) as it was; a key the file lacks is added at the end of its
 * section, a section it lacks at the end of the file. */
int rky_settings_set(const char *path, const char *section, const char *key,
                     const char *value)
{
    enum { MAX_LINES = 512 };
    static char lines[MAX_LINES][512];
    char cur[64] = "";
    int n = 0, done = 0, sec_end = -1;
    FILE *f = fopen(path, "r");

    if (f) {
        while (n < MAX_LINES && fgets(lines[n], sizeof lines[n], f))
            n++;
        fclose(f);
    }
    for (int i = 0; i < n && !done; i++) {
        char tmp[512], *p, *eq;
        snprintf(tmp, sizeof tmp, "%s", lines[i]);
        p = trim(tmp);
        if (*p == '[') {
            char *e = strchr(p, ']');
            if (e) {
                *e = 0;
                snprintf(cur, sizeof cur, "%s", trim(p + 1));
            }
            continue;
        }
        if (!strcmp(cur, section) && *p && *p != ';' && *p != '#')
            sec_end = i;
        eq = strchr(p, '=');
        if (!strcmp(cur, section) && eq && *p != ';' && *p != '#') {
            *eq = 0;
            if (!strcmp(trim(p), key)) {
                snprintf(lines[i], sizeof lines[i], "%s = %s\n", key, value);
                done = 1;
            }
        }
    }
    if (!done && n < MAX_LINES - 2) {
        if (sec_end >= 0) {
            memmove(lines[sec_end + 2], lines[sec_end + 1],
                    (size_t)(n - sec_end - 1) * sizeof lines[0]);
            snprintf(lines[sec_end + 1], sizeof lines[0], "%s = %s\n", key, value);
            n++;
        } else {
            snprintf(lines[n++], sizeof lines[0], "\n[%s]\n", section);
            snprintf(lines[n++], sizeof lines[0], "%s = %s\n", key, value);
        }
    }
    f = fopen(path, "w");
    if (!f)
        return 0;
    for (int i = 0; i < n; i++)
        fputs(lines[i], f);
    fclose(f);
    return 1;
}

int rky_settings_auto_scale(int out_w, int out_h, int cap)
{
    int s = 1;
    while (s < cap && (640 * s < out_w || 480 * s < out_h))
        s++;
    return s;
}

void rky_settings_load(RkySettings *s, const char *exe_dir, const char *save_dir)
{
    char path[4200], line[512], section[64] = "";
    FILE *f;

    memset(s, 0, sizeof *s);
    s->width = 1920;
    s->height = 1080;
    s->mode = RKY_WINDOWED;
    s->aspect = 0;
    s->vsync = 1;
    s->render_scale = 0;
    s->max_render_scale = 6;
    s->msaa = 1;
    s->update_check = 1;
    snprintf(s->texture_filter, sizeof s->texture_filter, "smooth");

    snprintf(path, sizeof path, "%ssettings.ini", exe_dir);
    f = fopen(path, "r");
    if (!f) {
        /* Portable if the exe's folder takes the file, else the save folder. */
        f = fopen(path, "w");
        if (!f) {
            snprintf(path, sizeof path, "%s/settings.ini", save_dir);
            f = fopen(path, "r");
            if (!f)
                f = fopen(path, "w");
            else
                goto parse;
        }
        if (f) {
            fputs(DEFAULT_INI, f);
            fclose(f);
            f = fopen(path, "r");
        }
    }
parse:
    snprintf(s->path, sizeof s->path, "%s", path);
    if (f) {
        while (fgets(line, sizeof line, f)) {
            char *p = trim(line), *eq;
            if (!*p || *p == ';' || *p == '#')
                continue;
            if (*p == '[') {
                char *e = strchr(p, ']');
                if (e) {
                    *e = 0;
                    snprintf(section, sizeof section, "%s", trim(p + 1));
                }
                continue;
            }
            eq = strchr(p, '=');
            if (!eq)
                continue;
            *eq = 0;
            apply(s, section, trim(p), trim(eq + 1));
        }
        fclose(f);
    }
}

/* The settings that reach the renderer through its environment switches.
 * Once, after the launcher (whose changes are in s by then). */
void rky_settings_apply_env(const RkySettings *s)
{
    if (s->show_fps)
        set_default("NV2A_SHOW_FPS", "1");
    if (s->msaa > 1) {
        char m[4];
        snprintf(m, sizeof m, "%d", s->msaa);
        set_default("NV2A_MSAA", m);
    }
    set_default("NV2A_TEX_FILTER", s->texture_filter);
}
