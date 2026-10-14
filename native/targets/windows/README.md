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
  SDL audio backend and synth goldens under MSVC).
- `scenarios/` — the real executable, headless (SDL offscreen video, dummy
  audio), driven by `--autotest` scripts through synthetic SDL key events:
  title, character creation, play, both interface modes, typed and list
  casting (also in an arena), System Menu save/load, quick save/load, the
  Original-mode key rules, the 1988 interaction model (roster pickers,
  console prompts, the arena's turns, the camp) against Enhanced's panels,
  and the PC/DOS save bridge. They need the player's
  packs (`UV_SCENARIO_PACKS`) and are skipped without them; `dungeon_walk`,
  `surface_combat`, `typed_cast_combat`, `town_talk`, `original_arena` and
  `original_camp` also need `UV_ENABLE_DEVELOPER_TOOLS=ON`. The script language is documented in
  `src/app/autotest.h`.

The shared code keeps its own suite (`native/core`, GCC/MinGW, 204 tests,
two of them for Original mode's 1988 interaction model), which must stay
green for every change made for this target. A shared change must also leave
the T-Deck's behaviour unchanged: see `ORIGINAL_MODE_FIDELITY.md` §8 for how
that was checked.

Original mode's interaction rules, with the 1988 evidence for each, are in
[`ORIGINAL_MODE_FIDELITY.md`](ORIGINAL_MODE_FIDELITY.md).

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
                      desktop_settings, data_paths, presenter (SDL window), SDL audio
tests/  scenarios/    see above
packaging/            player README, controls, asset-preparation wrapper
tools/                build-windows.ps1 (build, test, package)
```
