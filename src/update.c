/**
 * Updates from the project's GitLab releases.
 *
 * A check reads https://gitlab.com/api/v4/projects/<id>/releases (newest
 * first) and compares the newest tag (vX.Y.Z) with this build's version. A
 * release carries two asset links: RockyRecompiled-X.Y.Z.zip and
 * RockyRecompiled-X.Y.Z.zip.sha256 (the zip holds one folder,
 * RockyRecompiled-X.Y.Z/, with the files to put beside Rocky.exe).
 *
 * Installing downloads both into <save dir>\update, checks the zip's
 * SHA-256, unpacks it with Windows' tar.exe, and copies each file over its
 * counterpart beside the executable; the file it replaces is kept as
 * <name>.old (a running Rocky.exe can be renamed, not overwritten) and
 * removed at the next start. Only files the package brings are touched:
 * settings.ini, saves, mods and the game files are never in a package (and
 * are refused if one tries).
 *
 * HTTPS only, to gitlab.com and the storage it redirects downloads to.
 */
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "update.h"

#define RKY_GITLAB_PROJECT "88056818"   /* gitlab.com/unowa001/rocky-recomp */
#define RKY_PKG_PREFIX     "RockyRecompiled-"

static volatile LONG s_state = RKY_UPD_IDLE;
static volatile LONG s_progress;
static char s_latest[32], s_notes[160], s_err[256];
static char s_zip_url[1024], s_sha_url[1024];
static char s_exe_dir[MAX_PATH * 2];

int rky_update_state(void) { return (int)s_state; }
const char *rky_update_latest(void) { return s_latest; }
const char *rky_update_notes(void) { return s_notes; }
int rky_update_progress(void) { return (int)s_progress; }
const char *rky_update_error(void) { return s_err; }

static void failf(const char *fmt, const char *arg)
{
    snprintf(s_err, sizeof s_err, fmt, arg ? arg : "");
    InterlockedExchange(&s_state, RKY_UPD_FAILED);
}

/* ── HTTPS ─────────────────────────────────────────────────── */

typedef struct { char *data; size_t len, cap; } Buf;

static int host_allowed(const wchar_t *h)
{
    return !_wcsicmp(h, L"gitlab.com") || !_wcsicmp(h, L"storage.googleapis.com") ||
           !_wcsicmp(h, L"cdn.gitlab.com");
}

/* GET url into out (at most limit bytes). FILE *to: write there instead. */
/* Testing (RKY_UPDATE_FEED=<releases.json>): the feed and the files it links
 * as file:<path> are read from disk instead of GitLab. */
static int local_get(const char *path, Buf *out, FILE *to, size_t limit)
{
    char buf[65536];
    size_t n;
    FILE *f = fopen(path, "rb");
    if (!f) { failf("Test feed: cannot read %s.", path); return 0; }
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        if (to) fwrite(buf, 1, n, to);
        else {
            if (out->len + n + 1 > out->cap) out->data = realloc(out->data, out->cap = (out->len + n + 1) * 2);
            memcpy(out->data + out->len, buf, n);
            out->len += n;
            out->data[out->len] = 0;
        }
        if (out && out->len > limit) break;
    }
    fclose(f);
    InterlockedExchange(&s_progress, 100);
    return 1;
}

