/**
 * The launcher: a window before the game, in the look of Rocky's menus.
 *
 *   PLAY             start the game (needs the game files)
 *   PC OPTIONS       the same page as F1 in game; saved, applied at start
 *   MODS             the mods found, each switched on or off
 *   GAME FILES       where the game is installed; install from a disc image
 *   UPDATES          check the GitLab releases; download and install
 *   OPEN SAVES FOLDER
 *   QUIT
 *
 * Drawn in the launcher's own look (src/ui.c), with an info panel on the
 * main page. Keys (arrows, Enter, Esc), the mouse, or a
 * controller through XInput (the game's SDL input is not up yet). --play, or settings.ini skip_launcher =
 * on, goes straight to the game.
 */
#include <windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "install.h"
#include "launcher.h"
#include "mods.h"
#include "settings.h"
#include "ui.h"
#include "update.h"

enum { PAGE_MAIN, PAGE_OPTIONS, PAGE_MODS, PAGE_GAME, PAGE_UPDATE };
enum { M_PLAY, M_OPTIONS, M_MODS, M_GAME, M_UPDATE, M_SAVES, M_QUIT, M_COUNT };

typedef struct {
    HWND wnd;
    int page, sel, result;
    RkySettings *set;
    const char *exe_dir, *save_dir;
    char *game_dir;
    size_t game_n;
    int have_game;
    uint32_t *px;
    int pw, ph;
    char note[256];
} Launcher;

static Launcher L;

/* ── Controller (XInput) ───────────────────────────────────── */

typedef struct { WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; } XPad;
typedef struct { DWORD packet; XPad pad; } XState;
typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XState *);
static XInputGetState_t s_xget;
static WORD s_prev_buttons;

static int poll_pad(void)
{
    XState st;
    WORD now = 0, pressed;
    if (!s_xget) {
        HMODULE m = LoadLibraryA("xinput1_4.dll");
        if (!m) m = LoadLibraryA("xinput9_1_0.dll");
        s_xget = m ? (XInputGetState_t)(void *)GetProcAddress(m, "XInputGetState") : NULL;
        if (!s_xget) s_xget = (XInputGetState_t)(void *)1;
    }
    if (s_xget == (XInputGetState_t)(void *)1)
        return 0;
    for (DWORD i = 0; i < 4; i++)
        if (s_xget(i, &st) == 0) {
            now |= st.pad.buttons;
            if (st.pad.ly > 16000) now |= 0x0001;
            if (st.pad.ly < -16000) now |= 0x0002;
            if (st.pad.lx < -16000) now |= 0x0004;
            if (st.pad.lx > 16000) now |= 0x0008;
        }
    pressed = now & ~s_prev_buttons;
    s_prev_buttons = now;
    if (pressed & 0x0001) return RKY_IN_UP;
    if (pressed & 0x0002) return RKY_IN_DOWN;
    if (pressed & 0x0004) return RKY_IN_LEFT;
    if (pressed & 0x0008) return RKY_IN_RIGHT;
    if (pressed & (0x1000 | 0x0010)) return RKY_IN_OK;      /* A, Start */
    if (pressed & (0x2000 | 0x0020)) return RKY_IN_CLOSE;   /* B, Back */
    return 0;
}

/* ── Pages ─────────────────────────────────────────────────── */

static void open_folder(const char *path)
{
    CreateDirectoryA(path, NULL);
    ShellExecuteA(L.wnd, "open", path, NULL, NULL, SW_SHOWNORMAL);
}

static int mods_rows(void)
{
    const RkyModInfo *m;
    return rky_mods_list(&m) + 2;   /* + Open Mods Folder, Back */
}

