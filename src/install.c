/**
 * Finding the game's files, and installing them from the player's disc.
 *
 * The program carries no game data. It runs from the disc's files, looked
 * for in this order: the directory on the command line or in RKY_GAME_DIR,
 * the one installed earlier (<save dir>/game), a folder chosen earlier
 * (<save dir>/game_path.txt), and "game" beside the executable. When none of
 * them holds Rocky, it asks for the disc: an Xbox ISO (a plain XDVDFS image
 * or a full Redump dump), the .zip/.7z it came in, or the default.xbe of an
 * extracted copy. An image is unpacked into <save dir>/game once.
 *
 * The code was recompiled from one default.xbe (USA retail), so the files
 * found are checked against it: another release or a modified disc would run
 * this code against data it was not built for.
 */
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "install.h"

/* The retail USA default.xbe (title 0x55530014) the code was recompiled from. */
#define RKY_XBE_SIZE 1232896u
#define RKY_XBE_FNV  0x6765b13ee98c26b1ull

#define SECTOR 2048u
static const char XDVDFS_MAGIC[20] = "MICROSOFT*XBOX*MEDIA";
/* Where the game partition starts: plain XISO, rebuilt-image layouts, and a
 * Redump full-disc dump (after the video partition). */
static const uint64_t BASES[] = { 0, 0xFD90, 0x30600, 0xFD90000, 0x18300000 };

static char s_err[1024];

static void fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_err, sizeof s_err, fmt, ap);
    va_end(ap);
    fprintf(stderr, "[INSTALL] %s\n", s_err);
}

const char *rky_install_error(void) { return s_err; }

/* ── Checking a game folder ────────────────────────────────── */

static int file_exists(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* 1: dir/default.xbe is the USA retail XBE. */
int rky_check_xbe(const char *dir)
{
    char path[MAX_PATH * 2];
    uint64_t h = 0xcbf29ce484222325ull;
    unsigned char buf[65536];
    size_t n, total = 0;
    FILE *f;

    snprintf(path, sizeof path, "%s/default.xbe", dir);
    f = fopen(path, "rb");
    if (!f) {
        fail("%s has no default.xbe.", dir);
        return 0;
    }
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        for (size_t i = 0; i < n; i++)
            h = (h ^ buf[i]) * 0x100000001b3ull;
        total += n;
    }
    fclose(f);
    if (total != RKY_XBE_SIZE || h != RKY_XBE_FNV) {
        fail("This is not the USA retail release of Rocky for the original Xbox "
             "(default.xbe is %zu bytes, fingerprint %016llx). Only that release "
             "is supported.", total, (unsigned long long)h);
        return 0;
    }
    return 1;
}

/* ── XDVDFS ────────────────────────────────────────────────── */

typedef struct {
    FILE *f;
    uint64_t base;
    uint64_t total, done;      /* bytes of file data, for progress */
    rky_progress_fn progress;
    void *ud;
} Xiso;

static int xiso_read(Xiso *x, uint64_t off, void *buf, size_t n)
{
    if (_fseeki64(x->f, (int64_t)(x->base + off), SEEK_SET) != 0)
        return 0;
    return fread(buf, 1, n, x->f) == n;
}

static int xiso_open(Xiso *x, const char *path, uint32_t *root_sector, uint32_t *root_size)
{
    char magic[20];
    memset(x, 0, sizeof *x);
    x->f = fopen(path, "rb");
    if (!x->f) {
        fail("Cannot open %s.", path);
        return 0;
    }
    for (size_t i = 0; i < sizeof BASES / sizeof BASES[0]; i++) {
        x->base = BASES[i];
        if (xiso_read(x, 32u * SECTOR, magic, sizeof magic) &&
            !memcmp(magic, XDVDFS_MAGIC, sizeof magic)) {
            uint32_t r[2];
            xiso_read(x, 32u * SECTOR + 20, r, sizeof r);
            *root_sector = r[0];
            *root_size = r[1];
            return 1;
        }
    }
    fclose(x->f);
    x->f = NULL;
    fail("%s is not an Xbox disc image.", path);
    return 0;
}

