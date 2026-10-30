/**
 * Menu drawing in the look of Rocky's own menus (see ui.h).
 *
 * Text is drawn with Windows' GDI into a DIB; the gold bar is the olive
 * gold of the game's selected menu item, brighter in the middle and fading
 * out at the ends. Drawing happens on one thread at a time (the launcher
 * before the game, the overlay on the renderer's thread during it).
 *
 * The launcher (no backdrop) has its own look, a fight-night poster: a
 * spotlit ring corner with red, white and blue ropes, the menu down the left
 * in Bebas Neue, the chosen row on a red tape bar that slides to it, an
 * info panel at the top right and controller button chips along the bottom. Bebas
 * Neue and Oswald (SIL OFL 1.1) are in the exe as resources (rocky.rc.in).
 */
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

#ifndef RKY_VERSION
#define RKY_VERSION ""
#endif

typedef struct {
    HDC dc;
    HBITMAP bmp;
    uint32_t *bits;
    int w, h;
    HFONT title, row, small;
} Canvas;

static Canvas s_cv;

/* The last layout, for rky_ui_row_at. */
static int s_rows_n, s_row_y[32], s_row_h, s_row_x0, s_row_x1;

static void canvas_size(int w, int h, int ph)
{
    BITMAPINFO bi = { 0 };
    static int font_ph;
    if (s_cv.w != w || s_cv.h != h) {
        if (!s_cv.dc)
            s_cv.dc = CreateCompatibleDC(NULL);
        if (s_cv.bmp)
            DeleteObject(s_cv.bmp);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        s_cv.bmp = CreateDIBSection(s_cv.dc, &bi, DIB_RGB_COLORS, (void **)&s_cv.bits, NULL, 0);
        SelectObject(s_cv.dc, s_cv.bmp);
        s_cv.w = w;
        s_cv.h = h;
        SetBkMode(s_cv.dc, TRANSPARENT);
    }
    if (font_ph != ph) {
        font_ph = ph;
        if (s_cv.title) DeleteObject(s_cv.title);
        if (s_cv.row) DeleteObject(s_cv.row);
        if (s_cv.small) DeleteObject(s_cv.small);
        s_cv.title = CreateFontA(-ph / 11, 0, 0, 0, FW_BLACK, 0, 0, 0, ANSI_CHARSET, 0, 0,
                                 ANTIALIASED_QUALITY, 0, "Arial Black");
        s_cv.row = CreateFontA(-ph / 26, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0,
                               ANTIALIASED_QUALITY, 0, "Arial Black");
        s_cv.small = CreateFontA(-ph / 34, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0,
                                 ANTIALIASED_QUALITY, 0, "Arial");
    }
}

static uint32_t blend(uint32_t dst, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    uint32_t dr = dst >> 16 & 0xFF, dg = dst >> 8 & 0xFF, db = dst & 0xFF;
    dr = (dr * (255 - a) + r * a) / 255;
    dg = (dg * (255 - a) + g * a) / 255;
    db = (db * (255 - a) + b * a) / 255;
    return 0xFF000000u | dr << 16 | dg << 8 | db;
}

static void gold_bar(int x0, int y0, int x1, int y1)
{
    int w = x1 - x0;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s_cv.w) x1 = s_cv.w;
    if (y1 > s_cv.h) y1 = s_cv.h;
    for (int y = y0; y < y1; y++) {
        float v = 1.0f - 2.0f * (float)abs(2 * (y - y0) - (y1 - y0)) / (float)(y1 - y0) * 0.35f;
        for (int x = x0; x < x1; x++) {
            float e = (float)(x - x0) / (float)w;
            float fade = e < 0.12f ? e / 0.12f : e > 0.88f ? (1.0f - e) / 0.12f : 1.0f;
            uint32_t *p = &s_cv.bits[y * s_cv.w + x];
            *p = blend(*p, (uint32_t)(150 * v), (uint32_t)(140 * v), (uint32_t)(40 * v),
                       (uint32_t)(230 * fade));
        }
    }
    for (int x = x0; x < x1; x++) {   /* thin highlight lines above and below */
        float e = (float)(x - x0) / (float)w;
        uint32_t a = (uint32_t)(200 * (e < 0.5f ? e * 2 : (1 - e) * 2));
        s_cv.bits[y0 * s_cv.w + x] = blend(s_cv.bits[y0 * s_cv.w + x], 230, 210, 120, a);
        s_cv.bits[(y1 - 1) * s_cv.w + x] = blend(s_cv.bits[(y1 - 1) * s_cv.w + x], 230, 210, 120, a);
    }
}