static void draw(void)
{
    RECT rc;
    int w, h, x0, y0, pw, ph;
    if (L.wnd) {
        GetClientRect(L.wnd, &rc);
    } else {
        rc.right = 1280;    /* RKY_LAUNCHER_SHOT: drawn without a window */
        rc.bottom = 720;
    }
    w = rc.right > 64 ? rc.right : 64;
    h = rc.bottom > 48 ? rc.bottom : 48;
    if (w != L.pw || h != L.ph) {
        free(L.px);
        L.px = malloc((size_t)w * h * 4);
        L.pw = w;
        L.ph = h;
    }
    ph = h;
    pw = ph * 4 / 3 < w ? ph * 4 / 3 : w;
    x0 = (w - pw) / 2;
    y0 = 0;

    if (L.page == PAGE_OPTIONS) {
        rky_options_page_draw(L.px, w, h, NULL, x0, y0, pw, ph);
    } else if (L.page == PAGE_MAIN) {
        static char upd[64], info[4][32];
        static const char *const keys[4] = { "VERSION", "GAME FILES", "MODS", "UPDATES" };
        const char *vals[4] = { info[0], info[1], info[2], info[3] };
        const RkyModInfo *m;
        RkyUiRow r[M_COUNT] = {
            { "Play", NULL, 0, !L.have_game }, { "PC Options", NULL }, { "Mods", NULL },
            { "Game Files", NULL }, { "Updates", upd }, { "Open Saves Folder", NULL },
            { "Quit", NULL, 1 } };
        int st = rky_update_state(), nm = rky_mods_list(&m), on = 0;
        for (int i = 0; i < nm; i++)
            on += m[i].enabled;
        snprintf(upd, sizeof upd, "%s", st == RKY_UPD_AVAILABLE ? "New version!" :
                 st == RKY_UPD_CHECKING ? "Checking..." : st == RKY_UPD_READY ? "Restart" : "");
        r[M_UPDATE].value = upd[0] ? upd : NULL;
        snprintf(info[0], sizeof info[0], "%s", RKY_VERSION);
        snprintf(info[1], sizeof info[1], "%s", L.have_game ? "READY" : "NOT INSTALLED");
        if (on) snprintf(info[2], sizeof info[2], "%d ON", on);
        else snprintf(info[2], sizeof info[2], "%s", nm ? "ALL OFF" : "NONE");
        snprintf(info[3], sizeof info[3], "%s",
                 st == RKY_UPD_CHECKING ? "CHECKING..." : st == RKY_UPD_CURRENT ? "UP TO DATE" :
                 st == RKY_UPD_AVAILABLE ? "NEW VERSION" : st == RKY_UPD_DOWNLOADING ? "DOWNLOADING" :
                 st == RKY_UPD_READY ? "RESTART TO USE" : st == RKY_UPD_FAILED ? "COULD NOT CHECK" : "NOT CHECKED");
        rky_ui_panel(keys, vals, 4);
        rky_ui_menu(L.px, w, h, NULL, x0, y0, pw, ph, "ROCKY", r, M_COUNT, L.sel,
                    L.note[0] ? L.note :
                    !L.have_game ? "The game files are not installed yet: choose Game Files." :
                    st == RKY_UPD_AVAILABLE ? "A new version is available under Updates." : NULL,
                    "Up/Down: choose    Enter / A: select    Esc / B: quit");
    } else if (L.page == PAGE_MODS) {
        const RkyModInfo *m;
        int n = rky_mods_list(&m);
        RkyUiRow r[RKY_MAX_MODS + 2];
        static char vals[RKY_MAX_MODS][32];
        for (int i = 0; i < n; i++) {
            if (m[i].enabled) snprintf(vals[i], sizeof vals[i], "On (%d files)", m[i].files);
            else snprintf(vals[i], sizeof vals[i], "Off");
            r[i] = (RkyUiRow){ m[i].name, vals[i], 0, !m[i].enabled };
        }
        r[n] = (RkyUiRow){ "Open Mods Folder", NULL, 1, 0 };
        r[n + 1] = (RkyUiRow){ "Back", NULL, 0, 0 };
        rky_ui_menu(L.px, w, h, NULL, x0, y0, pw, ph, "MODS", r, n + 2, L.sel,
                    L.note[0] ? L.note : n ? "Changes apply when the game starts."
                                           : "No mods yet: put them in the mods folder.",
                    "Enter / A: switch on or off    Esc / B: back");
    } else if (L.page == PAGE_GAME) {
        RkyUiRow r[4] = {
            { "Installed Copy", L.have_game ? "Ready" : "Not found", 0, 0 },
            { "Install From Disc Image...", NULL, 1, 0 },
            { "Open Game Folder", NULL, 0, !L.have_game },
            { "Back", NULL, 0, 0 } };
        rky_ui_menu(L.px, w, h, NULL, x0, y0, pw, ph, "GAME FILES", r, 4, L.sel,
                    L.note[0] ? L.note : L.have_game ? L.game_dir
                    : "Needs Rocky for the original Xbox (USA): .iso, .zip, .7z or default.xbe.",
                    "Enter / A: select    Esc / B: back");
    } else {
        static char status[200], latest[64];
        int st = rky_update_state();
        RkyUiRow r[3] = {
            { "Check For Updates", NULL, 0, st == RKY_UPD_CHECKING || st == RKY_UPD_DOWNLOADING },
            { latest, NULL, 0, st != RKY_UPD_AVAILABLE && st != RKY_UPD_READY },
            { "Back", NULL, 1, 0 } };
        if (st == RKY_UPD_READY) snprintf(latest, sizeof latest, "Restart Now");
        else if (st == RKY_UPD_AVAILABLE) snprintf(latest, sizeof latest, "Update To %s", rky_update_latest());
        else snprintf(latest, sizeof latest, "Update");
        switch (st) {
        case RKY_UPD_IDLE: snprintf(status, sizeof status, "This is version %s.", RKY_VERSION); break;
        case RKY_UPD_CHECKING: snprintf(status, sizeof status, "Checking GitLab for releases..."); break;
        case RKY_UPD_CURRENT: snprintf(status, sizeof status, "Version %s is the newest.", RKY_VERSION); break;
        case RKY_UPD_AVAILABLE: snprintf(status, sizeof status, "Version %s is available: %s", rky_update_latest(), rky_update_notes()); break;
        case RKY_UPD_DOWNLOADING: snprintf(status, sizeof status, "Downloading %s... %d%%", rky_update_latest(), rky_update_progress()); break;
        case RKY_UPD_READY: snprintf(status, sizeof status, "Version %s is installed. Restart to use it.", rky_update_latest()); break;
        default: snprintf(status, sizeof status, "%s", rky_update_error()); break;
        }
        rky_ui_menu(L.px, w, h, NULL, x0, y0, pw, ph, "UPDATES", r, 3, L.sel, status,
                    "Saves, settings and mods are kept.    Esc / B: back");
    }
    if (L.wnd) {
        HDC dc = GetDC(L.wnd);
        BITMAPINFO bi = { 0 };
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        SetDIBitsToDevice(dc, 0, 0, (DWORD)w, (DWORD)h, 0, 0, 0, (UINT)h, L.px, &bi, DIB_RGB_COLORS);
        ReleaseDC(L.wnd, dc);
    }
}

