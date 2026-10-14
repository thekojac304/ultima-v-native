# Ultima V Native — Windows target

A Windows x64 desktop build of Ultima V Native using **SDL3**. It runs the
same gameplay core and the same application layer as the T-Deck firmware:
`native/core` and `native/targets/tdeck/main` (AlphaRuntime, the Board
presentation, saves, resources, audio policy) are the **same sources** the
firmware builds -- one implementation, with small behaviour-preserving
changes for the desktop (a platform profile for key names and Settings rows,
a desktop input entry point, the typed spell names and (Q)uit & Save of the
1988 keyboard; listed in the plan, section 5) -- compiled over a Windows
implementation of the small ESP-IDF surface they use. Design,
decisions and evidence: [`WINDOWS_PORT_PLAN.md`](../../../WINDOWS_PORT_PLAN.md).
Player instructions: [`packaging/README-Windows.txt`](packaging/README-Windows.txt).

## Build requirements

- Windows 10/11 x64
- Visual Studio 2022 or 2026 with the "Desktop development with C++" workload
  (MSVC v143+, the Windows SDK, the bundled CMake ≥ 3.24 and Ninja)
- Internet access on the first configure (SDL3 3.4.16 is downloaded from its
  official GitHub release and checked against a pinned SHA-256), or an
  unpacked SDL3 source tree passed as `-DFETCHCONTENT_SOURCE_DIR_SDL3=<dir>`,
  or an installed SDL3 with `-DUV_USE_SYSTEM_SDL3=ON`

Nothing else: no package manager, no Node.js, no game data. (Making the
player's resource packs is a separate, one-time step — see
[`packaging/README-Windows.txt`](packaging/README-Windows.txt).)

## Build

From a "Developer PowerShell / Command Prompt for VS" (or after running
`vcvars64.bat`):

```bat
cmake -S native/targets/windows -B native/targets/windows/build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build native/targets/windows/build-release
```

or with the Visual Studio generator (multi-config: Debug and Release):

```bat
cmake -S native/targets/windows -B native/targets/windows/build-vs -G "Visual Studio 18 2026" -A x64
cmake --build native/targets/windows/build-vs --config Release
cmake --build native/targets/windows/build-vs --config Debug
```

The executable is `UltimaVNative.exe`, with `SDL3.dll` copied beside it. The
C/C++ runtime is linked statically (`/MT`), so no Visual C++ redistributable is
needed. `build*/` folders under this directory are git-ignored.

`tools\build-windows.ps1` (from the repository root) configures, builds,
tests and packages in one go:

```powershell
powershell -ExecutionPolicy Bypass -File native\targets\windows\tools\build-windows.ps1 -Config Release -Test -Package
```

### Options

| CMake option | Default | Meaning |
|---|---|---|
| `UV_BUILD_TESTS` | `ON` | the Windows tests and scenarios |
| `UV_USE_SYSTEM_SDL3` | `OFF` | `find_package(SDL3)` instead of fetching the pinned release |
| `UV_ENABLE_DEVELOPER_TOOLS` | `ON` | the Developer menu (Alt+D), as the T-Deck builds have it; the four scenarios that teleport or start a fight through it are skipped when `OFF` |
| `UV_VERSION_SUFFIX` | `dev` | appended to the version (`0.5.0-dev`) |
| `UV_SCENARIO_PACKS` | `native/assets` | folder with `openu5-*.bin` for the scenarios |
| `UV_SCENARIO_PC_SAVES` | `original/u5/ultima5` | folder with `SAVED.GAM`/`SAVED.OOL` for the PC-save scenario |

## Running from the build tree

```bat
native\targets\windows\build-release\UltimaVNative.exe --data-dir C:\path\to\a\data\folder
```

The data folder must contain `ultima5\openu5-assets.bin` and
`ultima5\openu5-alpha1-resources.bin` (and optionally `openu5-audio.bin`).
Without `--data-dir`, the game uses `%APPDATA%\Ultima V Native` (or
`<exe folder>\data` when a `portable.txt` sits beside the executable).
`UltimaVNative.exe --help` lists the options.

## Tests

