# Bring-up notes

## Binary facts

- USA retail XBE, title `0x55530014`, built 2002-08-14, XDK 4721. Debug path
  `C:\Boxing\Game\Game.exe`. Entry `0x0009AE9C`, kernel thunks `0x000F7500`
  (121 imports).
- Statically linked libraries in named sections: D3D, XGRPH, DSOUND, XPP and
  RAD's Bink (BINK, BINK32/32A/16/4444/5551/YUY2..., data in BINKDATA).
- The disc copies its `.PAK` archives to the cache partition
  (`\Device\Harddisk0\Partition5`) on first use and reads them from there.
- D3D device: static at `0x000B74F0` (pointer at `0x000BA130`). device+0x30 is
  the pushbuffer write sequence, device+0x34 points at the GPU semaphore.
- Game state worth knowing: `0x003AABD0` is the game's own "movies on" flag
  (the movie player returns at once when it is clear).

## Pipeline results (2026-10-24)

| Step | Result |
|---|---|
| disasm + seeds | 3,562 functions (130 seeds in `config/seed_functions.json`) |
| Ghidra 12.1.4 naming | 89 names applied |
| recomp | 3,559 lifted, 2 failed, 198 unresolved call targets stubbed, 15 chunks |

## Bring-up log

### 2026-10-24

1. **Hang in D3D device creation.** BlockOnTime (`0x000AF6D0`) spun at
   `0x000AF7D0`: the GPU stopped at GET = pushbuffer start + 4, the data word
   of the first SET_OBJECT. XDK 4721's D3D leaves PGRAPH CTX_CONTROL's
   channel-valid bit clear (it writes `0x10000100` to 0x400144), so the GPU
   raises a context-switch interrupt and waits for D3D's ISR to service it.
   The ISR (`0x000B0F20`, passed to KeConnectInterrupt at `0x000B1FDF` as an
   immediate) was not a detected function, so the kernel could not run it
   (`RKY_STATE_LOG` showed the line raised and 0 ISR calls).
   `scripts/find_code_pointers.py` found it and 15 more callbacks.
   Result: Newcastle Studio logo.
2. **Black movies.** The CRT's MMX check (`0x0009BF40`) uses pushfd/popfd and
   cpuid, which the lifter leaves as no-ops; it answered "no MMX", so Bink's
   setup (`0x000DE3AB`) skipped choosing its row converters and
   BinkCopyToBuffer called a null pointer for every row. `recomp_manual.c`
   answers yes. Bink's converters then lived in BINKDATA pointer tables the
   detector missed: 113 seeded (`scripts/find_table_pointers.py`).
3. **Bink decoder crash.** A few frames into the first movie the lifted
   decoder makes an indirect call to `0x000E2601` (mid-instruction, so a
   corrupted computed target) and then faults in `0x000DA470`. Movies are
   skipped (`0x0006BD50` returns as if movies were off) unless
   `RKY_MOVIES=1`. Next step: find the lifting bug, or play movies on the
   host.
4. **Result:** legal screen, title screen, main menu, Movie Mode (career
   start, difficulty, name entry), the ring announcer intro and a fight
   (Rocky vs Rico). Music plays (`RECOMP_AUDIO_WAV`; a little hot, with some
   clipping at peaks).

### Open issues

- Movies (above).
- `[THUNK] 233 kernel imports are unresolved` warning at boot, with only 121
  imports; ordinals 37 and 91 are unresolved. Nothing has failed from it yet.
- Audio peaks clip.

## Bring-up tools

- `RKY_STATE_LOG=<s>`: GPU queue/interrupt state and kernel interrupt counts
  every s seconds.
- `RKY_BINK_LOG=1`: where each Bink frame went and whether it is all black.
- `scripts/shot_run.ps1 -Out <prefix> -At "15,30" -Steps "22=RETURN,25=DOWN"`:
  run, press keys (keyboard controls as in Animaniacs: Enter = Start,
  Space/K = A, L = B, arrows = D-pad), screenshot the window.
- `scripts/contact_sheet.ps1`, `scripts/winshot.ps1`, `scripts/disx.py <va>`,
  `scripts/guest_mem.py peek <va[:*][:*+off]>`, `scripts/thread_stacks.py`,
  `scripts/sample_profile.py`, `scripts/build_vk.ps1 [-Regen]`.