/* Walk one directory's entry tree. measure: only add up file sizes. */
static int xiso_dir(Xiso *x, uint32_t sector, uint32_t size, const char *out, int measure)
{
    unsigned char *dir;
    int ok = 1;
    /* The tree is walked with an explicit stack of entry offsets (in dwords). */
    uint16_t stack[4096];
    int sp = 0;

    if (!size)
        return 1;
    dir = malloc(size);
    if (!dir || !xiso_read(x, (uint64_t)sector * SECTOR, dir, size)) {
        free(dir);
        fail("The disc image is damaged (cannot read a directory).");
        return 0;
    }
    stack[sp++] = 0;
    while (ok && sp) {
        uint32_t off = (uint32_t)stack[--sp] * 4u;
        uint16_t left, right;
        uint32_t esec, esize;
        unsigned char attr, nlen;
        char name[256], path[MAX_PATH * 2];

        if (off + 14 > size)
            continue;
        memcpy(&left, dir + off, 2);
        memcpy(&right, dir + off + 2, 2);
        memcpy(&esec, dir + off + 4, 4);
        memcpy(&esize, dir + off + 8, 4);
        attr = dir[off + 12];
        nlen = dir[off + 13];
        if (left == 0xFFFF && right == 0xFFFF && esec == 0xFFFFFFFFu)
            continue;              /* padding */
        if (off + 14 + nlen > size)
            continue;
        memcpy(name, dir + off + 14, nlen);
        name[nlen] = 0;
        if (left && left != 0xFFFF && sp < 4096)
            stack[sp++] = left;
        if (right && right != 0xFFFF && sp < 4096)
            stack[sp++] = right;
        if (!nlen || strchr(name, '/') || strchr(name, '\\') || !strcmp(name, "..") ||
            !strcmp(name, "."))
            continue;
        snprintf(path, sizeof path, "%s/%s", out, name);

        if (attr & 0x10) {
            if (!measure)
                CreateDirectoryA(path, NULL);
            ok = xiso_dir(x, esec, esize, path, measure);
        } else if (measure) {
            x->total += esize;
        } else {
            static unsigned char buf[1 << 20];
            FILE *o = fopen(path, "wb");
            uint64_t pos = (uint64_t)esec * SECTOR;
            uint32_t left_bytes = esize;
            if (!o) {
                fail("Cannot write %s.", path);
                ok = 0;
                break;
            }
            while (left_bytes) {
                uint32_t n = left_bytes < sizeof buf ? left_bytes : (uint32_t)sizeof buf;
                if (!xiso_read(x, pos, buf, n) || fwrite(buf, 1, n, o) != n) {
                    fail("Copying %s failed (damaged image or disk full).", name);
                    ok = 0;
                    break;
                }
                pos += n;
                left_bytes -= n;
                x->done += n;
                if (x->progress)
                    x->progress(x->done, x->total, x->ud);
            }
            fclose(o);
        }
    }
    free(dir);
    return ok;
}

static int unpack_iso(const char *iso, const char *out, rky_progress_fn progress, void *ud)
{
    Xiso x;
    uint32_t rs, rz;
    int ok;

    if (!xiso_open(&x, iso, &rs, &rz))
        return 0;
    x.progress = progress;
    x.ud = ud;
    ok = xiso_dir(&x, rs, rz, out, 1);
    if (ok) {
        CreateDirectoryA(out, NULL);
        ok = xiso_dir(&x, rs, rz, out, 0);
    }
    fclose(x.f);
    return ok;
}

/* ── Archives (.zip, .7z) ──────────────────────────────────── */

/* Windows 10+ ships bsdtar (libarchive) as tar.exe, which reads zip and 7z.
 * The image inside is unpacked to a scratch folder, then read as an ISO. */
static int run_tar(const char *args)
{
    char cmd[4096], sys[MAX_PATH];
    STARTUPINFOA si = { sizeof si };
    PROCESS_INFORMATION pi;
    DWORD code = 1;

    GetSystemDirectoryA(sys, sizeof sys);
    snprintf(cmd, sizeof cmd, "\"%s\\tar.exe\" %s", sys, args);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        fail("Cannot run tar.exe to unpack the archive (Windows 10 or later is needed).");
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code == 0;
}

static int find_iso(const char *dir, char *out, size_t n)
{
    char pat[MAX_PATH * 2];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    int found = 0;

    snprintf(pat, sizeof pat, "%s/*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        char path[MAX_PATH * 2];
        const char *ext = strrchr(fd.cFileName, '.');
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            found = find_iso(path, out, n);
        else if (ext && (!_stricmp(ext, ".iso") || !_stricmp(ext, ".xiso"))) {
            snprintf(out, n, "%s", path);
            found = 1;
        }
    } while (!found && FindNextFileA(h, &fd));
    FindClose(h);
    return found;
}

static void remove_tree(const char *dir)
{
    char from[MAX_PATH * 2 + 2];
    SHFILEOPSTRUCTA op = { 0 };
    memset(from, 0, sizeof from);
    snprintf(from, sizeof from - 2, "%s", dir);
    for (char *p = from; *p; p++) if (*p == '/') *p = '\\';
    op.wFunc = FO_DELETE;
    op.pFrom = from;
    op.fFlags = FOF_NO_UI;
    SHFileOperationA(&op);
}

