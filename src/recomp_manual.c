/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>   /* ptrdiff_t */

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

/* Thread-local in the runtime (RECOMP_TLS). A plain extern binds to the TLS
 * template, not the calling thread's register, so writes would be lost. */
#if defined(_MSC_VER)
#  define RKY_TLS __declspec(thread)
#else
#  define RKY_TLS _Thread_local
#endif
extern RKY_TLS uint32_t g_eax, g_esp;
extern ptrdiff_t g_xbox_mem_offset;

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
/* ── Rocky ─────────────────────────────────────────────────── */

/* Guest dword at an Xbox VA. */
#define GUEST32(va) (*(volatile uint32_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))

/* Display and QOL settings handed over by main_vk.c. Nothing reads them yet. */
float rky_aspect = 4.0f / 3.0f;
int rky_skip_intro;
int rky_hud_43 = 1;

/* A debug switch from the environment, read once: getenv in a per-frame hook
 * costs real frame time. */
#define RKY_ENV(name)     ({ static int v_ = -1; if (v_ < 0) v_ = getenv(name) != NULL; v_; })

/* MMX check (0x0009BF40, cdecl, one flags argument). Toggles EFLAGS.ID with
 * pushfd/popfd, runs cpuid(1) and tests EDX bit 23, then stores the answer at
 * 0x001127E0 and Bink's "use MMX" flag at 0x004D53A8 (cleared when the
 * argument has 0x40000 set). The lifter leaves pushfd, popfd and cpuid as
 * no-ops, so the check failed, Bink's setup (0x000DE3AB) skipped choosing its
 * row converters, BinkCopyToBuffer called a null converter for every row, and
 * every movie played black. The Xbox's Pentium III has MMX: answer yes. */
void sub_0009BF40(void)
{
    uint32_t flags = GUEST32(g_esp + 4);
    GUEST32(0x001127E0u) = 1;
    GUEST32(0x004D53A8u) = (flags & 0x40000u) ? 0 : 1;
    g_eax = 1;    /* eax still holds the MMX answer at the ret */
    g_esp += 4;   /* the ret: the caller pops the argument (cdecl) */
}

/* Video mode setup (0x00058F20). Reads XGetVideoStandard (0x00096BA7) and
 * XGetVideoFlags (0x00096BD0); with the dashboard's widescreen flag (bit 0)
 * it sets the projection aspect at 0x0010FF84 (4:3 by default) to 16:9 and
 * the game's widescreen flag at 0x0026339C, which the 3D camera divides by
 * (0x0010FF84 is read at the fdiv in the projection setup). The game's own
 * anamorphic widescreen, so the picture keeps its height and widens (Hor+).
 * Any shape wider than 4:3 is given to it here, ultrawide included. */
extern void sub_00058F20_gen(void);
void sub_00058F20(void)
{
    sub_00058F20_gen();
    if (rky_aspect > 4.0f / 3.0f + 0.01f) {
        memcpy((void *)((uintptr_t)g_xbox_mem_offset + 0x0010FF84u), &rky_aspect, 4);
        GUEST32(0x0026339Cu) = 1;
    }
}

/* Movie player (0x0006BD50, cdecl: name, ...; called through 0x0006BEC0 and
 * 0x0006BEE0). It plays the whole movie inside the call, and returns at once
 * when the game's own "movies on" flag (0x003AABD0) is clear.
 *
 * Bring-up: the lifted Bink decoder corrupts its state a few frames in (an
 * indirect call to 0x000E2601, mid-instruction, then a crash in 0x000DA470),
 * so movies are skipped the way the game skips them unless RKY_MOVIES=1. */
extern void sub_0006BD50_gen(void);
void sub_0006BD50(void)
{
    if (!RKY_ENV("RKY_MOVIES")) {
        g_eax = 0;
        g_esp += 4;   /* the ret: the caller pops the arguments (cdecl) */
        return;
    }
    sub_0006BD50_gen();
}

/* Label trace for a hand-instrumented generated function (bring-up only):
 * each label reached writes "va esp ebp" to RKY_TRACE_FILE. */
