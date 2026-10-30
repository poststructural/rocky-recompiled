/**
 * PC wording: Rocky's own text, reworded for a PC.
 *
 * The game's menus and messages come from text files in FEND\FEND.PAK
 * (docs/MENU_MAP.md). At start-up this builds a copy of that archive with
 * the save-device and memory-unit messages reworded ("Xbox hard disk" ->
 * "This PC", and so on) in every language, and serves it in place of the
 * disc's through the mod path hook. The copy is made from the user's own
 * files (or from a mod's FEND.PAK, when one replaces it) and rebuilt when
 * its source or this table changes; nothing from the game is shipped.
 *
 * A line is reworded only in the shared part of a string file or its
 * PLATFORM "XBOX" part, only where its id is in the table, and never where
 * the original is blank or a translator's "!duplicate!" note. The game keeps
 * each string in a 64-byte slot, so every replacement is under 64 bytes.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods.h"
#include "pcpatch.h"

/* Bump when the table changes, so existing copies are rebuilt. */
#define PCPATCH_VERSION 1

typedef struct {
    const char *file;   /* inside FEND.PAK */
    int id;
    const char *text;   /* Windows-1252, as the game's files are */
} Reword;

static const Reword REWORDS[] = {
    { "STRS_EN.TXT", 156, "This PC" },
    { "STRS_EN.TXT", 245, "Disk full" },
    { "STRS_EN.TXT", 359, "Save data unreadable" },
    { "STRS_EN.TXT", 362, "Your PC doesn't have enough free disk space to save games." },
    { "STRS_EN.TXT", 365, "free up disk space, then try again." },
    { "STRS_EN.TXT", 370, "The save folder" },
    { "STRS_EN.TXT", 366, "Save folder" },
    { "STRS_EN.TXT", 367, "The save folder" },
    { "STRS_EN.TXT", 89,  "Save folder not found" },
    { "STRS_EN.TXT", 90,  "Disk full" },
    { "STRS_EN.TXT", 91,  "Checking saves" },

    { "STRS_DE.TXT", 156, "Dieser PC" },
    { "STRS_DE.TXT", 359, "Speicherdaten nicht lesbar." },
    { "STRS_DE.TXT", 362, "Dein PC hat nicht gen\xfcgend freien Speicherplatz." },
    { "STRS_DE.TXT", 365, "Speicherplatz freigeben, dann erneut versuchen." },
    { "STRS_DE.TXT", 370, "Der Speicherordner wurde entfernt" },
    { "STRS_DE.TXT", 366, "Speicherordner" },
    { "STRS_DE.TXT", 89,  "Speicherordner nicht gefunden." },
    { "STRS_DE.TXT", 90,  "Festplatte voll." },

    { "STRS_FR.TXT", 156, "Ce PC" },
    { "STRS_FR.TXT", 359, "Impossible de lire%nla sauvegarde" },
    { "STRS_FR.TXT", 362, "Votre PC n'a pas assez d'espace disque libre." },
    { "STRS_FR.TXT", 365, "lib\xe9rez de l'espace disque et r\xe9\x65ssayez." },
    { "STRS_FR.TXT", 370, "Le dossier de sauvegarde" },
    { "STRS_FR.TXT", 366, "Dossier de sauvegarde" },
    { "STRS_FR.TXT", 367, "Le dossier de sauvegarde" },
    { "STRS_FR.TXT", 89,  "Dossier de sauvegarde introuvable" },
    { "STRS_FR.TXT", 90,  "Disque plein" },

    { "STRS_ES.TXT", 156, "Este PC" },
    { "STRS_ES.TXT", 245, "Disco lleno" },
    { "STRS_ES.TXT", 359, "Datos guardados%nno disponibles." },
    { "STRS_ES.TXT", 362, "Tu PC no tiene suficiente espacio libre en disco." },
    { "STRS_ES.TXT", 365, "libera espacio en disco y vuelve a intentarlo." },
    { "STRS_ES.TXT", 370, "Se ha quitado la carpeta de guardado" },
    { "STRS_ES.TXT", 366, "Carpeta de guardado" },
    { "STRS_ES.TXT", 367, "La carpeta de guardado" },
    { "STRS_ES.TXT", 89,  "No se encuentra la carpeta de guardado." },
    { "STRS_ES.TXT", 90,  "Disco lleno." },

    { "STRS_IT.TXT", 156, "Questo PC" },
    { "STRS_IT.TXT", 245, "Disco pieno" },
    { "STRS_IT.TXT", 359, "Salvataggio%nilleggibile" },
    { "STRS_IT.TXT", 362, "Il PC non ha abbastanza spazio libero su disco." },
    { "STRS_IT.TXT", 365, "libera spazio su disco e riprova." },
    { "STRS_IT.TXT", 370, "La cartella dei salvataggi" },
    { "STRS_IT.TXT", 366, "Cartella salvataggi" },
    { "STRS_IT.TXT", 89,  "Cartella salvataggi non trovata" },
    { "STRS_IT.TXT", 90,  "Disco pieno" },
};
#define REWORD_COUNT ((int)(sizeof REWORDS / sizeof REWORDS[0]))