```bat
ctest --test-dir native/targets/windows/build-release --output-on-failure
```

- `tests/` — unit and integration tests of the Windows layer (keyboard
  translation in both modes, desktop settings, the `/sd` storage mount and its
  redirect, the virtual panel with the real Board, the save record layout, the
  SDL audio backend and synth goldens under MSVC, the presentation layout and
  window sizing).
- `scenarios/` — the real executable, headless (SDL offscreen video, dummy
  audio), driven by `--autotest` scripts through synthetic SDL key events:
  title, character creation, play, both interface modes, typed and list
  casting (also in an arena), System Menu save/load, quick save/load, the
  Original-mode key rules, the PC/DOS save bridge and the presented window
  itself (`presentation`: read back from the renderer and checked pixel for
  pixel). They need the player's packs (`UV_SCENARIO_PACKS`) and are skipped
  without them; `dungeon_walk`, `surface_combat`, `typed_cast_combat` and
  `town_talk` also need `UV_ENABLE_DEVELOPER_TOOLS=ON`. The script language
  is documented in `src/app/autotest.h`.

## Presentation

The Board draws the T-Deck's 320×240 screen; the presenter
(`src/app/presenter.cpp`) never resamples it except by a whole number, and
places everything itself in whole output pixels
(`src/app/presentation_layout.{h,cpp}`, pure functions, SDL's logical
presentation is off). Settings → Scaling, stored in `windows.json` as
`"scaling"`:

| Row label | `windows.json` | What it does |
|---|---|---|
| Scaling: Framed (default) | `"Framed"` | largest whole scale `u = max(1, min(W÷320, H÷240))`, centred (an odd leftover pixel goes right/bottom), with a ring in the game's chrome where it fits: 4u black, 1u white rule (0xFFFF), 2u band (0x0015) with cut corners; side pillars only when the height has no room (7u), nothing when the width has none |
| Scaling: Whole pixels | `"Integer"` | the same picture, no ring |
| Scaling: Fit window | `"Fit"` | the largest 4:3 rectangle; sampled with `SDL_SCALEMODE_PIXELART` when the scale is fractional (nearest on renderers without it) |

A missing or unknown `"scaling"` is Framed; the first build's `"Integer"` and
`"Fit"` keep their meaning. The ring's colours come from a 2×1 RGB565 texture,
so the band is the in-game band's exact colour on every renderer.

"Window N×" (Settings → Display, Ctrl+= / Ctrl+-) is the picture at scale N:
a framed client of 336N×256N in Framed when that window fits the display's
work area with its real frame, else 320N×240N, else a smaller N. Both
controls step 1× .. the largest that fits; the Display row and
`windows.json` show the scale the window really has. With no `"windowScale"`
in `windows.json` the scale follows the display's scale (3× at 100 %, 4× at
150 %, 6× at 200 %) and is not written until the player picks one. A
maximised window is restored before a new size; the window's aspect is
locked while dragging; fullscreen is borderless at the desktop's mode.
`--autotest`'s `window_screenshot`, `window_size` and `expect window_layout`
see the window as presented; F12 always saves the raw 320×240 panel.

The shared code keeps its own suite (`native/core`, GCC/MinGW, 202 tests),
which must stay green for every change made for this target.

## Layout

```
CMakeLists.txt        the target: uv_core (shared core), uv_platform, uv_shared_app
                      (native/targets/tdeck/main, the firmware's own sources), uv_desktop, the exe
cmake/                loot_names.inc from the tracked JSON
esp_compat/           ESP-IDF API surface for Windows (headers); uv_storage_redirect.h
                      is force-included into the shared device sources
src/platform/         clock/heap/tasks, log, the /sd mount, the virtual ST7789 panel,
                      the SD-log API, input glue
src/app/              application (boot, loop, --autotest), desktop_input (keys ->
                      UiAction), desktop_profile (platform words and rows),
                      desktop_settings, data_paths, presenter (SDL window),
                      presentation_layout (picture, ring and window size maths), SDL audio
tests/  scenarios/    see above
packaging/            player README, controls, asset-preparation wrapper
tools/                build-windows.ps1 (build, test, package)
```