static int page_rows(void)
{
    switch (L.page) {
    case PAGE_MAIN: return M_COUNT;
    case PAGE_MODS: return mods_rows();
    case PAGE_GAME: return 4;
    case PAGE_UPDATE: return 3;
    }
    return 0;
}

static void go(int page)
{
    L.page = page;
    L.sel = 0;
    L.note[0] = 0;
    if (page == PAGE_OPTIONS)
        rky_options_page_open(0);
    if (page == PAGE_UPDATE && rky_update_state() == RKY_UPD_IDLE)
        rky_update_check();
}

static void activate(void)
{
    L.note[0] = 0;
    if (L.page == PAGE_MAIN) {
        switch (L.sel) {
        case M_PLAY:
            if (L.have_game) { L.result = 1; DestroyWindow(L.wnd); }
            else go(PAGE_GAME);
            break;
        case M_OPTIONS: go(PAGE_OPTIONS); break;
        case M_MODS: go(PAGE_MODS); break;
        case M_GAME: go(PAGE_GAME); break;
        case M_UPDATE: go(PAGE_UPDATE); break;
        case M_SAVES: open_folder(L.save_dir); break;
        case M_QUIT: L.result = 0; DestroyWindow(L.wnd); break;
        }
    } else if (L.page == PAGE_MODS) {
        const RkyModInfo *m;
        int n = rky_mods_list(&m);
        if (L.sel < n) {
            if (!rky_mod_set_enabled(L.sel, !m[L.sel].enabled))
                snprintf(L.note, sizeof L.note, "Could not change %s.", m[L.sel].name);
        } else if (L.sel == n) {
            char p[MAX_PATH * 2];
            snprintf(p, sizeof p, "%smods", L.exe_dir);
            open_folder(p);
        } else {
            go(PAGE_MAIN);
        }
    } else if (L.page == PAGE_GAME) {
        if (L.sel == 1) {
            if (rky_install_dialog(L.wnd, L.save_dir, L.game_dir, L.game_n)) {
                L.have_game = 1;
                snprintf(L.note, sizeof L.note, "Installed. Choose Back, then Play.");
            }
        } else if (L.sel == 2 && L.have_game) {
            open_folder(L.game_dir);
        } else if (L.sel == 3) {
            go(PAGE_MAIN);
        }
    } else if (L.page == PAGE_UPDATE) {
        int st = rky_update_state();
        if (L.sel == 0 && st != RKY_UPD_CHECKING && st != RKY_UPD_DOWNLOADING)
            rky_update_check();
        else if (L.sel == 1 && st == RKY_UPD_AVAILABLE)
            rky_update_install(L.exe_dir);
        else if (L.sel == 1 && st == RKY_UPD_READY) {
            if (rky_update_restart(L.exe_dir)) { L.result = 0; DestroyWindow(L.wnd); }
        } else if (L.sel == 2)
            go(PAGE_MAIN);
    }
}