static int http_get(const char *url, Buf *out, FILE *to, size_t limit, int progress)
{
    wchar_t wurl[2048];
    if (getenv("RKY_UPDATE_FEED") && !strncmp(url, "file:", 5))
        return local_get(url + 5, out, to, limit);
    HINTERNET ses = WinHttpOpen(L"RockyRecompiled/" RKY_VERSION, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    int ok = 0;
    if (!ses) return 0;
    WinHttpSetTimeouts(ses, 10000, 10000, 20000, 30000);
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048);
    for (int hop = 0; hop < 6 && !ok; hop++) {
        URL_COMPONENTS u = { sizeof u };
        wchar_t host[256], path[2048];
        HINTERNET con, req;
        DWORD status = 0, sz = sizeof status, total = 0, got = 0;
        u.lpszHostName = host; u.dwHostNameLength = 256;
        u.lpszUrlPath = path; u.dwUrlPathLength = 2048;
        u.dwExtraInfoLength = (DWORD)-1;
        if (!WinHttpCrackUrl(wurl, 0, 0, &u) || u.nScheme != INTERNET_SCHEME_HTTPS ||
            !host_allowed(host)) {
            failf("Update refused: the download is not on gitlab.com over HTTPS.", NULL);
            break;
        }
        if (u.dwExtraInfoLength && u.lpszExtraInfo)
            wcsncat(path, u.lpszExtraInfo, 2047 - wcslen(path));
        con = WinHttpConnect(ses, host, u.nPort, 0);
        req = con ? WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
        if (req) {
            DWORD pol = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &pol, sizeof pol);
        }
        if (!req || !WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) ||
            !WinHttpReceiveResponse(req, NULL) ||
            !WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX)) {
            failf("Could not reach GitLab. Check the internet connection.", NULL);
            if (req) WinHttpCloseHandle(req);
            if (con) WinHttpCloseHandle(con);
            break;
        }
        if (status >= 300 && status < 400) {
            DWORD n = sizeof wurl;
            if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                     wurl, &n, WINHTTP_NO_HEADER_INDEX))
                failf("GitLab sent a bad redirect.", NULL);
            WinHttpCloseHandle(req);
            WinHttpCloseHandle(con);
            if (s_state == RKY_UPD_FAILED) break;
            continue;
        }
        if (status != 200) {
            char m[64];
            snprintf(m, sizeof m, "%lu", (unsigned long)status);
            if (status == 429)
                failf("GitLab is limiting requests from this network right now. Try again in a few minutes.", NULL);
            else
                failf("GitLab answered HTTP %s.", m);
            WinHttpCloseHandle(req);
            WinHttpCloseHandle(con);
            break;
        }
        sz = sizeof total;
        WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &total, &sz, WINHTTP_NO_HEADER_INDEX);
        for (;;) {
            char chunk[65536];
            DWORD n = 0;
            if (!WinHttpReadData(req, chunk, sizeof chunk, &n)) { failf("The download broke off.", NULL); break; }
            if (!n) { ok = 1; break; }
            got += n;
            if (got > limit) { failf("The download is larger than expected.", NULL); break; }
            if (to) {
                if (fwrite(chunk, 1, n, to) != n) { failf("Could not write the download (disk full?).", NULL); break; }
            } else {
                if (out->len + n + 1 > out->cap) {
                    out->cap = (out->len + n + 1) * 2;
                    out->data = realloc(out->data, out->cap);
                }
                memcpy(out->data + out->len, chunk, n);
                out->len += n;
                out->data[out->len] = 0;
            }
            if (progress && total)
                InterlockedExchange(&s_progress, (LONG)((uint64_t)got * 100 / total));
        }
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(con);
        break;
    }
    WinHttpCloseHandle(ses);
    return ok;
}

/* ── A small JSON reader (the releases list) ───────────────── */

typedef struct Json {
    char type;             /* o a s n (number/true/false/null) */
    char *str;             /* s: the string; n: the token */
    char **keys;           /* o */
    struct Json *items;    /* o, a */
    int count;
} Json;

static const char *ws(const char *p) { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; return p; }

static const char *parse_string(const char *p, char **out)
{
    size_t cap = 64, n = 0;
    char *s = malloc(cap);
    p++;   /* the quote */
    while (*p && *p != '"') {
        unsigned c = (unsigned char)*p++;
        if (c == '\\') {
            c = (unsigned char)*p++;
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            else if (c == 'r') c = '\r';
            else if (c == 'b' || c == 'f') c = ' ';
            else if (c == 'u') {
                unsigned v = 0;
                for (int i = 0; i < 4 && *p; i++, p++)
                    v = v * 16 + (unsigned)(*p <= '9' ? *p - '0' : (*p | 32) - 'a' + 10);
                c = v < 128 ? v : '?';
            }
        }
        if (n + 2 > cap) s = realloc(s, cap *= 2);
        s[n++] = (char)c;
    }
    s[n] = 0;
    *out = s;
    return *p ? p + 1 : p;
}

