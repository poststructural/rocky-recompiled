/**
 * Mods: files that replace the game's own.
 *
 * A mod is a folder under mods/ (beside the executable, or in the save
 * folder) holding files at the same paths as on the disc, for example
 * mods/MyRings/ARENAS/MADISON/MADISON.PAK. Mods apply in folder-name order,
 * a later one winning where two replace the same file; a mod folder holding
 * a file named "disabled" is skipped.
 *
 * The runtime asks rky_mod_override (through xbox_path_override) about every
 * game-folder path it opens, and gets the mod's file instead where there is
 * one. Rocky copies its archives to the hard-disk cache on first use and
 * reads them from there afterwards, so a cached copy of any file a mod adds,
 * changes or stops replacing is deleted at start-up; the game then copies it
 * again, from the mod.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods.h"

typedef struct {
    char rel[260];          /* path below the disc root, upper case, backslashes */
    WCHAR host[MAX_PATH];   /* the mod's file */
    char mod[64];
    uint64_t size, mtime;
    int logged;
} ModFile;

static ModFile *s_files;
static int s_count, s_cap;
static RkyModInfo s_mods[RKY_MAX_MODS];
static int s_mod_count;

static void upper_rel(char *s)
{
    for (; *s; s++) {
        if (*s == '/') *s = '\\';
        else if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
    }
}

static int cmp_file(const void *a, const void *b)
{
    return strcmp(((const ModFile *)a)->rel, ((const ModFile *)b)->rel);
}

static ModFile *find(const char *rel)
{
    ModFile key;
    snprintf(key.rel, sizeof key.rel, "%s", rel);
    upper_rel(key.rel);
    return s_count ? bsearch(&key, s_files, (size_t)s_count, sizeof *s_files, cmp_file) : NULL;
}

static void add_file(const char *rel, const char *path, const char *mod, uint64_t size,
                     uint64_t mtime)
{
    ModFile *f;
    char r[260];
    snprintf(r, sizeof r, "%s", rel);
    upper_rel(r);
    for (int i = 0; i < s_count; i++)
        if (!strcmp(s_files[i].rel, r)) {   /* a later mod replaces an earlier one */
            f = &s_files[i];
            goto fill;
        }
    if (s_count == s_cap) {
        s_cap = s_cap ? s_cap * 2 : 64;
        s_files = realloc(s_files, (size_t)s_cap * sizeof *s_files);
    }
    f = &s_files[s_count++];
    memset(f, 0, sizeof *f);
    snprintf(f->rel, sizeof f->rel, "%s", r);
fill:
    MultiByteToWideChar(CP_ACP, 0, path, -1, f->host, MAX_PATH);
    snprintf(f->mod, sizeof f->mod, "%s", mod);
    f->size = size;
    f->mtime = mtime;
}

