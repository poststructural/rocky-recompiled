# settings.ini

`settings.ini` sits beside `Rocky.exe` (or in `%LOCALAPPDATA%\RockyRecompiled`
when that folder is not writable). It is written with the defaults and a
comment per key on first start; the launcher's **PC Options** page and the
in-game PC OPTIONS screen (F1) change it for you. Delete it to get the
defaults back.

## [display]

| Key | Values | Default |
|---|---|---|
| `mode` | `windowed`, `fullscreen` (borderless, the monitor's resolution), `exclusive` | `windowed` |
| `resolution` | window size, e.g. `1920x1080`, or `native` | `1920x1080` |
| `aspect` | `auto` (the window's shape), `4:3`, `16:9`, `16:10`, `21:9`, `32:9` | `auto` |
| `vsync` | `on`, `off`, `adaptive` | `on` |
| `show_fps` | `on` / `off` (F10 in game) | `off` |

Fights use the game's own 16:9 mode at any shape wider than 4:3 (more of
the ring at the sides); menus and movies stay 4:3 with side bars.

## [graphics]

| Key | Values | Default |
|---|---|---|
| `render_scale` | `auto` or 1-8 (times the console's 640x480) | `auto` |
| `max_render_scale` | the limit for `auto` | `6` |
| `texture_filter` | `original` (as the Xbox drew it), `smooth`, or 2/4/8/16 (anisotropic) | `smooth` |

## [game]

| Key | Values | Default |
|---|---|---|
| `skip_intro` | `on` skips the Ubi Soft and MGM logos and the intro movie | `off` |

## [launcher]

| Key | Values | Default |
|---|---|---|
| `skip_launcher` | `on` starts the game straight away | `off` |
| `update_check` | `on` looks for a new release when the launcher opens | `on` |

## Command line

| | |
|---|---|
| `Rocky.exe --play` | no launcher |
| `Rocky.exe --launcher` | the launcher even with `skip_launcher = on` |
| `Rocky.exe --install <image>` | install the game files from a disc image, then play |
| `Rocky.exe --update` | install a newer release from GitLab, then quit |
| `Rocky.exe <folder>` | use the game files in that folder |