static const char *parse(const char *p, Json *j, int depth)
{
    memset(j, 0, sizeof *j);
    p = ws(p);
    if (depth > 24 || !*p) return NULL;
    if (*p == '{' || *p == '[') {
        int obj = *p == '{';
        j->type = obj ? 'o' : 'a';
        p = ws(p + 1);
        if (*p == (obj ? '}' : ']')) return p + 1;
        for (;;) {
            j->items = realloc(j->items, sizeof *j->items * (size_t)(j->count + 1));
            if (obj) {
                j->keys = realloc(j->keys, sizeof *j->keys * (size_t)(j->count + 1));
                p = ws(p);
                if (*p != '"') return NULL;
                p = parse_string(p, &j->keys[j->count]);
                p = ws(p);
                if (*p++ != ':') return NULL;
            }
            p = parse(p, &j->items[j->count], depth + 1);
            if (!p) return NULL;
            j->count++;
            p = ws(p);
            if (*p == ',') { p++; continue; }
            if (*p == (obj ? '}' : ']')) return p + 1;
            return NULL;
        }
    }
    if (*p == '"') { j->type = 's'; return parse_string(p, &j->str); }
    {
        const char *s = p;
        while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\r')
            p++;
        j->type = 'n';
        j->str = malloc((size_t)(p - s) + 1);
        memcpy(j->str, s, (size_t)(p - s));
        j->str[p - s] = 0;
        return p;
    }
}

static void json_free(Json *j)
{
    for (int i = 0; i < j->count; i++) {
        json_free(&j->items[i]);
        if (j->keys) free(j->keys[i]);
    }
    free(j->items);
    free(j->keys);
    free(j->str);
}

static const Json *get(const Json *o, const char *key)
{
    if (!o || o->type != 'o') return NULL;
    for (int i = 0; i < o->count; i++)
        if (!strcmp(o->keys[i], key)) return &o->items[i];
    return NULL;
}

static const char *gets_(const Json *o, const char *key)
{
    const Json *v = get(o, key);
    return v && v->type == 's' ? v->str : "";
}

/* ── Versions ──────────────────────────────────────────────── */

static int parse_ver(const char *s, int v[3])
{
    if (*s == 'v' || *s == 'V') s++;
    return sscanf(s, "%d.%d.%d", &v[0], &v[1], &v[2]) == 3 && !strchr(s, '-');
}

static int newer(const char *tag)
{
    int a[3], b[3];
    if (!parse_ver(tag, a) || !parse_ver(RKY_VERSION, b)) return 0;
    for (int i = 0; i < 3; i++)
        if (a[i] != b[i]) return a[i] > b[i];
    return 0;
}

/* ── Check ─────────────────────────────────────────────────── */

static DWORD WINAPI check_thread(LPVOID unused)
{
    Buf b = { 0 };
    Json root;
    (void)unused;
    s_err[0] = 0;
    if (getenv("RKY_UPDATE_FEED") ? !local_get(getenv("RKY_UPDATE_FEED"), &b, NULL, 4u << 20)
        : !http_get("https://gitlab.com/api/v4/projects/" RKY_GITLAB_PROJECT "/releases?per_page=20",
                    &b, NULL, 4u << 20, 0)) {
        if (s_state != RKY_UPD_FAILED) failf("Could not read the releases.", NULL);
        free(b.data);
        return 0;
    }
    if (!parse(b.data, &root, 0) || root.type != 'a') {
        failf("GitLab's release list could not be read.", NULL);
        free(b.data);
        return 0;
    }
    free(b.data);
    /* The newest stable release with a package. */
    s_latest[0] = 0;
    for (int i = 0; i < root.count && !s_latest[0]; i++) {
        const Json *r = &root.items[i], *links = get(get(r, "assets"), "links");
        const char *tag = gets_(r, "tag_name");
        int v[3];
        char zip[96], sha[104];
        if (!parse_ver(tag, v) || !links || links->type != 'a') continue;
        snprintf(zip, sizeof zip, RKY_PKG_PREFIX "%d.%d.%d.zip", v[0], v[1], v[2]);
        snprintf(sha, sizeof sha, "%s.sha256", zip);
        s_zip_url[0] = s_sha_url[0] = 0;
        for (int k = 0; k < links->count; k++) {
            const Json *l = &links->items[k];
            const char *name = gets_(l, "name");
            const char *url = *gets_(l, "direct_asset_url") ? gets_(l, "direct_asset_url") : gets_(l, "url");
            if (!strcmp(name, zip)) snprintf(s_zip_url, sizeof s_zip_url, "%s", url);
            if (!strcmp(name, sha)) snprintf(s_sha_url, sizeof s_sha_url, "%s", url);
        }
        if (!s_zip_url[0] || !s_sha_url[0]) continue;
        snprintf(s_latest, sizeof s_latest, "%d.%d.%d", v[0], v[1], v[2]);
        snprintf(s_notes, sizeof s_notes, "%s", gets_(r, "description"));
        s_notes[strcspn(s_notes, "\r\n")] = 0;
        if (!s_notes[0]) snprintf(s_notes, sizeof s_notes, "%s", gets_(r, "name"));
    }
    json_free(&root);
    InterlockedExchange(&s_state, s_latest[0] && newer(s_latest) ? RKY_UPD_AVAILABLE : RKY_UPD_CURRENT);
    return 0;
}