/* ── Installing ────────────────────────────────────────────── */

static int has_ext(const char *path, const char *ext)
{
    size_t lp = strlen(path), le = strlen(ext);
    return lp >= le && !_stricmp(path + lp - le, ext);
}

int rky_install(const char *src, const char *save_dir, char *game_dir, size_t n,
                rky_progress_fn progress, void *ud)
{
    char dest[MAX_PATH * 2], tmp[MAX_PATH * 2], scratch[MAX_PATH * 2], iso[MAX_PATH * 2];
    int ok;

    /* An extracted copy: use the folder where it is. */
    if (has_ext(src, "default.xbe")) {
        char dir[MAX_PATH * 2];
        snprintf(dir, sizeof dir, "%s", src);
        dir[strlen(dir) - strlen("default.xbe") - 1] = 0;
        if (!rky_check_xbe(dir))
            return 0;
        snprintf(tmp, sizeof tmp, "%s/game_path.txt", save_dir);
        FILE *f = fopen(tmp, "w");
        if (f) { fputs(dir, f); fclose(f); }
        snprintf(game_dir, n, "%s", dir);
        return 1;
    }

    snprintf(dest, sizeof dest, "%s/game", save_dir);
    snprintf(tmp, sizeof tmp, "%s/game.partial", save_dir);
    snprintf(scratch, sizeof scratch, "%s/unpack.tmp", save_dir);
    remove_tree(tmp);
    remove_tree(scratch);

    snprintf(iso, sizeof iso, "%s", src);
    if (has_ext(src, ".zip") || has_ext(src, ".7z")) {
        char args[4096];
        CreateDirectoryA(scratch, NULL);
        if (progress) progress(0, 0, ud);   /* unknown length while unpacking */
        snprintf(args, sizeof args, "-xf \"%s\" -C \"%s\"", src, scratch);
        if (!run_tar(args)) {
            if (!s_err[0]) fail("Unpacking %s failed.", src);
            remove_tree(scratch);
            return 0;
        }
        if (!find_iso(scratch, iso, sizeof iso)) {
            fail("There is no disc image (.iso) inside %s.", src);
            remove_tree(scratch);
            return 0;
        }
    }

    ok = unpack_iso(iso, tmp, progress, ud) && rky_check_xbe(tmp);
    remove_tree(scratch);
    if (!ok) {
        remove_tree(tmp);
        return 0;
    }
    remove_tree(dest);
    if (!MoveFileA(tmp, dest)) {
        fail("Cannot rename %s to %s.", tmp, dest);
        return 0;
    }
    snprintf(game_dir, n, "%s", dest);
    return 1;
}

/* ── Finding the game ──────────────────────────────────────── */

int rky_find_game(const char *arg, const char *save_dir, const char *exe_dir,
                  char *out, size_t n)
{
    char cand[MAX_PATH * 2], xbe[MAX_PATH * 2];
    const char *env = getenv("RKY_GAME_DIR");

    if (arg && *arg) { snprintf(out, n, "%s", arg); return 1; }
    if (env && *env) { snprintf(out, n, "%s", env); return 1; }

    snprintf(cand, sizeof cand, "%s/game", save_dir);
    snprintf(xbe, sizeof xbe, "%s/default.xbe", cand);
    if (file_exists(xbe) && rky_check_xbe(cand)) { snprintf(out, n, "%s", cand); return 1; }

    snprintf(xbe, sizeof xbe, "%s/game_path.txt", save_dir);
    {
        FILE *f = fopen(xbe, "r");
        if (f) {
            if (fgets(cand, sizeof cand, f)) {
                cand[strcspn(cand, "\r\n")] = 0;
                snprintf(xbe, sizeof xbe, "%s/default.xbe", cand);
                if (file_exists(xbe) && rky_check_xbe(cand)) {
                    fclose(f);
                    snprintf(out, n, "%s", cand);
                    return 1;
                }
            }
            fclose(f);
        }
    }

    snprintf(cand, sizeof cand, "%sgame", exe_dir);
    snprintf(xbe, sizeof xbe, "%s/default.xbe", cand);
    if (file_exists(xbe) && rky_check_xbe(cand)) { snprintf(out, n, "%s", cand); return 1; }
    return 0;
}

/* ── The first-run dialog ──────────────────────────────────── */

typedef struct {
    HWND wnd, bar, text;
    const char *src, *save_dir;
    char game_dir[MAX_PATH * 2];
    volatile LONG done;
    int ok;
} InstallUi;

