# Ultima V Native v0.5.0 — Windows & T-Deck

The first native Windows release brings Ultima V Native to Windows x64 alongside LilyGO T-Deck Plus. Both run the same C++ gameplay engine and shared application code, with a playable campaign from character creation through the ending. Bring your own legally obtained DOS Ultima V installation; commercial game data is not included.

## What changes for players

Windows offers two interfaces. Original uses the DOS-style 320×200 screen, original commands and prompts, typed spell syllables, party selectors, equipment lists and shop conversations. Enhanced offers the shared 320×240 presentation with selection panels, spell lists, quick save/load and optional difficulty/cheats. Both support fullscreen, configurable pixel scaling, portable storage and the original Summoning introduction and four-scene View.

Since v0.4.0, shared preservation work corrects behaviors across combat, magic, exploration, dialogue and shops, including the hanging-sign Look description. Enhanced selection/equipment presentation is clearer. DOS Save Transfer now has consistent wording; Settings values/categories and menu shortcut reminders are more readable. Windows uses **F10 — System Menu** (Alt+M also works); T-Deck uses **Alt+M — System Menu**.

T-Deck firmware optimization keeps the shared behavior while fitting the Launcher app image into 1,075,504 bytes, with 235,216 bytes free in the build partition. Launcher allocates 1,114,112 bytes, leaving 38,608 bytes within that allocation.

Campaign play, three native save slots with backup recovery, DOS save transfer, optional music, difficulty/cheats and the T-Deck frontend were already present in v0.4.0. This release carries them forward; [v0.4.0 remains available](https://github.com/thekojac304/ultima-v-native/releases/tag/v0.4.0).

## Downloads and installation

| File | Purpose |
| --- | --- |
| `UltimaV-Native-v0.5.0-Windows-x64.zip` | Windows EXE, SDL3 runtime, game-data preparation script, controls, player instructions and licenses |
| `UltimaV-Native-v0.5.0-TDeck-Launcher.bin` | Bare ESP32-S3 application for the existing T-Deck Launcher SD installer; not a complete flash image |
| `SHA256SUMS.txt` | SHA-256 checksums for both downloads |

**Windows 10/11 x64:** extract the ZIP to a writable folder. Install Node.js for the one-time asset preparation, run `Prepare-Game-Data.cmd` and select your DOS game folder, then run `UltimaVNative.exe`. No compiler, separate SDL installation or Visual C++ runtime installation is required. [Complete Windows guide](https://github.com/thekojac304/ultima-v-native/blob/main/docs/WINDOWS.md).

**T-Deck Plus:** back up your card, prepare the packs on a computer using the Windows ZIP's script or source tools, and put them under `/ultima5/` on a FAT-formatted microSD card. Copy the Launcher BIN to the card, install it using your existing Launcher's SD installer and launch it. No Launcher/bootloader/partition table is bundled. A minimum Launcher version has not been established. [Complete T-Deck guide and rollback](https://github.com/thekojac304/ultima-v-native/blob/main/docs/TDECK.md).

The required packs are `openu5-assets.bin` and `openu5-alpha1-resources.bin`; `openu5-audio.bin` is optional. Stock DOS Ultima V has no music. Compatible community music-patched assets enable optional music; no songs or commercial assets are supplied.

## Saves and compatibility

Native Windows and T-Deck saves share the SD-style layout; back up saves/settings before copying or upgrading. DOS Save Transfer imports `SAVED.GAM` and `SAVED.OOL` from `ultima5/import`, and exports into `ultima5/export/slotN`. Leave dungeons first: dungeon transfer is refused. Some town/transient state, dropped items, parked monsters and carpets are not carried. A full Native → real DOS play/save → Native round trip is not claimed. [Save guide](https://github.com/thekojac304/ultima-v-native/blob/main/docs/SAVE_COMPATIBILITY.md).

## Validation, identity and limitations

The stable release intentionally promotes the accepted RC2 Windows executable and T-Deck firmware **without recompilation**, following the project's accepted-candidate promotion process. Both still report `0.5.0-rc2`, Git `355d4a1e564c`, from full source **`355d4a1e564cef4c6a6d422b47dd0b6d9334cfe9`**. The annotated `v0.5.0` tag identifies that actual build source. Final documentation on main is newer; it is not falsely presented as the binary source. The Windows archive refreshes instructions while preserving executable/runtime/script bytes and BUILD-INFO provenance. No version-only rebuild creates an untested replacement firmware.

The user personally tested this exact final RC2 firmware on the T-Deck and reports everything works correctly: **user-reported hardware PASS**. No serial logs or photographs were provided, and no itemized hardware checklist, heap measurement or stack high-water result is claimed. Previous manual Windows acceptance and the engineering checks are distinct evidence.

RC2 engineering gates passed 213 native host tests, 40 Windows tests, 11 relevant TypeScript tests and seven release-tooling tests. Paced and unpaced comparisons each covered 1,065 walks/8,514 snapshots: only eight expected menu framebuffer changes in two walks, with zero unexpected differences. Eight capability mutants and two release-tooling mutants were killed. Package tests cover missing assets, portable launch, bundled preparation and Original/Enhanced scenarios. Exact final verification is in the [release record](https://github.com/thekojac304/ultima-v-native/blob/main/docs/V050_RELEASE_RECORD.md).

Pre-1.0 limitations remain: documented preservation differences, keyboard-only Windows play, no widescreen redesign or Ultima IV transfer, optional music requirements and DOS transfer edge cases. No numeric minimum CPU/RAM specification is benchmarked. The RC2 SDL source override was local rather than archive-hash-checked at configure time; this qualification is retained.

## Attribution

Ultima V and its artwork belong to their respective rights holders. Ultima is a registered trademark of Electronic Arts. This project is not affiliated with or endorsed by Electronic Arts or Origin Systems. Supply legitimate game files; owning them does not establish redistribution rights. Native code is GPL-3.0; [NOTICE](https://github.com/thekojac304/ultima-v-native/blob/main/NOTICE) preserves OpenU5 lineage and third-party attribution (including SDL3, pngjs and Ultima5Redux metadata). OpenU5 is the separate TypeScript browser/reference implementation, not this Native release.