/* ── Text ──────────────────────────────────────────────────── */

typedef struct {
    char *p;
    size_t n, cap;
} Buf;

static void put(Buf *b, const char *s, size_t n)
{
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 256;
        b->p = realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
}

static const char *find_reword(const char *file, int id)
{
    for (int i = 0; i < REWORD_COUNT; i++)
        if (REWORDS[i].id == id && !_stricmp(REWORDS[i].file, file))
            return REWORDS[i].text;
    return NULL;
}

/* One string file, reworded. Returns the lines changed. */
static int reword_file(const char *file, const char *in, size_t n, Buf *out)
{
    int changed = 0, xbox = 1;   /* the shared part counts as Xbox */
    size_t i = 0;

    while (i < n) {
        size_t e = i, len;
        const char *l = in + i, *q1, *q2, *text;
        char line[512];
        int id;
        while (e < n && in[e] != '\n')
            e++;
        if (e < n)
            e++;                     /* keep the newline (and any \r before it) */
        len = e - i < sizeof line ? e - i : sizeof line - 1;
        memcpy(line, l, len);
        line[len] = 0;

        if (!strncmp(line, "CROSSPLATFORM", 13))
            xbox = 1;
        else if (!strncmp(line, "PLATFORM", 8))
            xbox = strstr(line, "\"XBOX\"") != NULL;

        if (xbox && !strncmp(line, "STRING", 6) && sscanf(line + 6, "%d", &id) == 1 &&
            (text = find_reword(file, id)) &&
            (q1 = memchr(l, '"', e - i)) && (q2 = memchr(q1 + 1, '"', (size_t)(in + e - q1 - 1))) &&
            q2 > q1 + 1 && !(q2 == q1 + 2 && q1[1] == ' ') &&
            q1[1] != '!' && (unsigned char)q1[1] != 0xA1) {
            put(out, l, (size_t)(q1 + 1 - l));
            put(out, text, strlen(text));
            put(out, q2, (size_t)(in + e - q2));
            changed++;
        } else {
            put(out, l, e - i);
        }
        i = e;
    }
    return changed;
}

/* ── The archive ───────────────────────────────────────────── */

/* FEND.PAK: "H5\\\0", a count, then 32-byte entries (name[24], offset,
 * size) in a table that runs up to the first file's data. */
typedef struct {
    char name[25];
    uint32_t off, size;
    char *data;              /* reworded copy, or NULL */
    size_t data_n;
} Entry;

static int build(const unsigned char *src, size_t n, Buf *out)
{
    uint32_t count, table_end = 0xFFFFFFFFu, pos;
    Entry *e;
    int changed = 0;

    if (n < 8 || memcmp(src, "H5\\", 4) != 0)
        return -1;
    memcpy(&count, src + 4, 4);
    if (count == 0 || count > 4096 || 8 + (size_t)count * 32 > n)
        return -1;
    e = calloc(count, sizeof *e);
    for (uint32_t i = 0; i < count; i++) {
        const unsigned char *r = src + 8 + i * 32;
        memcpy(e[i].name, r, 24);
        memcpy(&e[i].off, r + 24, 4);
        memcpy(&e[i].size, r + 28, 4);
        if ((size_t)e[i].off + e[i].size > n) {
            free(e);
            return -1;
        }
        if (e[i].off < table_end)
            table_end = e[i].off;
        if (!_strnicmp(e[i].name, "STRS_", 5)) {
            Buf b = { 0 };
            int c = reword_file(e[i].name, (const char *)src + e[i].off, e[i].size, &b);
            if (c) {
                e[i].data = b.p;
                e[i].data_n = b.n;
                changed += c;
            } else {
                free(b.p);
            }
        }
    }
    if (table_end < 8 + count * 32) {
        free(e);
        return -1;
    }
    /* The header as it was (table space and all); the files after it in the
     * same order, each at its new offset. */
    put(out, (const char *)src, table_end);
    pos = table_end;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t size = e[i].data ? (uint32_t)e[i].data_n : e[i].size;
        memcpy(out->p + 8 + i * 32 + 24, &pos, 4);
        memcpy(out->p + 8 + i * 32 + 28, &size, 4);
        if (e[i].data)
            put(out, e[i].data, e[i].data_n);
        else
            put(out, (const char *)src + e[i].off, e[i].size);
        pos += size;
        free(e[i].data);
    }
    free(e);
    return changed;
}

