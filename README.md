# Ultima V Native

**A native C++ implementation of Ultima V: Warriors of Destiny for Windows x64 and LilyGO T-Deck Plus.** Explore Britannia, build a party, fight, cast spells, visit shops and dungeons, and play from character creation through the ending. The portable engine runs directly on each platform, with a platform-specific display, storage, input and audio layer.

## Download v0.5.0

The first native Windows release joins the T-Deck in one [public release](https://github.com/thekojac304/ultima-v-native/releases/tag/v0.5.0).

| Platform | Download | Start here |
| --- | --- | --- |
| Windows 10/11 x64 | [Windows ZIP](https://github.com/thekojac304/ultima-v-native/releases/download/v0.5.0/UltimaV-Native-v0.5.0-Windows-x64.zip) | [Windows installation](docs/WINDOWS.md) |
| LilyGO T-Deck Plus | [Launcher app image](https://github.com/thekojac304/ultima-v-native/releases/download/v0.5.0/UltimaV-Native-v0.5.0-TDeck-Launcher.bin) | [T-Deck installation](docs/TDECK.md) |
| Checksums | [SHA256SUMS.txt](https://github.com/thekojac304/ultima-v-native/releases/download/v0.5.0/SHA256SUMS.txt) | Compare with `Get-FileHash -Algorithm SHA256` |

**Bring your own legally obtained DOS Ultima V files.** Game data and prepared resource packs are not included. Node.js is needed once to prepare your files; it is not needed to run the Windows game afterward. SDL3.dll and license notices are included in the ZIP.

The accepted RC2 executable and firmware are promoted unchanged. They intentionally still report `0.5.0-rc2`, Git `355d4a1e564c`; this is the stable v0.5.0 payload, not a separate candidate download. The Windows ZIP has refreshed instructions. See the [release record](docs/V050_RELEASE_RECORD.md) for exact provenance and hardware acceptance.

## Original and Enhanced

**Original** preserves the DOS game's rules, commands and prompts. On Windows it also uses the DOS-style 320×200 screen, party selectors, equipment panels and shop conversations. Cast and Mix accept typed spell syllables; Q asks to save. Original is the default interface.

**Enhanced** offers selection panels, spell lists, quick save/load, optional difficulty and cheats, and handheld-friendly interaction. On Windows it uses the shared 320×240 presentation. Changing the interface does not reset difficulty or cheats already stored in a journey. Enhanced conveniences are not claims about the original DOS behavior.

Both platforms share exploration, combat, magic, quests, shops, dungeons, three native save slots with backup recovery, and DOS save import/export. Windows adds fullscreen and configurable pixel scaling. Sound effects work without music files; optional music requires compatible community music-patched assets.

## Screenshots

The established repository gallery below contains captures from the Native T-Deck rendering path, at 320×240, using user-supplied data. These are rendered captures, not photographs of a device and not evidence of RC2 hardware testing. No new game imagery is added for this release: the contributing policy excludes game art, and separate redistribution permission for the new Windows evidence captures has not been established. Windows Original/Enhanced and menu captures remain local verification evidence.

| Native title/menu | T-Deck rendering: Britannia |
| --- | --- |
| ![Native title](docs/images/screenshots/title-screen.png) | ![Britannia](docs/images/screenshots/hero-britannia-coast.png) |

Ultima V artwork remains the property of its rights holders. Screenshots do not grant permission to redistribute game assets.

## What is new in v0.5.0

- First Windows x64 release with Original and Enhanced interfaces, DOS presentation and shop interactions, window scaling, fullscreen and portable data storage.
- Shared preservation corrections to combat, magic, dialogue, shops, exploration and the hanging-sign Look description.
- Improved Enhanced selection and equipment interfaces, clearer DOS Save Transfer wording, Settings categories and values, and visible System Menu shortcuts.
- The original Summoning introduction and four-scene View remain available.
- T-Deck firmware optimization retains the shared behavior while fitting a 1,114,112-byte Launcher allocation; the app is 1,075,504 bytes.

Native campaign play, save slots/recovery, DOS transfer, optional music and difficulty settings were already present in [v0.4.0](RELEASE_NOTES_v0.4.0.md). [Full v0.5.0 notes](RELEASE_NOTES_v0.5.0.md) describe the changes and limitations.

## Installation and controls

Windows: extract the ZIP to a writable folder, run `Prepare-Game-Data.cmd` with your DOS installation, then run `UltimaVNative.exe`. **F10 — System Menu**; Alt+M also works. Alt+Enter/F11 toggles fullscreen; Ctrl+= / Ctrl+- changes window size. [Complete guide](docs/WINDOWS.md).

T-Deck: prepare the packs on your computer, copy them under `/ultima5/` on a FAT-formatted microSD card, and install the app BIN through your existing T-Deck Launcher. **Alt+M — System Menu**. The download is an app image, not a complete flash image. [Complete guide and rollback](docs/TDECK.md).

## Documentation and development

| Topic | Documentation |
| --- | --- |
| Player setup | [Windows](docs/WINDOWS.md), [T-Deck](docs/TDECK.md) |
| Controls | [Native controls](docs/NATIVE_CONTROLS.md), [detailed Windows keys](native/targets/windows/packaging/CONTROLS.txt) |
| Saves | [Save compatibility and DOS transfer](docs/SAVE_COMPATIBILITY.md) |
| Building | [Build guide](docs/BUILDING.md), [Windows target](native/targets/windows/README.md), [T-Deck target](native/targets/tdeck/README.md) |
| Architecture | [Native architecture](docs/NATIVE_ARCHITECTURE.md), [core](native/core/README.md) |
| Validation and limitations | [Release record](docs/V050_RELEASE_RECORD.md), [fidelity](docs/FIDELITY.md), [preservation ledger](docs/hardware/ALPHA2_PRESERVATION_LEDGER.md), [deliberate divergences](re/deliberate-divergences.md) |
| Contributing | [Contributing](CONTRIBUTING.md), [roadmap](ROADMAP.md) |
| Licensing | [GPL-3.0](LICENSE), [attribution](NOTICE) |

Bug reports should identify platform, build/Git identity, interface, difficulty/cheats, reproduction steps and any useful logs. Never post proprietary game data or private saves publicly. Gameplay changes require an assembly citation or parity evidence.

## Preservation, lineage and AI-assisted development

Ultima V Native is a native reimplementation, not an emulator. It relied heavily on the separate **OpenU5 TypeScript/browser implementation** as a behavioral reference, alongside direct reverse engineering of the DOS executable, native tests and hardware results. It is not an independent clean-room reconstruction. When the sources disagree, the original DOS executable decides. See [project history](docs/history/PROJECT_HISTORY.md).

AI coding agents wrote substantial parts of the project. Confidence comes from review, parity/golden tests, mutation testing, build checks and physical testing, not from who wrote the code. v0.5.0 RC2 passed 213 native host tests and 40 Windows tests; the user reports the exact RC2 firmware works correctly on their T-Deck. This is not formal verification or perfect DOS parity.

OpenU5 remains in `game/`, `demo-byo/` and `extractor/`, with its own [browser roadmap](docs/browser/OPENU5_BROWSER_ROADMAP.md). Its browser demo, features and release lifecycle do not describe Native. Historical `openu5` namespaces, filenames and SD pack names remain for compatibility; see the [internal rename plan](docs/INTERNAL_RENAME_PLAN.md).

## Known limitations

- Pre-1.0 software; documented preservation discrepancies remain.
- Windows play is keyboard-based; no mouse/controller gameplay or widescreen redesign.
- Ultima IV character transfer is unavailable.
- DOS save transfer refuses dungeon saves and omits some transient objects/state; a complete Native → DOS → Native session in a real DOS installation is not claimed.
- Music needs compatible optional patched assets; stock DOS files have no music.
- RC2 hardware acceptance is a user report. No serial logs, device photographs, measured free heap or stack high-water figures were supplied for that acceptance.

## Credits and legal notice

Credit to OpenU5 and its reverse-engineering work, Origin Systems and the original creators, [Ultima5Redux](https://github.com/bradhannah/Ultima5Redux) for MIT-licensed tile metadata, SDL3, pngjs, LilyGO, bmorcelli's Launcher and the community Exodus Ultima V Upgrade music patch (not included). Details: [NOTICE](NOTICE).

Ultima V and its artwork belong to their respective rights holders. Ultima is a registered trademark of Electronic Arts. This project is not affiliated with or endorsed by Electronic Arts or Origin Systems. Commercial game data, art, text, maps and music packs are not distributed; supply your own legally obtained files. Owning a copy does not itself grant redistribution rights. Project code is GPL-3.0; third-party terms remain applicable.