static void text(const char *s, int x, int y, int w, int h, UINT align, COLORREF c, HFONT f, int shadow)
{
    RECT r = { x, y, x + w, y + h };
    SelectObject(s_cv.dc, f);
    if (shadow) {
        RECT rs = { x + shadow, y + shadow, x + w + shadow, y + h + shadow };
        SetTextColor(s_cv.dc, RGB(0, 0, 0));
        DrawTextA(s_cv.dc, s, -1, &rs, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    SetTextColor(s_cv.dc, c);
    DrawTextA(s_cv.dc, s, -1, &r, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

/* ── The launcher's look ───────────────────────────────────── */

/* Fonts from the exe's resources (101 Bebas Neue, 102 Oswald, 103 Oswald
 * Medium), registered for this process once. */
static void load_fonts(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    for (int id = 101; id <= 103; id++) {
        HRSRC r = FindResourceA(NULL, MAKEINTRESOURCEA(id), (LPCSTR)RT_RCDATA);
        HGLOBAL g = r ? LoadResource(NULL, r) : NULL;
        void *p = g ? LockResource(g) : NULL;
        DWORD count = 0;
        if (p)
            AddFontMemResourceEx(p, SizeofResource(NULL, r), NULL, &count);
    }
}

static HFONT make_font(int px, int weight, const char *face)
{
    return CreateFontA(-px, 0, 0, 0, weight, 0, 0, 0, ANSI_CHARSET, OUT_TT_PRECIS, 0,
                       ANTIALIASED_QUALITY, 0, face);
}

/* Launcher fonts, for a window height. */
static struct { int h; HFONT logo, title, row, sel, info, infob, hint; } s_lf;

static void launcher_fonts(int h)
{
    float u = (float)h / 720.0f;
    if (s_lf.h == h)
        return;
    load_fonts();
    if (s_lf.logo) {
        DeleteObject(s_lf.logo); DeleteObject(s_lf.title); DeleteObject(s_lf.row);
        DeleteObject(s_lf.sel); DeleteObject(s_lf.info); DeleteObject(s_lf.infob);
        DeleteObject(s_lf.hint);
    }
    s_lf.h = h;
    s_lf.logo = make_font((int)(150 * u), FW_NORMAL, "Bebas Neue");
    s_lf.title = make_font((int)(96 * u), FW_NORMAL, "Bebas Neue");
    s_lf.row = make_font((int)(38 * u), FW_NORMAL, "Bebas Neue");
    s_lf.sel = make_font((int)(46 * u), FW_NORMAL, "Bebas Neue");
    s_lf.info = make_font((int)(20 * u), FW_NORMAL, "Oswald");
    s_lf.infob = make_font((int)(20 * u), FW_NORMAL, "Oswald Medium");
    s_lf.hint = make_font((int)(18 * u), FW_NORMAL, "Oswald Medium");
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* Distance from (px, py) to the segment a-b. */
static float seg_dist(float px, float py, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay;
    float t = clampf(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy), 0, 1);
    float ex = px - (ax + t * dx), ey = py - (ay + t * dy);
    return sqrtf(ex * ex + ey * ey);
}

static const uint8_t ROPE[3][3] = { { 200, 30, 30 }, { 235, 230, 220 }, { 30, 60, 170 } };

/* The ring corner behind the launcher, drawn once per window size: a dark
 * hall, a spotlight on the canvas, the corner post with its pads, the three
 * ropes, a little grain, and the left side darkened for the menu. */
static uint32_t *s_scene;
static int s_scene_w, s_scene_h;

static void scene_draw(int w, int h)
{
    float u = (float)h / 720.0f, cx = w * 0.62f;
    float px0 = w * 0.600f, px1 = w * 0.600f + 61 * u;   /* the post */
    float rope[3][6];
    free(s_scene);
    s_scene = malloc((size_t)w * h * 4);
    s_scene_w = w;
    s_scene_h = h;
    if (!s_scene)
        return;
    for (int i = 0; i < 3; i++) {   /* left end, at the post, right end */
        float yp = h * 0.56f + i * 42 * u;
        rope[i][0] = -w * 0.05f; rope[i][1] = h * 0.62f + i * 62 * u + 110 * u;
        rope[i][2] = (px0 + px1) / 2; rope[i][3] = yp;
        rope[i][4] = w * 1.05f; rope[i][5] = yp - 30 * u;
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float fx = (float)x, fy = (float)y;
            float d = fabsf(fx - cx) / (60 * u + fy * 0.55f);
            float cone = powf(clampf(1 - d, 0, 1), 1.6f) * clampf(1 - fy / h * 0.4f, 0, 1);
            float ex = (fx - cx) / (w * 0.42f), ey = (fy - h * 0.80f) / (h * 0.16f);
            float pool = clampf(1 - (ex * ex + ey * ey), 0, 1);
            float light = cone * 0.35f + pool * 0.55f;
            float ax = fx / w * 1.1f, ay = (fy - h * 0.55f) / h * 1.6f;
            float amb = clampf(1 - (ax * ax + ay * ay), 0, 1);
            float vx = (fx - w / 2.0f) / (w * 0.75f), vy = (fy - h / 2.0f) / (h * 0.75f);
            float vig = powf(clampf(1 - (vx * vx + vy * vy), 0, 1), 0.6f);
            float r = 10 + light * 120 + amb * 70, g = 8 + light * 104 + amb * 8,
                  b = 10 + light * 80 + amb * 8;
            r *= vig; g *= vig; b *= vig;

            /* the ropes: a soft glow, the rope, a highlight along its top */
            for (int i = 0; i < 3; i++) {
                float d1 = seg_dist(fx, fy, rope[i][0], rope[i][1], rope[i][2], rope[i][3]);
                float d2 = seg_dist(fx, fy, rope[i][2], rope[i][3], rope[i][4], rope[i][5]);
                float dd = d1 < d2 ? d1 : d2, core = 3.5f * u;
                float a = dd < core ? 1 : dd < core + 1 ? core + 1 - dd : 0;
                float glow = dd < 12 * u ? 0.10f * (1 - dd / (12 * u)) : 0;
                a = a > glow ? a : glow;
                r += (ROPE[i][0] - r) * a; g += (ROPE[i][1] - g) * a; b += (ROPE[i][2] - b) * a;
            }

            /* the post, lit from the left, with the pads and a gold cap */
            if (fx >= px0 && fx < px1 && fy >= h * 0.47f && fy < h * 0.93f) {
                float t = (fx - px0) / (px1 - px0);
                float sh = 0.45f + 0.75f * powf(sinf(t * 3.14159f), 0.8f) * (1 - 0.5f * t);
                r = 120 * sh; g = 14 * sh; b = 14 * sh;
            }
            for (int i = 0; i < 3; i++) {
                float yp = rope[i][3];
                if (fx >= px0 - 4 * u && fx < px1 + 4 * u && fy >= yp - 14 * u && fy < yp + 16 * u) {
                    float t = (fx - px0) / (px1 - px0), sh = 0.8f + 0.3f * sinf(clampf(t, 0, 1) * 3.14159f);
                    r = ROPE[i][0] * sh; g = ROPE[i][1] * sh; b = ROPE[i][2] * sh;
                }
            }
            if (fx >= px0 - 2 * u && fx < px1 + 2 * u && fy >= h * 0.455f && fy < h * 0.48f) {
                r = 246; g = 214; b = 112;
            }

            /* the menu side fades to near black */
            if (fx < w * 0.48f) {
                float a = 0.96f * powf(1 - fx / (w * 0.48f), 0.9f);
                r += (14 - r) * a; g += (6 - g) * a; b += (8 - b) * a;
            }
            {
                uint32_t hsh = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
                float n = (float)((hsh ^ (hsh >> 13)) * 1274126177u >> 24) / 255.0f - 0.5f;
                r += n * 8; g += n * 8; b += n * 8;
            }
            s_scene[y * w + x] = 0xFF000000u | (uint32_t)clampf(r, 0, 255) << 16 |
                                 (uint32_t)clampf(g, 0, 255) << 8 | (uint32_t)clampf(b, 0, 255);
        }
    }
}

static void fill_blend(int x0, int y0, int x1, int y1, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s_cv.w) x1 = s_cv.w;
    if (y1 > s_cv.h) y1 = s_cv.h;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            s_cv.bits[y * s_cv.w + x] = blend(s_cv.bits[y * s_cv.w + x], r, g, b, a);
}

/* The chosen row's bar: red tape with a gold edge, its right end cut at a
 * slant. */
static void tape_bar(int x0, int y0, int x1, int y1, float u)
{
    int edge = (int)(10 * u), slant = (int)(20 * u);
    if (y0 < 0) y0 = 0;
    if (y1 > s_cv.h) y1 = s_cv.h;
    for (int y = y0; y < y1; y++) {
        int xe = x1 - slant * (y - y0) / (y1 - y0 > 0 ? y1 - y0 : 1);
        float v = 1.0f - 0.25f * (float)(y - y0) / (float)(y1 - y0);
        for (int x = x0 < 0 ? 0 : x0; x < xe && x < s_cv.w; x++) {
            uint32_t *p = &s_cv.bits[y * s_cv.w + x];
            *p = x < x0 + edge ? 0xFF000000u | 246u << 16 | 214u << 8 | 112u
                               : blend(*p, (uint32_t)(175 * v), (uint32_t)(24 * v), (uint32_t)(22 * v), 236);
        }
    }
}

/* A round button chip, anti-aliased; label: "A", "B", or "UD"/"LR" for
 * D-pad arrows. */
static void chip(int cx, int cy, int rad, uint32_t r, uint32_t g, uint32_t b, const char *label, float u)
{
    for (int y = cy - rad - 1; y <= cy + rad + 1; y++)
        for (int x = cx - rad - 1; x <= cx + rad + 1; x++) {
            float d = sqrtf((float)((x - cx) * (x - cx) + (y - cy) * (y - cy)));
            float a = clampf(rad + 0.5f - d, 0, 1);
            if (a > 0 && x >= 0 && y >= 0 && x < s_cv.w && y < s_cv.h)
                s_cv.bits[y * s_cv.w + x] = blend(s_cv.bits[y * s_cv.w + x], r, g, b, (uint32_t)(a * 255));
        }
    if (!strcmp(label, "UD") || !strcmp(label, "LR")) {
        int k = rad / 2, gap = rad / 6 + 1;
        for (int i = 0; i <= k; i++) {   /* two small triangles */
            if (label[0] == 'U') {
                fill_blend(cx - i, cy - gap - k + i, cx + i + 1, cy - gap - k + i + 1, 255, 255, 255, 255);
                fill_blend(cx - i, cy + gap + k - i, cx + i + 1, cy + gap + k - i + 1, 255, 255, 255, 255);
            } else {
                fill_blend(cx - gap - k + i, cy - i, cx - gap - k + i + 1, cy + i + 1, 255, 255, 255, 255);
                fill_blend(cx + gap + k - i, cy - i, cx + gap + k - i + 1, cy + i + 1, 255, 255, 255, 255);
            }
        }
    } else {
        GdiFlush();
        text(label, cx - rad, cy - rad, rad * 2, rad * 2, DT_CENTER, RGB(255, 255, 255), s_lf.hint, 0);
    }
    (void)u;
}

/* The key help along the bottom, from the menus' footer text: parts split by
 * runs of spaces, "Keys: what" each; Enter/A, Esc/B, Up/Down and Left/Right
 * become button chips. */
static void hints(const char *footer, int y, float u)
{
    char buf[256], *part, *next;
    int x = (int)(84 * u), rad = (int)(13 * u);
    snprintf(buf, sizeof buf, "%s", footer);
    for (part = buf; part && *part; part = next) {
        char *colon, *what;
        SIZE sz;
        next = strstr(part, "  ");
        if (next) {
            *next = 0;
            while (*++next == ' ') {}
        }
        colon = strchr(part, ':');
        what = colon ? colon + 1 : part;
        while (*what == ' ') what++;
        if (colon) {
            *colon = 0;
            if (strstr(part, "Up/Down")) chip(x + rad, y, rad, 90, 90, 90, "UD", u);
            else if (strstr(part, "Left/Right")) chip(x + rad, y, rad, 90, 90, 90, "LR", u);
            else if (strstr(part, "B")) chip(x + rad, y, rad, 200, 50, 40, "B", u);
            else if (strstr(part, "A")) chip(x + rad, y, rad, 60, 170, 70, "A", u);
            else {
                GdiFlush();
                SelectObject(s_cv.dc, s_lf.hint);
                GetTextExtentPoint32A(s_cv.dc, part, (int)strlen(part), &sz);
                text(part, x, y - rad, sz.cx, rad * 2, DT_LEFT, RGB(246, 214, 112), s_lf.hint, 0);
                x += sz.cx - rad * 2;
            }
            x += rad * 2 + (int)(8 * u);
        }
        {
            char up[128];
            int i;
            for (i = 0; what[i] && i < 127; i++)
                up[i] = (char)(what[i] >= 'a' && what[i] <= 'z' ? what[i] - 32 : what[i]);
            up[i] = 0;
            GdiFlush();
            SelectObject(s_cv.dc, s_lf.hint);
            GetTextExtentPoint32A(s_cv.dc, up, i, &sz);
            text(up, x, y - rad, sz.cx + 2, rad * 2, DT_LEFT, RGB(200, 192, 184), s_lf.hint, 0);
            x += sz.cx + (int)(30 * u);
        }
    }
}

/* The info panel at the top right (rky_ui_panel), shown with the next
 * launcher menu only. */
static const char *const *s_panel_k, *const *s_panel_v;
static int s_panel_n;

void rky_ui_panel(const char *const *keys, const char *const *vals, int n)
{
    s_panel_k = keys;
    s_panel_v = vals;
    s_panel_n = n;
}

static void panel(int w, float u)
{
    int pw = (int)(330 * u), x0 = w - pw - (int)(70 * u), y0 = (int)(96 * u);
    int ph = (int)((22 + 34 * s_panel_n) * u);
    GdiFlush();
    fill_blend(x0, y0, x0 + pw, y0 + ph, 8, 6, 8, 170);
    fill_blend(x0, y0, x0 + pw, y0 + 1, 246, 214, 112, 120);
    fill_blend(x0, y0 + ph - 1, x0 + pw, y0 + ph, 246, 214, 112, 120);
    fill_blend(x0, y0, x0 + 1, y0 + ph, 246, 214, 112, 120);
    fill_blend(x0 + pw - 1, y0, x0 + pw, y0 + ph, 246, 214, 112, 120);
    for (int i = 0; i < s_panel_n; i++) {
        int y = y0 + (int)((14 + 34 * i) * u);
        GdiFlush();
        if (i)
            fill_blend(x0 + (int)(18 * u), y - (int)(4 * u), x0 + pw - (int)(18 * u), y - (int)(3 * u),
                       255, 255, 255, 30);
        text(s_panel_k[i], x0 + (int)(18 * u), y, pw / 2, (int)(28 * u), DT_LEFT,
             RGB(170, 160, 150), s_lf.info, 0);
        text(s_panel_v[i], x0 + pw / 3, y, pw - pw / 3 - (int)(18 * u), (int)(28 * u), DT_RIGHT,
             RGB(255, 244, 214), s_lf.infob, 0);
    }
}

static void launcher_menu(uint32_t *out, int w, int h, const char *title,
                          const RkyUiRow *rows, int n, int sel, const char *note,
                          const char *footer)
{
    static float bar_y = -1;
    static const char *last_title;
    float u = (float)h / 720.0f;
    int sh = (int)(3 * u) + 1, step = (int)(48 * u), top, first = 0, vis, values = 0;
    int col0 = (int)(64 * u), text_x = (int)(92 * u), col1, foot_y = h - (int)(60 * u);
    int has_logo = !strcmp(title, "ROCKY");

    canvas_size(w, h, h);
    launcher_fonts(h);
    if (s_scene_w != w || s_scene_h != h)
        scene_draw(w, h);
    GdiFlush();
    if (s_scene)
        memcpy(s_cv.bits, s_scene, (size_t)w * h * 4);
    for (int i = 0; i < n && !has_logo; i++)   /* the main page's one value fits its bar */
        values |= rows[i].value != NULL;
    col1 = values ? (int)(w * 0.56f) : (int)(470 * u);
    if (values)   /* a darker column behind label and value rows */
        for (int x = 0; x < (int)(w * 0.64f); x++) {
            float t = (float)x / (w * 0.64f);
            fill_blend(x, 0, x + 1, h, 6, 3, 4, (uint32_t)(150 * (t < 0.8f ? 1 : (1 - t) / 0.2f)));
        }

    /* the title: the logo on the main page, else a small logo line and the page's name */
    if (has_logo) {
        text("ROCKY", (int)(80 * u), (int)(52 * u), w / 2, (int)(150 * u), DT_LEFT,
             RGB(246, 214, 112), s_lf.logo, sh + 1);
        SetTextCharacterExtra(s_cv.dc, (int)(7 * u));
        text("RECOMPILED", (int)(86 * u), (int)(186 * u), w / 2, (int)(32 * u), DT_LEFT,
             RGB(225, 60, 50), s_lf.infob, 0);
        SetTextCharacterExtra(s_cv.dc, 0);
        top = (int)(262 * u);
    } else {
        SetTextCharacterExtra(s_cv.dc, (int)(5 * u));
        text("ROCKY RECOMPILED", (int)(86 * u), (int)(44 * u), w / 2, (int)(30 * u), DT_LEFT,
             RGB(225, 60, 50), s_lf.infob, 0);
        SetTextCharacterExtra(s_cv.dc, 0);
        text(title, (int)(80 * u), (int)(70 * u), w / 2, (int)(100 * u), DT_LEFT,
             RGB(246, 214, 112), s_lf.title, sh);
        top = (int)(190 * u);
    }

    /* fit the rows above the note and the footer: closer together if need
     * be, and past that scroll to keep sel in view */
    {
        int avail = foot_y - (int)(16 * u) - top - (note && *note ? (int)(44 * u) : 0), ngap = 0;
        for (int i = 1; i < n; i++)
            ngap += rows[i].gap_before;
        if (n > 0 && step * n + ngap * step / 3 > avail) {
            step = avail * 3 / (3 * n + ngap);
            if (step < (int)(34 * u)) step = (int)(34 * u);
        }
        vis = (avail - ngap * step / 3) / step;
        if (vis < 1) vis = 1;
    }
    if (n > vis) {
        first = sel - vis / 2;
        if (first > n - vis) first = n - vis;
        if (first < 0) first = 0;
    }
    s_rows_n = n < 32 ? n : 32;
    s_row_h = step;
    s_row_x0 = col0;
    s_row_x1 = col1 + (int)(30 * u);
    {
        int ry[128], gaps = 0;
        float target = 0;
        if (n > 128) n = 128;
        for (int i = 0; i < n; i++) {
            int on = i >= first && i < first + vis;
            if (on && rows[i].gap_before && i != first) gaps += step / 3;
            ry[i] = on ? top + (i - first) * step + gaps : -100000;
            if (i < 32) s_row_y[i] = ry[i];
            if (i == sel) target = (float)ry[i];
        }
        if (title != last_title || bar_y < 0 || fabsf(bar_y - target) > step * 4)
            bar_y = target;   /* a new page or a jump: no slide */
        else
            bar_y += (target - bar_y) * 0.35f;
        last_title = title;
        GdiFlush();
        tape_bar(col0, (int)bar_y + (int)(2 * u), col1 + (int)(30 * u), (int)bar_y + step - (int)(2 * u), u);
        for (int i = first; i < n && i < first + vis; i++) {
            int on_bar = i == sel, lw = rows[i].value ? (col1 - text_x) * 6 / 10 : col1 - text_x;
            COLORREF c = rows[i].dim ? RGB(110, 104, 100) : on_bar ? RGB(255, 244, 214) : RGB(205, 196, 186);
            text(rows[i].label, text_x, ry[i], lw, step, DT_LEFT, c, on_bar ? s_lf.sel : s_lf.row,
                 on_bar ? sh : 0);
            if (rows[i].value)
                text(rows[i].value, text_x + (col1 - text_x) * 4 / 10, ry[i], (col1 - text_x) * 6 / 10, step,
                     DT_RIGHT, rows[i].dim ? c : on_bar ? RGB(255, 236, 160) : RGB(225, 216, 206),
                     on_bar ? s_lf.sel : s_lf.row, on_bar ? sh : 0);
        }
        if (n > vis) {   /* more above or below */
            if (first > 0)
                text("...", text_x, top - step / 2, step, step / 2, DT_LEFT, RGB(170, 160, 150), s_lf.info, 0);
            if (first + vis < n)
                text("...", text_x, top + vis * step + gaps, step, step / 2, DT_LEFT, RGB(170, 160, 150), s_lf.info, 0);
        }
        if (note && *note) {
            int ny = top + (n < vis ? n : vis) * step + gaps + (int)(18 * u);
            if (ny > foot_y - (int)(52 * u)) ny = foot_y - (int)(52 * u);
            text(note, text_x, ny, w - text_x - (int)(60 * u), (int)(30 * u), DT_LEFT,
                 RGB(246, 214, 112), s_lf.info, 0);
        }
    }

    if (s_panel_n > 0)
        panel(w, u);
    s_panel_n = 0;

    GdiFlush();
    fill_blend(0, foot_y, w, h, 0, 0, 0, 150);
    if (footer && *footer)
        hints(footer, foot_y + (h - foot_y) / 2, u);
    text("ROCKY RECOMPILED " RKY_VERSION "  \xB7  UNOFFICIAL", w / 2, foot_y, w / 2 - (int)(60 * u), h - foot_y,
         DT_RIGHT, RGB(130, 122, 116), s_lf.info, 0);
    GdiFlush();
    memcpy(out, s_cv.bits, (size_t)w * h * 4);
}

void rky_ui_menu(uint32_t *out, int w, int h, const uint32_t *backdrop,
                 int x0, int y0, int pw, int ph, const char *title,
                 const RkyUiRow *rows, int n, int sel, const char *note,
                 const char *footer)
{
    int rowh, top, sh, extra = 0;

    if (!backdrop) {
        launcher_menu(out, w, h, title, rows, n, sel, note, footer);
        return;
    }
    canvas_size(w, h, ph);
    GdiFlush();
    for (int i = 0; i < w * h; i++) {
        uint32_t c = backdrop[i];
        s_cv.bits[i] = 0xFF000000u | ((c >> 16 & 0xFF) * 2 / 7) << 16 |
                       ((c >> 8 & 0xFF) * 2 / 7) << 8 | (c & 0xFF) * 2 / 7;
    }
    sh = ph / 300 + 1;
    text(title, x0, y0 + ph / 14, pw, ph / 7, DT_CENTER, RGB(244, 212, 110), s_cv.title, sh * 2);
    rowh = ph / 15;
    top = y0 + ph * 22 / 100;
    s_rows_n = n < 32 ? n : 32;
    s_row_h = rowh;
    s_row_x0 = x0 + pw / 16;
    s_row_x1 = x0 + pw - pw / 16;
    for (int i = 0; i < n; i++) {
        int y;
        if (rows[i].gap_before)
            extra += rowh / 3;
        y = top + i * rowh + extra;
        if (i < 32)
            s_row_y[i] = y;
        if (i == sel) {
            GdiFlush();
            gold_bar(s_row_x0, y + rowh / 10, s_row_x1, y + rowh - rowh / 10);
        }
        COLORREF c = rows[i].dim ? RGB(130, 130, 130) : RGB(255, 255, 255);
        if (!rows[i].value) {
            text(rows[i].label, x0, y, pw, rowh, DT_CENTER, c, s_cv.row, sh);
        } else {
            text(rows[i].label, x0 + pw / 8, y, pw * 7 / 16, rowh, DT_LEFT, c, s_cv.row, sh);
            text(rows[i].value, x0 + pw * 9 / 16, y, pw * 5 / 16, rowh, DT_RIGHT,
                 rows[i].dim ? c : i == sel ? RGB(255, 236, 160) : RGB(225, 225, 225),
                 s_cv.row, sh);
        }
    }
    if (note && *note) {
        int ny = top + n * rowh + extra + rowh / 3;
        if (ny > y0 + ph - ph / 9 - rowh)   /* keep clear of the footer */
            ny = y0 + ph - ph / 9 - rowh;
        text(note, x0, ny, pw, rowh, DT_CENTER, RGB(244, 212, 110), s_cv.small, sh);
    }
    if (footer && *footer)
        text(footer, x0, y0 + ph - ph / 9, pw, ph / 14, DT_CENTER, RGB(200, 200, 200),
             s_cv.small, sh);
    GdiFlush();
    memcpy(out, s_cv.bits, (size_t)w * h * 4);
}

int rky_ui_row_at(int x, int y)
{
    if (x < s_row_x0 || x >= s_row_x1)
        return -1;
    for (int i = 0; i < s_rows_n; i++)
        if (y >= s_row_y[i] && y < s_row_y[i] + s_row_h)
            return i;
    return -1;
}