static void walk(const char *dir, const char *rel, const char *mod, int *files)
{
    char pat[MAX_PATH * 2];
    WIN32_FIND_DATAA fd;
    HANDLE h;

    snprintf(pat, sizeof pat, "%s\\*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        char path[MAX_PATH * 2], r[MAX_PATH * 2];
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
            continue;
        snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
        snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "\\" : "", fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            walk(path, r, mod, files);
        } else if (*rel || (_stricmp(fd.cFileName, "disabled") &&
                            _stricmp(fd.cFileName, "readme.txt") &&
                            _stricmp(fd.cFileName, "mod.txt"))) {
            add_file(r, path, mod,
                     (uint64_t)fd.nFileSizeHigh << 32 | fd.nFileSizeLow,
                     (uint64_t)fd.ftLastWriteTime.dwHighDateTime << 32 |
                         fd.ftLastWriteTime.dwLowDateTime);
            (*files)++;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static int cmp_name(const void *a, const void *b)
{
    return _stricmp(((const RkyModInfo *)a)->name, ((const RkyModInfo *)b)->name);
}

static void scan_root(const char *root)
{
    char pat[MAX_PATH * 2];
    WIN32_FIND_DATAA fd;
    HANDLE h;

    snprintf(pat, sizeof pat, "%s\\*", root);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        RkyModInfo *m;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.' ||
            s_mod_count == RKY_MAX_MODS)
            continue;
        m = &s_mods[s_mod_count++];
        memset(m, 0, sizeof *m);
        snprintf(m->name, sizeof m->name, "%s", fd.cFileName);
        snprintf(m->path, sizeof m->path, "%s\\%s", root, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* The cache copies to drop: every file replaced now or at the last start. */
static void refresh_cache(const char *save_dir)
{
    char list_path[MAX_PATH * 2], line[600], cache[MAX_PATH * 2];
    char *old = NULL, *now = NULL;
    size_t old_n = 0, now_n = 0;
    FILE *f;

    snprintf(cache, sizeof cache, "%s\\Cache", save_dir);
    snprintf(list_path, sizeof list_path, "%s\\mods_in_cache.txt", cache);

    /* This start's list: rel|size|mtime per line. */
    for (int i = 0; i < s_count; i++) {
        char l[600];
        int n = snprintf(l, sizeof l, "%s|%llu|%llu\n", s_files[i].rel,
                         (unsigned long long)s_files[i].size,
                         (unsigned long long)s_files[i].mtime);
        now = realloc(now, now_n + (size_t)n + 1);
        memcpy(now + now_n, l, (size_t)n + 1);
        now_n += (size_t)n;
    }
    f = fopen(list_path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        old_n = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        old = malloc(old_n + 1);
        old_n = fread(old, 1, old_n, f);
        old[old_n] = 0;
        fclose(f);
    }
    if ((old_n == now_n && (!now_n || !memcmp(old, now, now_n))) ) {
        free(old);
        free(now);
        return;
    }
    /* Changed: drop the cached copy of every file in either list. */
    for (int pass = 0; pass < 2; pass++) {
        char *p = pass ? now : old;
        while (p && *p) {
            char *e = strchr(p, '\n'), *bar, *base;
            size_t len = e ? (size_t)(e - p) : strlen(p);
            snprintf(line, sizeof line, "%.*s", (int)len, p);
            bar = strchr(line, '|');
            if (bar) *bar = 0;
            base = strrchr(line, '\\');
            base = base ? base + 1 : line;
            if (*base) {
                char c[MAX_PATH * 2];
                snprintf(c, sizeof c, "%s\\%s", cache, base);
                if (DeleteFileA(c))
                    fprintf(stderr, "[MODS] cleared cached %s\n", base);
            }
            p = e ? e + 1 : NULL;
        }
    }
    CreateDirectoryA(cache, NULL);
    f = fopen(list_path, "wb");
    if (f) {
        if (now_n)
            fwrite(now, 1, now_n, f);
        fclose(f);
    }
    free(old);
    free(now);
}

static void override(const char *rel, WCHAR *host, DWORD size)
{
    ModFile *f = find(rel);
    if (!f)
        return;
    wcsncpy_s(host, size, f->host, _TRUNCATE);
    if (!f->logged) {
        f->logged = 1;
        fprintf(stderr, "[MODS] %s from %s\n", f->rel, f->mod);
    }
}

extern void (*xbox_path_override)(const char *rel, WCHAR *host_path, DWORD size);

void rky_mods_init(const char *exe_dir, const char *save_dir)
{
    char root[MAX_PATH * 2];

    s_mod_count = 0;
    s_count = 0;
    {
        char other[MAX_PATH * 2];
        snprintf(root, sizeof root, "%smods", exe_dir);
        CreateDirectoryA(root, NULL);
        scan_root(root);
        snprintf(other, sizeof other, "%s\\mods", save_dir);
        if (_stricmp(root, other) != 0)
            scan_root(other);
    }
    qsort(s_mods, (size_t)s_mod_count, sizeof s_mods[0], cmp_name);
    for (int i = 0; i < s_mod_count; i++) {
        char dis[MAX_PATH * 2];
        snprintf(dis, sizeof dis, "%s\\disabled", s_mods[i].path);
        s_mods[i].enabled = GetFileAttributesA(dis) == INVALID_FILE_ATTRIBUTES;
        if (s_mods[i].enabled)
            walk(s_mods[i].path, "", s_mods[i].name, &s_mods[i].files);
        fprintf(stderr, "[MODS] %s: %s, %d file(s)\n", s_mods[i].name,
                s_mods[i].enabled ? "on" : "off", s_mods[i].files);
    }
    qsort(s_files, (size_t)s_count, sizeof *s_files, cmp_file);
    refresh_cache(save_dir);
    xbox_path_override = s_count ? override : NULL;
}

int rky_mods_list(const RkyModInfo **mods)
{
    *mods = s_mods;
    return s_mod_count;
}

int rky_mod_set_enabled(int i, int on)
{
    char dis[MAX_PATH * 2];
    if (i < 0 || i >= s_mod_count)
        return 0;
    snprintf(dis, sizeof dis, "%s\\disabled", s_mods[i].path);
    if (on) {
        DeleteFileA(dis);
    } else {
        FILE *f = fopen(dis, "w");
        if (!f) return 0;
        fputs("This mod is switched off. Delete this file (or use the launcher) to turn it on.\n", f);
        fclose(f);
    }
    s_mods[i].enabled = on;
    return 1;
}
