# Windows installation — v0.5.0

Use Windows 10/11 x64. The release was validated with MSVC x64 and automated launch/scenario tests on the development Windows system; no minimum CPU, RAM or graphics specification has been benchmarked. No Windows ARM/native 32-bit build is provided.

1. Download [UltimaV-Native-v0.5.0-Windows-x64.zip](https://github.com/thekojac304/ultima-v-native/releases/download/v0.5.0/UltimaV-Native-v0.5.0-Windows-x64.zip) and optionally verify it against [SHA256SUMS.txt](https://github.com/thekojac304/ultima-v-native/releases/download/v0.5.0/SHA256SUMS.txt).
2. Extract everything into an ordinary writable folder, outside Program Files. Keep `SDL3.dll` beside `UltimaVNative.exe`. The C/C++ runtime is linked statically; no Visual C++ redistributable, compiler or separate SDL installation is required.
3. Supply a legally obtained DOS Ultima V installation. Choose the folder containing files such as `ULTIMA.EXE`, `BRIT.DAT`, `TILES.16` and `DATA.OVL`, not a shortcut or a ZIP. The script checks the full required file set and reports omissions.
4. Install a supported Node.js runtime for data preparation (Node 24.18.0 was verified). Double-click `Prepare-Game-Data.cmd`, then choose the game folder when prompted. It builds the resource packs locally using the bundled `prepare-assets.mjs`; no npm install or online game-data download is needed. Let it finish and read any missing-file diagnostics. Node.js is not required for subsequent play. Existing compatible prepared T-Deck packs can also be copied.
5. Run `UltimaVNative.exe`. Use Settings → Interface to choose Original or Enhanced. Both offer the original introduction, character creation and Journey Onward. Ultima IV transfer remains unavailable.
6. Settings → Scaling selects Framed, Whole pixels or Fit window; Display selects size/fullscreen. Alt+Enter or F11 toggles fullscreen; Ctrl+= / Ctrl+- changes window scale. Original play uses the DOS 320×200 picture; Enhanced and menus use 320×240.
7. Configure SFX Volume and Music Volume. Stock DOS assets have no music. Optional music requires compatible Exodus Ultima V Upgrade files when preparing the packs; no songs are bundled.
8. **F10 — System Menu** (also Alt+M): Save Game or Load Game. There are three slots with a previous-generation backup. Journey Onward continues the last saved slot. Original Q asks Y/N to save; Enhanced F5/F9 quick saves/loads. Closing the window does not save.

## Data, settings and portability

Normally the data root is `%APPDATA%\Ultima V Native`. Ctrl+O opens it. It contains:

```text
ultima5/
  openu5-assets.bin
  openu5-alpha1-resources.bin
  openu5-audio.bin           optional
  settings.json
  windows.json
  saves/
  import/
  export/slotN/
  logs/windows.log
screenshots/
```

For portable mode, create an empty `portable.txt` beside the EXE **before preparing data**; the root becomes `data` beside the EXE. Alternatively start with `UltimaVNative.exe --data-dir "D:\Games\U5Data"`; packs go in its `ultima5` subfolder. Back up the data root before copying or replacing anything. One game process may use a given root at a time.

`settings.json` holds shared game options; `windows.json` holds desktop window/interface choices. Removing `windows.json` resets desktop defaults. F12 captures the game's picture into `screenshots`.

## DOS save transfer and controls

Copy `SAVED.GAM` and `SAVED.OOL` into `ultima5/import`, then use Title → DOS Save Transfer → Import. Export writes `ultima5/export/slotN/SAVED.GAM`, `SAVED.OOL` and `EXPORT.TXT`. Back up both DOS and native saves first. [Compatibility limits](SAVE_COMPATIBILITY.md).

Arrow keys move and navigate; Enter accepts; letter keys issue the original commands. Original prompts retain DOS rules, including Y/N questions and typed spell syllables; Esc is not a universal cancel. Enhanced offers panels, spell lists and F2 WASD mode. F7/Ctrl+M toggles music and F8/Ctrl+S toggles sound for the session. [Full keyboard reference](../native/targets/windows/packaging/CONTROLS.txt).

## Troubleshooting

- Missing SDL3.dll: extract the entire ZIP again beside the EXE. Do not download random DLLs.
- Missing/invalid packs: use Ctrl+O or the startup diagnostic to locate the active data root, then regenerate with this release's bundled script. Supply the full DOS installation; music files alone are insufficient.
- Preparation cannot find Node: install the runtime, reopen the command script and retry. The game itself does not require Node.
- No music: expected with stock DOS data; optional patched music assets are required.
- Settings/saves not retained: use a writable data root and check portable.txt or --data-dir; keep a backup before moving folders.
- Black side areas: expected with the preserved picture aspect ratio. Try Settings → Scaling or fullscreen.

The stable payload intentionally reports `0.5.0-rc2`, Git `355d4a1e564c`: the accepted executable was preserved unchanged. Include that identity and `ultima5/logs/windows.log` in a bug report. [Release record](V050_RELEASE_RECORD.md).