static void ui_progress(uint64_t done, uint64_t total, void *ud)
{
    InstallUi *ui = ud;
    static int last = -1;
    int pct = total ? (int)(done * 100 / total) : -1;
    if (pct == last)
        return;
    last = pct;
    PostMessageA(ui->wnd, WM_APP, (WPARAM)pct, 0);
}

static DWORD WINAPI install_thread(LPVOID arg)
{
    InstallUi *ui = arg;
    ui->ok = rky_install(ui->src, ui->save_dir, ui->game_dir, sizeof ui->game_dir,
                         ui_progress, ui);
    InterlockedExchange(&ui->done, 1);
    PostMessageA(ui->wnd, WM_APP + 1, 0, 0);
    return 0;
}

static LRESULT CALLBACK ui_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    InstallUi *ui = (InstallUi *)GetWindowLongPtrA(h, GWLP_USERDATA);
    if (m == WM_APP && ui) {
        char t[128];
        int pct = (int)w;
        if (pct < 0) {
            SetWindowTextA(ui->text, "Unpacking the archive...");
            SendMessageA(ui->bar, PBM_SETMARQUEE, TRUE, 30);
        } else {
            SetWindowLongPtrA(ui->bar, GWL_STYLE,
                              GetWindowLongPtrA(ui->bar, GWL_STYLE) & ~PBS_MARQUEE);
            SendMessageA(ui->bar, PBM_SETPOS, (WPARAM)pct, 0);
            snprintf(t, sizeof t, "Installing the game files... %d%%", pct);
            SetWindowTextA(ui->text, t);
        }
        return 0;
    }
    if (m == WM_APP + 1) {
        DestroyWindow(h);
        return 0;
    }
    if (m == WM_CLOSE)
        return 0;   /* not while copying */
    if (m == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static int install_with_progress(const char *src, const char *save_dir, char *out, size_t n)
{
    InstallUi ui = { 0 };
    WNDCLASSA wc = { 0 };
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_PROGRESS_CLASS };
    MSG msg;

    InitCommonControlsEx(&icc);
    wc.lpfnWndProc = ui_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "RockyInstall";
    RegisterClassA(&wc);
    ui.src = src;
    ui.save_dir = save_dir;
    ui.wnd = CreateWindowA("RockyInstall", "Rocky Recompiled - installing",
                           WS_OVERLAPPED | WS_CAPTION | WS_VISIBLE,
                           CW_USEDEFAULT, CW_USEDEFAULT, 460, 140, NULL, NULL, wc.hInstance, NULL);
    SetWindowLongPtrA(ui.wnd, GWLP_USERDATA, (LONG_PTR)&ui);
    ui.text = CreateWindowA("STATIC", "Reading the disc image...", WS_CHILD | WS_VISIBLE,
                            16, 16, 420, 20, ui.wnd, NULL, wc.hInstance, NULL);
    ui.bar = CreateWindowA(PROGRESS_CLASSA, NULL, WS_CHILD | WS_VISIBLE | PBS_MARQUEE,
                           16, 46, 420, 22, ui.wnd, NULL, wc.hInstance, NULL);
    SendMessageA(ui.bar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    CloseHandle(CreateThread(NULL, 0, install_thread, &ui, 0, NULL));
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    while (!ui.done)
        Sleep(10);
    if (ui.ok)
        snprintf(out, n, "%s", ui.game_dir);
    return ui.ok;
}

int rky_first_run(const char *save_dir, char *out, size_t n)
{
    for (;;) {
        char file[MAX_PATH * 2] = "";
        OPENFILENAMEA ofn = { sizeof ofn };
        int r = MessageBoxA(NULL,
            "Rocky Recompiled needs your copy of Rocky for the original Xbox (USA).\n\n"
            "Choose its disc image (.iso, or the .zip or .7z it came in), or the "
            "default.xbe of an extracted copy. A disc image is installed once "
            "(about 800 MB).",
            "Rocky Recompiled", MB_OKCANCEL | MB_ICONINFORMATION);
        if (r != IDOK)
            return 0;
        ofn.lpstrFilter = "Rocky disc image or default.xbe\0*.iso;*.xiso;*.zip;*.7z;default.xbe\0"
                          "All files\0*.*\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = sizeof file;
        ofn.lpstrTitle = "Choose your Rocky disc image";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameA(&ofn))
            continue;
        s_err[0] = 0;
        if (install_with_progress(file, save_dir, out, n))
            return 1;
        MessageBoxA(NULL, s_err[0] ? s_err : "The installation failed.",
                    "Rocky Recompiled", MB_OK | MB_ICONERROR);
    }
}
