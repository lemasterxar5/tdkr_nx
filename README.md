# tdkr_nx — The Dark Knight Rises (Gameloft) for Switch

A 32-bit (AArch32) wrapper on the **android32** runtime that runs the Android
build of **The Dark Knight Rises** on Nintendo Switch. No game code or data
included: it uses your APK and your data files.

## Required version

This build targets and was tested against:

* **The Dark Knight Rises 1.1.6** (`com.gameloft.android.AMAZ.GloftKRAS`,
  versionCode 116, `lib/armeabi-v7a/libKRAS.so`). The APK is recognised by
  its contents, not its file name.
* The extracted data files (`data/`, `textures/`, `welcome/`… ≈ 1.9 GB).
  OBBs are also accepted (`datakrhm.obb`, optional `patchkrhm.obb`, or
  `main.<n>.<package>.obb` / `patch.<n>.<package>.obb`).

Other versions are untested.

## Folder on the SD card

```
sdmc:/switch/tdkr_nx/
├── tdkr_nx.nro          ← the launcher (Batman forwarder icon)
├── <any name>.apk
├── obb/                 ← only if you have OBBs (skip otherwise)
└── data/files/          ← extracted data
    ├── data/ textures/ welcome/ ...
```

`config.ini`, `debug.log`, `libKRAS.so` (unpacked) and `classes.txt` are
generated on first launch. If you rename the folder, change `PORT_NAME` in
`source/port_config.h`.

Copy `tdkr_nx.nro` to `sdmc:/switch/tdkr_nx/`, install the forwarder from
sphaira (Homebrew > The Dark Knight Rises > Install Forwarder) and launch
the icon.

## Controls (Joy-Cons)

The game is touch-driven (left virtual joystick, right-side buttons, swipe
to look). The Joy-Cons emulate it with virtual fingers shared with the
touchscreen (ids 0-15), plus the engine's native gamepad mode (Xperia with
open slide + announced MOGA: keys through its tables, right stick into its
own variables).

| Joy-Con | Action |
| --- | --- |
| Left stick or D-pad | drags the virtual joystick (`touch_joy_*`, 170/540 r90) plus native 8-way arrows |
| Right stick | engine-native camera (`nativeSetPowerARightJoystick`, Y flippable with `gamepad_invert_y`); swipe when `gamepad_native = false` |
| A / L | jump (`touch_btn_jump_*` 1030/610); A also confirms menus (key 23) |
| B / R / right click | attack (`touch_btn_attack_*` 1150/550) |
| X / ZR | counter (`touch_btn_counter_*` 1150/420) + SHIELD key |
| Y / ZL / left click | interact / grapnel (`touch_btn_use_*` 1030/480) + SHIELD key |
| D-pad | menu arrows and 8-way movement; `+` = Menu, `-` = Back |
| `Minus+R3` | hot-swap native (gameplay) / raw (tech shop, menus) |
| Touchscreen | always live alongside (driving slider, hacks, doors) |

Positions are 1280x720 pixels in `[controls]` of `config.ini`. The game's
buttons move with the context: if a tap misses, tune its coordinate.
`gamepad_touch = false` leaves touch screen + keys only.

## Settings (`config.ini`)

* `[display] resolution`: 720, 1080 or auto. `frame_rate`: 60 or 30 (30
  runs cooler in handheld). `show_fps`: an `[fps]` line every 2 s (default on).
* `[performance] boost_cpu_when_loading`: `false` (stock clocks: the 1785
  MHz load boost is off; GPU/RAM always stock).
* `[controls]`: everything in the table above.
* `[debug] log_java_calls`: every Java call (slow; for bug reports only).

## Port files (`source/`)

| file | what it does |
| --- | --- |
| `port_config.h` | name, package, APK role, region size, I/O readahead/buffering |
| `tdkr_main.c` | setup, `libKRAS.so` loading, `init`/`nativeInit`/`setPaths` order, `step`+swap loop, FPS |
| `tdkr_java.c` | the "Java" the engine calls + native gamepad announce |
| `tdkr_gl.c` | the EGL/GLSurfaceView Java would have made |
| `tdkr_audio.c` | `AudioTrack` over audout |
| `tdkr_input.c` + `touchmap.h/.c` | touchscreen + reusable gamepad→touch/keys mapper |
| `tdkr_obb.c` | OBB lookup by role + `obb/`→`data/files/` fallback without OBBs |
| `tdkr_libc.c` | `rewind`, `wcscmp`, `wcscpy` |
| `tdkr_config.c` | `config.ini` options (migrations included) |
| `imports.c` | generated: `python3 runtime/tools/gen_imports.py` |

## Status

* Menu, open world (Gotham), audio, saves and touch input working; 40-56
  fps in gameplay once loaded. The menu streams ~200 MB over SD at stock
  clocks and takes minutes: let it finish.
* No online services (Gameloft Live, shop, IAB answer empty).
* Bug reports: send `debug.log` (plus `crash.log` on a crash). For input
  reports keep `log_java_calls = true` and note which button fails and where
  it shows on screen.

## Build

```sh
./build.sh              # tdkr_nx.nsp + tdkr_nx.build  (AArch32 toolchain, Docker)
./launcher/build.sh     # tdkr_nx.nro                  (devkitA64)
```

Requires Docker (image `ghcr.io/vita2hos/devcontainer/vita2hos`), libnx32
(`DCR_LIBNX32` or `../libnx32/prefix`, see `runtime/tools/docker_build.sh`)
and mesa32 (`portlibs32/`). Clean rebuild: `./build.sh clean`.

## Credits

* The Dark Knight Rises: Gameloft (the game; not included).
* android32 runtime, libnx32, toolchain: aks796, vita2hos, devkitPro.
* Mesa 3D, miniz: their authors (see their licenses).
* Original `.so` loader work: Andy Nguyen (TheOfficialFloW), fgsfds.
* Input/mapping patterns studied from the Ducktales-NX, Sonic All-Stars
  Racing, TASM2, Modern Combat 3 and Asphalt 8 ports (Thorhax, aks796,
  Rinnegatamante).

## Disclaimer

Unofficial fan project, not affiliated with Nintendo or Gameloft. No game
code or data included: you need your own APK and data files.