int rky_trace_on;
void rky_trace(uint32_t va, uint32_t esp, uint32_t ebp)
{
    static FILE *f;
    if (!f) {
        const char *p = getenv("RKY_TRACE_FILE");
        f = fopen(p ? p : "trace.txt", "w");
        if (!f) return;
    }
    fprintf(f, "%08X %08X %08X\n", va, esp, ebp);
}

/* Bink block decoder (0x000E14B0). RKY_BINK_GUARD=0xVA: report the call
 * (counting from 0) after which the dword at VA first reads 0x01010101 --
 * which decode call overran its block map. */
extern void sub_000E14B0_gen(void);
void sub_000E14B0(void)
{
    static unsigned calls;
    static int reported;
    static uint32_t guard = 1;
    if (guard == 1) {
        const char *e = getenv("RKY_BINK_GUARD");
        guard = e ? (uint32_t)strtoul(e, NULL, 16) : 0;
    }
    {
        extern int rky_trace_on;
        const char *t = getenv("RKY_TRACE_CALL");
        rky_trace_on = t && (unsigned)atoi(t) == calls;
    }
    sub_000E14B0_gen();
    {
        extern int rky_trace_on;
        rky_trace_on = 0;
    }
    if (guard && !reported && GUEST32(guard) == 0x01010101u) {
        reported = 1;
        fprintf(stderr, "[BINK] decode call %u overran into 0x%08X\n", calls, guard);
    }
    calls++;
}

/* XAPI ReleaseMutex (0x0009A4F0, stdcall: handle). RKY_MUTEX_LOG=1: each
 * failed release with its handle and caller. */
extern void sub_0009A4F0_gen(void);
void sub_0009A4F0(void)
{
    uint32_t h = GUEST32(g_esp + 4), ret = GUEST32(g_esp);
    sub_0009A4F0_gen();
    if (RKY_ENV("RKY_MUTEX_LOG") && !g_eax)
        fprintf(stderr, "[MUTEX] ReleaseMutex(0x%08X) failed, called from 0x%08X\n", h, ret);
}

/* BinkCopyToBuffer (0x000DCED0, stdcall: bink, dest, pitch, height, x, y,
 * flags). The movie player (0x0006BC80) locks a texture and has Bink write the
 * frame into it. RKY_BINK_LOG=1: where each frame went and how much of it is
 * not black. */
extern void sub_000DCED0_gen(void);
void sub_000DCED0(void)
{
    uint32_t dest = GUEST32(g_esp + 8), pitch = GUEST32(g_esp + 12);
    uint32_t height = GUEST32(g_esp + 16), flags = GUEST32(g_esp + 28);
    static unsigned frame;

    sub_000DCED0_gen();
    if (RKY_ENV("RKY_BINK_LOG") && frame++ % 30 == 0) {
        const uint8_t *p = (const uint8_t *)((uintptr_t)g_xbox_mem_offset + dest);
        unsigned nz = 0;
        for (uint32_t y = 0; y < height; y += 4)
            for (uint32_t x = 0; x < pitch; x += 3)
                nz += p[y * pitch + x] != 0;
        fprintf(stderr, "[BINK] frame %u -> 0x%08X pitch %u height %u flags 0x%X: "
                        "%u nonzero samples\n", frame - 1, dest, pitch, height, flags, nz);
    }
}

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    /* The call pushed its return address before dispatching: that names the
     * call site. */
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (returns to 0x%08X; total calls: %llu)\n",
            va, GUEST32(g_esp), (unsigned long long)g_icall_count);

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
}

/* ── Untranslated instructions ───────────────────────────────────────────
 *
 * The lifter emits RECOMP_UNIMPL(text, va) at every instruction it has no
 * translation for, in place of the bare comment it used to leave. The
 * instruction is still a no-op; this only stops the omission being silent.
 * RECOMP_UNIMPL_TRAP=1 aborts at the first hit, at the guest address of the
 * cause rather than wherever the damage surfaces. */
#include <stdlib.h>

void recomp_unimpl(const char *text, uint32_t va)
{
    static int printed;
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    int stop = trap && *trap && *trap != '0';

    if (printed < 50 || stop) {
        printed++;
        fprintf(stderr,
                "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X"
                " (a no-op; set RECOMP_UNIMPL_TRAP=1 to stop here)\n",
                text, va);
        fflush(stderr);
    }
    if (stop) abort();
}
