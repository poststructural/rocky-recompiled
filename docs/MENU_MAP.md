# Rocky's front end: a map for PC menus

Notes for adding PC settings, Quit to Desktop, PC wording and PC button
prompts inside Rocky's own menus. Found by reading the USA retail XBE
(XDK 4721) and the scripts in `FEND\FEND.PAK`; nothing here was tested in
the running game yet. No game data is kept in the repo: the patches below
are meant to be applied at runtime to the files read from the user's disc.

## The big picture

Rocky's front end is data driven. Menus, their items, the pause menus, the
on-screen text, fonts and button prompts all come from text files in
`FEND\FEND.PAK`, read when the game boots. Adding a menu item is mostly a
script change plus a small code hook for what the item does, not a new
renderer. The text uses the game's own fonts, so new items look native for
free.

## FEND.PAK

| Offset | Size | Meaning |
|---|---|---|
| 0 | 4 | magic `48 35 5C 00` |
| 4 | 4 | entry count (91 on the USA disc) |
| 8 + 32·i | 24 | file name, NUL padded |
| +24 | 4 | offset |
| +28 | 4 | size |

The other `.PAK` files share this layout. The mod loader already replaces
whole files by disc path; a patched `FRONTEND.CSF` or `STRS_EN.TXT` can be
written into a generated PAK at first run (from the user's own copy) and
served through the same path override.

Text files inside: `FRONTEND.CSF` (menu script), `DEMO.CSF`,
`STRS_EN/DE/ES/FR/IT.TXT` (string banks), `CHARMAP.TXT` (font metrics),
`FRONTEND.TXT` (venues), `STRYMODE.TXT` (Movie Mode matches), `BIO*.TXT`.
Font and prompt art: `NEWFONT.DDS`, `ICONFONT.DDS`, `XBUTS.DDS`,
`BUTTONS.DDS`, `CONTROL.DDS`.

## FRONTEND.CSF (menu script)

Plain text, one record per line, `-` starts a comment, ends with `[EOF]`.
It begins with a `CHECKSUMS` line; whether the game verifies it is still to
be checked before editing the file.

| Section | Record | Count | Notes |
|---|---|---|---|
| Groups | `MENU` | 107 | one per screen: item count, first element, … |
| Group elements | `OBJECT x y type up down left right action kind index …` | 1086 | positions are in 640x480; `up/down/left/right` are the neighbouring element for pad navigation; `kind 4` points at a HUD text record |
| Special nodes | `SPECIAL id a b c` | 525 | code-driven widgets (sliders, lists); ids such as 31, 58, 70, 71, 76 |
| HUD text | `STRING strId font sx sy alpha align` | 191 | text drawn with font `font` from the string bank |
| Pause menus | `PAUSEMENU slot group` | 8 | slots 1, 2, 3 use groups 6, 8, 11 |
| Also | `TEXTUREFILE`, `SCREENTEXTURE`, `GROUPANIM`, `ANIMINSTRUCTION`, … | | art and transitions |

The `OBJECT` neighbour links mean adding an item needs the rows above and
below re-linked, which is a small text edit. The `up/down` links are also
what mouse hover has to drive.

## String bank (STRS_EN.TXT)

`STRING <id> "<text>"`, ids up to 638. The menus already have a
**Quit** (101), **Quit to Main Menu** (105) and **Quit to Main Menu?**
(158) confirm, so Quit to Desktop can reuse the existing confirm screen with
a new string. Button prompts are inline tokens (`$Q`, `$Z`, `$T`, `$C`,
`$R`, `$W`, `$E`, `$P`) drawn from the icon font, so PC prompts are a swap
of what those tokens draw, not a rewrite of every string.

Strings to reword for PC: the save and memory-unit messages (156 *Xbox hard
disk*, 245, 359, 362, 366, 367, 370, 375, 89–91) and the Gamecube-only
block (500–554, unused on Xbox). Every language file needs the same ids
changed.

## Code

Addresses are guest VAs in the USA XBE; functions are the lifted
`sub_XXXXXXXX` in `src/recomp/gen`.

| Function | What it does | Found by |
|---|---|---|
| `sub_00024F40` | loads fonts and `fend\charmap.txt` | "Unable to open character map" |
| `sub_000250C9` | text draw by font number (0–5) | "Bad font number" |
| `sub_00025B6A` | loads the string bank `STRS_xx.TXT` | "UI_STRINGBANKMAX" |
| `sub_00026960` | opens `FEND\FRONTEND.CSF` | the path string |
| `sub_00026A10` | parses groups, elements, `SPECIAL` nodes | "Error loading FE scripts", "SPECIAL" |
| `sub_00026DB2` | parses later CSF sections | "Error loading FE scripts" |
| `sub_000272E7` | parses `PAUSEMENU` | "PAUSEMENU" |
| `sub_0003D440` | reads `BUTTON` / `NUMBUTTONS` (controller layout) | the keys |
| `sub_00060160` | creates the gamepads | "XBoxCreateGamepads" |
| `sub_00066B70` | `NEWFONT` | the key |
| `sub_0008A3C0` | `START_TEXT`, `BIGFONT`, `SMALLFONT` | the keys |

Still to find: the per-frame menu update (selection, input, the `action`
dispatch), the `SPECIAL` widget dispatch, and where Quit to Main Menu acts.
Those come from following the tables these parsers fill.

## Plan that follows from this

1. **Quit to Desktop:** add an `OBJECT` + `STRING` to the pause groups (6,
   8, 11) and the main menu, reuse the Quit confirm, and hook its action in
   `recomp_manual.c` to close the game cleanly.
2. **PC wording:** rewrite the save and memory-unit strings in every
   `STRS_xx.TXT` at load.
3. **PC Settings pages:** new `MENU` groups built from existing `SPECIAL`
   widgets (list and slider), with a code hook that reads and writes
   `settings.ini`. Same font, colours and transitions as Rocky's Options.
4. **Prompts:** redraw what the `$` tokens map to per device (Xbox,
   PlayStation, Switch, keyboard).
5. **Mouse:** hit-test `OBJECT` rectangles in 640x480 space and move the
   selection along the existing links.
