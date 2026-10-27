/**
 * Menu drawing in the look of Rocky's own menus (see ui.h).
 *
 * Text is drawn with Windows' GDI into a DIB; the gold bar is the olive
 * gold of the game's selected menu item, brighter in the middle and fading
 * out at the ends. Drawing happens on one thread at a time (the launcher
 * before the game, the overlay on the renderer's thread during it).
 */
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

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

/* The launcher's background: deep red to black, like the title screen's. */
static void background(void)
{
    for (int y = 0; y < s_cv.h; y++) {
        for (int x = 0; x < s_cv.w; x++) {
            float dx = (float)(x - s_cv.w / 2) / (float)s_cv.w;
            float dy = (float)(y - s_cv.h / 3) / (float)s_cv.h;
            float glow = 1.0f - (dx * dx * 2.2f + dy * dy * 2.6f);
            if (glow < 0) glow = 0;
            uint32_t r = (uint32_t)(20 + 120 * glow), g = (uint32_t)(4 + 22 * glow * glow),
                     b = (uint32_t)(4 + 10 * glow * glow);
            s_cv.bits[y * s_cv.w + x] = 0xFF000000u | r << 16 | g << 8 | b;
        }
    }
}

void rky_ui_menu(uint32_t *out, int w, int h, const uint32_t *backdrop,
                 int x0, int y0, int pw, int ph, const char *title,
                 const RkyUiRow *rows, int n, int sel, const char *note,
                 const char *footer)
{
    int rowh, top, sh, extra = 0;

    canvas_size(w, h, ph);
    GdiFlush();
    if (backdrop) {
        for (int i = 0; i < w * h; i++) {
            uint32_t c = backdrop[i];
            s_cv.bits[i] = 0xFF000000u | ((c >> 16 & 0xFF) * 2 / 7) << 16 |
                           ((c >> 8 & 0xFF) * 2 / 7) << 8 | (c & 0xFF) * 2 / 7;
        }
    } else {
        background();
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