static unsigned char *read_all(const WCHAR *path, size_t *n)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    LARGE_INTEGER sz;
    unsigned char *p;
    DWORD got;
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > (64 << 20)) {
        CloseHandle(h);
        return NULL;
    }
    p = malloc((size_t)sz.QuadPart + 1);
    if (!ReadFile(h, p, (DWORD)sz.QuadPart, &got, NULL) || got != (DWORD)sz.QuadPart) {
        free(p);
        p = NULL;
    }
    CloseHandle(h);
    *n = (size_t)sz.QuadPart;
    return p;
}

void rky_pcpatch_init(const char *game_dir, const char *save_dir)
{
    WCHAR src[MAX_PATH * 2];
    char dir[MAX_PATH * 2], out_path[MAX_PATH * 2], stamp_path[MAX_PATH * 2];
    char stamp[128], old[128] = "";
    WIN32_FILE_ATTRIBUTE_DATA fa;
    unsigned char *data;
    size_t n;
    FILE *f;

    /* A mod's FEND.PAK when one replaces it, else the game's own. */
    if (!rky_mods_host_path("FEND\\FEND.PAK", src, MAX_PATH * 2)) {
        char p[MAX_PATH * 2];
        snprintf(p, sizeof p, "%s\\FEND\\FEND.PAK", game_dir);
        MultiByteToWideChar(CP_ACP, 0, p, -1, src, MAX_PATH * 2);
    }
    if (!GetFileAttributesExW(src, GetFileExInfoStandard, &fa)) {
        fprintf(stderr, "[PCPATCH] no FEND.PAK; the game's own text is used\n");
        return;
    }

    snprintf(dir, sizeof dir, "%s\\pcpatch", save_dir);
    CreateDirectoryA(dir, NULL);
    snprintf(out_path, sizeof out_path, "%s\\FEND.PAK", dir);
    snprintf(stamp_path, sizeof stamp_path, "%s\\FEND.PAK.stamp", dir);
    snprintf(stamp, sizeof stamp, "%d %lu %lu %lu", PCPATCH_VERSION, fa.nFileSizeLow,
             fa.ftLastWriteTime.dwLowDateTime, fa.ftLastWriteTime.dwHighDateTime);
    if ((f = fopen(stamp_path, "r"))) {
        if (!fgets(old, sizeof old, f)) old[0] = 0;
        fclose(f);
    }

    if (strcmp(old, stamp) != 0 || GetFileAttributesA(out_path) == INVALID_FILE_ATTRIBUTES) {
        Buf out = { 0 };
        int changed;
        if (!(data = read_all(src, &n))) {
            fprintf(stderr, "[PCPATCH] cannot read FEND.PAK; the game's own text is used\n");
            return;
        }
        changed = build(data, n, &out);
        free(data);
        if (changed <= 0) {
            fprintf(stderr, "[PCPATCH] FEND.PAK not as expected (%d); the game's own text is used\n",
                    changed);
            free(out.p);
            DeleteFileA(out_path);
            DeleteFileA(stamp_path);
            return;
        }
        if (!(f = fopen(out_path, "wb")) || fwrite(out.p, 1, out.n, f) != out.n) {
            if (f) fclose(f);
            free(out.p);
            DeleteFileA(out_path);
            fprintf(stderr, "[PCPATCH] cannot write %s\n", out_path);
            return;
        }
        fclose(f);
        free(out.p);
        if ((f = fopen(stamp_path, "w"))) {
            fputs(stamp, f);
            fclose(f);
        }
        fprintf(stderr, "[PCPATCH] built FEND.PAK, %d line(s) reworded\n", changed);
    }
    rky_mods_add_file("FEND\\FEND.PAK", out_path, "PC wording");
}