void rky_update_check(void)
{
    LONG st = s_state;
    if (st == RKY_UPD_CHECKING || st == RKY_UPD_DOWNLOADING || st == RKY_UPD_READY) return;
    InterlockedExchange(&s_state, RKY_UPD_CHECKING);
    CloseHandle(CreateThread(NULL, 0, check_thread, NULL, 0, NULL));
}

/* ── Install ───────────────────────────────────────────────── */

static int sha256_file(const char *path, char hex[65])
{
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_HASH_HANDLE h;
    unsigned char digest[32], buf[65536];
    size_t n;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) { fclose(f); return 0; }
    BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0);
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        BCryptHashData(h, buf, (ULONG)n, 0);
    fclose(f);
    BCryptFinishHash(h, digest, sizeof digest, 0);
    BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    for (int i = 0; i < 32; i++)
        sprintf(hex + i * 2, "%02x", digest[i]);
    return 1;
}

static int run_tar(const char *zip, const char *dir)
{
    char cmd[4096], sys[MAX_PATH];
    STARTUPINFOA si = { sizeof si };
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    GetSystemDirectoryA(sys, sizeof sys);
    snprintf(cmd, sizeof cmd, "\"%s\\tar.exe\" -xf \"%s\" -C \"%s\"", sys, zip, dir);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return 0;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code == 0;
}

/* Never in a package: the player's own files. */
static int forbidden(const char *rel)
{
    static const char *const never[] = { "settings.ini", "portable.txt", "mods", "game",
                                         "userdata", "update" };
    char first[64];
    size_t n = strcspn(rel, "\\/");
    if (strstr(rel, "..") || strchr(rel, ':') || n >= sizeof first) return 1;
    memcpy(first, rel, n);
    first[n] = 0;
    for (size_t i = 0; i < sizeof never / sizeof never[0]; i++)
        if (!_stricmp(first, never[i])) return 1;
    return 0;
}

/* Copy every file of src (relative path rel) over exe_dir; 0 on failure. */
static int apply_tree(const char *src, const char *rel, int *count)
{
    char pat[MAX_PATH * 2];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    int ok = 1;
    snprintf(pat, sizeof pat, "%s\\*", src);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        char from[MAX_PATH * 2], r[MAX_PATH * 2], to[MAX_PATH * 2], old[MAX_PATH * 2 + 8];
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        snprintf(from, sizeof from, "%s\\%s", src, fd.cFileName);
        snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "\\" : "", fd.cFileName);
        if (forbidden(r)) { failf("The update package contains a file it must not replace (%s).", r); ok = 0; break; }
        snprintf(to, sizeof to, "%s%s", s_exe_dir, r);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CreateDirectoryA(to, NULL);
            ok = apply_tree(from, r, count);
            continue;
        }
        snprintf(old, sizeof old, "%s.old", to);
        DeleteFileA(old);
        if (GetFileAttributesA(to) != INVALID_FILE_ATTRIBUTES && !MoveFileA(to, old)) {
            failf("Could not replace %s (is another copy of Rocky running?).", r);
            ok = 0;
            break;
        }
        if (!CopyFileA(from, to, FALSE)) {
            MoveFileA(old, to);
            failf("Could not write %s.", r);
            ok = 0;
            break;
        }
        (*count)++;
    } while (ok && FindNextFileA(h, &fd));
    FindClose(h);
    return ok;
}