static void input(int in)
{
    if (!in)
        return;
    if (L.page == PAGE_OPTIONS) {
        if (rky_options_page_input(in))
            go(PAGE_MAIN), L.sel = M_OPTIONS;
        return;
    }
    switch (in) {
    case RKY_IN_UP: L.sel = (L.sel + page_rows() - 1) % page_rows(); break;
    case RKY_IN_DOWN: L.sel = (L.sel + 1) % page_rows(); break;
    case RKY_IN_OK: activate(); break;
    case RKY_IN_CLOSE:
        if (L.page == PAGE_MAIN) { L.result = 0; DestroyWindow(L.wnd); }
        else { int from = L.page; go(PAGE_MAIN); L.sel = from == PAGE_MODS ? M_MODS : from == PAGE_GAME ? M_GAME : M_UPDATE; }
        break;
    case RKY_IN_LEFT: case RKY_IN_RIGHT:
        if (L.page == PAGE_MODS) activate();
        break;
    }
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_KEYDOWN:
        switch (w) {
        case VK_UP: case 'W': input(RKY_IN_UP); break;
        case VK_DOWN: case 'S': input(RKY_IN_DOWN); break;
        case VK_LEFT: case 'A': input(RKY_IN_LEFT); break;
        case VK_RIGHT: case 'D': input(RKY_IN_RIGHT); break;
        case VK_RETURN: case VK_SPACE: case 'K': input(RKY_IN_OK); break;
        case VK_ESCAPE: case VK_BACK: case 'L': input(RKY_IN_CLOSE); break;
        }
        return 0;
    case WM_MOUSEMOVE: {
        int r = rky_ui_row_at((short)LOWORD(l), (short)HIWORD(l));
        if (r >= 0 && L.page != PAGE_OPTIONS) L.sel = r;
        return 0;
    }
    case WM_LBUTTONUP: {
        int r = rky_ui_row_at((short)LOWORD(l), (short)HIWORD(l));
        if (r < 0) return 0;
        if (L.page == PAGE_OPTIONS) {
            if (rky_options_page_click(r)) go(PAGE_MAIN), L.sel = M_OPTIONS;
        } else {
            L.sel = r;
            activate();
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        draw();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        L.result = 0;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

/* RKY_LAUNCHER_SHOT=<prefix>: draw every page to <prefix>_<page>.bmp and
 * quit, without a window (checking the layout unattended). */
static void write_bmp(const char *path)
{
    BITMAPFILEHEADER fh = { 0 };
    BITMAPINFOHEADER ih = { 0 };
    FILE *f = fopen(path, "wb");
    if (!f) return;
    ih.biSize = sizeof ih;
    ih.biWidth = L.pw;
    ih.biHeight = -L.ph;
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof fh + sizeof ih;
    fh.bfSize = fh.bfOffBits + (DWORD)(L.pw * L.ph * 4);
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&ih, sizeof ih, 1, f);
    fwrite(L.px, 4, (size_t)L.pw * L.ph, f);
    fclose(f);
}

static void shots(const char *prefix)
{
    static const char *const names[] = { "main", "options", "mods", "game", "update" };
    char p[MAX_PATH * 2];
    for (int page = 0; page < 5; page++) {
        go(page);
        if (page == PAGE_UPDATE) {
            for (int i = 0; i < 100 && rky_update_state() == RKY_UPD_CHECKING; i++)
                Sleep(100);   /* the check's answer, for the picture */
        }
        draw();
        snprintf(p, sizeof p, "%s_%s.bmp", prefix, names[page]);
        write_bmp(p);
    }
}

int rky_launcher_run(RkySettings *s, const char *exe_dir, const char *save_dir,
                     char *game_dir, size_t game_n, int have_game)
{
    WNDCLASSA wc = { 0 };
    MSG msg;
    int sw = GetSystemMetrics(SM_CXSCREEN), shh = GetSystemMetrics(SM_CYSCREEN);
    int wh = shh * 2 / 3, ww = wh * 16 / 9;
    RECT r = { 0, 0, ww, wh };

    memset(&L, 0, sizeof L);
    L.set = s;
    L.exe_dir = exe_dir;
    L.save_dir = save_dir;
    L.game_dir = game_dir;
    L.game_n = game_n;
    L.have_game = have_game;
    rky_options_init(s);
    if (getenv("RKY_LAUNCHER_SHOT")) {
        shots(getenv("RKY_LAUNCHER_SHOT"));
        free(L.px);
        return 0;
    }

    SetProcessDPIAware();
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCEA(1));
    wc.lpszClassName = "RockyLauncher";
    RegisterClassA(&wc);
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    L.wnd = CreateWindowA("RockyLauncher", "Rocky Recompiled " RKY_VERSION, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          (sw - (r.right - r.left)) / 2, (shh - (r.bottom - r.top)) / 2,
                          r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    if (!L.wnd)
        return 1;   /* no launcher: play */
    if (s->update_check)
        rky_update_check();
    SetTimer(L.wnd, 1, 16, NULL);
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (msg.message == WM_TIMER && IsWindow(L.wnd)) {
            input(poll_pad());
            if (IsWindow(L.wnd))
                draw();
        }
    }
    free(L.px);
    L.px = NULL;
    L.pw = L.ph = 0;
    return L.result;
}