static void remove_tree(const char *dir)
{
    char from[MAX_PATH * 2 + 2];
    SHFILEOPSTRUCTA op = { 0 };
    memset(from, 0, sizeof from);
    snprintf(from, sizeof from - 2, "%s", dir);
    op.wFunc = FO_DELETE;
    op.pFrom = from;
    op.fFlags = FOF_NO_UI;
    SHFileOperationA(&op);
}


static DWORD WINAPI install_thread(LPVOID unused)
{
    char dir[MAX_PATH * 2], zip[MAX_PATH * 2], stage[MAX_PATH * 2], pkg[MAX_PATH * 2], hex[65];
    Buf sha = { 0 };
    FILE *f;
    int count = 0;
    (void)unused;

    snprintf(dir, sizeof dir, "%supdate", s_exe_dir);
    remove_tree(dir);
    CreateDirectoryA(dir, NULL);
    snprintf(zip, sizeof zip, "%s\\package.zip", dir);
    snprintf(stage, sizeof stage, "%s\\stage", dir);
    if (!http_get(s_sha_url, &sha, NULL, 8192, 0)) { free(sha.data); return 0; }
    f = fopen(zip, "wb");
    if (!f) { failf("Could not write %s.", zip); free(sha.data); return 0; }
    if (!http_get(s_zip_url, NULL, f, 1u << 30, 1)) { fclose(f); free(sha.data); return 0; }
    fclose(f);
    if (!sha256_file(zip, hex) || sha.len < 64 || _strnicmp(sha.data, hex, 64)) {
        failf("The download is damaged (its SHA-256 does not match). Nothing was changed.", NULL);
        free(sha.data);
        return 0;
    }
    free(sha.data);
    CreateDirectoryA(stage, NULL);
    if (!run_tar(zip, stage)) { failf("The update package could not be unpacked.", NULL); return 0; }
    snprintf(pkg, sizeof pkg, "%s\\" RKY_PKG_PREFIX "%s", stage, s_latest);
    {
        char exe[MAX_PATH * 2];
        snprintf(exe, sizeof exe, "%s\\Rocky.exe", pkg);
        if (GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES) {
            failf("The update package is not laid out as expected.", NULL);
            return 0;
        }
    }
    if (!apply_tree(pkg, "", &count))
        return 0;
    remove_tree(dir);
    InterlockedExchange(&s_state, RKY_UPD_READY);
    return 0;
}

void rky_update_install(const char *exe_dir)
{
    if (s_state != RKY_UPD_AVAILABLE) return;
    snprintf(s_exe_dir, sizeof s_exe_dir, "%s", exe_dir);
    InterlockedExchange(&s_progress, 0);
    InterlockedExchange(&s_state, RKY_UPD_DOWNLOADING);
    CloseHandle(CreateThread(NULL, 0, install_thread, NULL, 0, NULL));
}

/* ── Start-up ──────────────────────────────────────────────── */

static void delete_old(const char *dir)
{
    char pat[MAX_PATH * 2];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(pat, sizeof pat, "%s*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char p[MAX_PATH * 2];
        size_t n = strlen(fd.cFileName);
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        snprintf(p, sizeof p, "%s%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!_stricmp(fd.cFileName, "licenses")) {
                char sub[MAX_PATH * 2];
                snprintf(sub, sizeof sub, "%s\\", p);
                delete_old(sub);
            }
        } else if (n > 4 && !_stricmp(fd.cFileName + n - 4, ".old")) {
            DeleteFileA(p);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

void rky_update_cleanup(const char *exe_dir)
{
    delete_old(exe_dir);
}

int rky_update_restart(const char *exe_dir)
{
    char exe[MAX_PATH * 2], cmd[MAX_PATH * 2 + 8];
    STARTUPINFOA si = { sizeof si };
    PROCESS_INFORMATION pi;
    snprintf(exe, sizeof exe, "%sRocky.exe", exe_dir);
    snprintf(cmd, sizeof cmd, "\"%s\"", exe);
    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, 0, NULL, exe_dir, &si, &pi))
        return 0;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 1;
}
