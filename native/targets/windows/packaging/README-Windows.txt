ULTIMA V NATIVE for Windows -- development build
=================================================

Ultima V Native is a native C++ re-implementation of "Ultima V: Warriors of
Destiny" (Origin Systems, 1988). It is not an emulator: the game's rules were
re-derived from the original DOS program and checked against it. The same
engine runs on the LilyGO T-Deck handheld; this is its Windows x64 build.

THIS IS A DEVELOPMENT BUILD (version in BUILD-INFO.txt). It is playable from
character creation to the ending, but it has not yet been through a full
player-testing pass on Windows. Please report problems with the log file
(see "Files and folders" below).

NO GAME DATA IS INCLUDED. You need your own copy of Ultima V (for example the
GOG.com release). "Ultima" is a registered trademark of Electronic Arts; this
project is not affiliated with or endorsed by Electronic Arts or Origin.


1. WHAT IS IN THE ZIP
---------------------
  UltimaVNative.exe        the game
  SDL3.dll                 window, keyboard and sound library (zlib licence)
  Prepare-Game-Data.cmd    makes the game's data files from YOUR Ultima V copy
  prepare-assets.mjs       the data preparation script it runs (needs Node.js)
  README-Windows.txt       this file
  CONTROLS.txt             the keyboard reference
  BUILD-INFO.txt           version, source commit and file checksums
  LICENSE.txt, NOTICE.txt, THIRD-PARTY-LICENSES.txt

No installation is needed and nothing else has to be installed to RUN the
game: unpack the ZIP anywhere (not into "Program Files"). Visual Studio, SDL
and the Visual C++ runtime are NOT required.


2. PREPARING THE GAME DATA (once)
---------------------------------
The game plays from three data files ("packs") made from your Ultima V
files on your own computer:

  openu5-assets.bin            tiles, palette, the map of Britannia
  openu5-alpha1-resources.bin  everything else: towns, dungeons, people,
                               conversations, shops, pictures, the ending
  openu5-audio.bin             optional: music (see "Music" below)

Option A -- make them with Prepare-Game-Data.cmd (recommended)
  1. Install Node.js (the free "LTS" version from https://nodejs.org).
     Only the runtime is needed; there is nothing to "npm install".
  2. Double-click Prepare-Game-Data.cmd. When asked, choose the folder that
     holds your Ultima V files (the one with ULTIMA.EXE, BRIT.DAT, TILES.16,
     DATA.OVL, ...). For the GOG release that is the game's install folder.
  3. It writes the packs into the game's data folder (see below) and tells
     you whether they match what this build expects.

Option B -- copy them from a T-Deck SD card
  If you already prepared an SD card for Ultima V Native v0.4.0 or later on
  the same version of the pack builder, copy the card's "ultima5" folder into
  the data folder (saves and settings come along and keep working).

Option C -- from a source checkout (developers)
  npm install, then run the steps in native/tools/asset-prep (or the
  individual npm run extract / pack:native / pack:alpha1 / pack:audio steps).

If the packs are missing or were made by a different version of the pack
builder, the game says so when it starts and offers to open the data folder.


3. STARTING THE GAME
--------------------
Double-click UltimaVNative.exe. The window opens at three times the
original 320x240 picture, with sharp, square pixels. Alt+Enter (or F11)
switches to fullscreen and back.

The title sequence plays; press a key for the menu:
  Journey Onward          continue the journey you saved last
  Create New Character    a new journey (the original's character creation)
  Transfer from Ultima IV, Ultima V Introduction, Acknowledgements
  Return to the View      back to the title sequence
  Settings                sound, music, text size, interface, display
  PC Save Transfer        import / export saves of the original DOS game


4. ORIGINAL AND ENHANCED MODES
------------------------------
Settings -> "Interface" switches between the two. It applies at once and is
remembered.

ORIGINAL (the default) is the preservation mode:
  - the 1988 keyboard: the original letter commands, arrow keys to move,
    a member's number in "Player:" prompts, Esc to cancel, Space to pass;
  - Cast and Mix ask "Spell name:" / "For what spell?" and you TYPE the
    first letter of each syllable, as in 1988 (I L = IN LOR, V F = VAS FLAM;
    the order does not matter; Backspace removes a syllable, Enter accepts,
    Esc cancels);
  - the 1988 rules: the System Menu has no Difficulty or Cheats;
  - no shortcut keys that the original did not have.

ENHANCED adds the project's optional conveniences:
  - Difficulty presets (Original, Relaxed, Easy, Custom) and Cheats in the
    System Menu -- a journey that uses them keeps that in its save;
  - choosing spells from a list for Cast and Mix;
  - F5 quick save and F9 quick load (the journey's own slot);
  - F2 WASD movement mode (W/A/S/D move; Settings -> "Movement default");
  - the Developer menu (Alt+D).

A journey's own rules (difficulty, cheats) are kept with its save whatever the
interface mode: switching to Original never rewrites them.


5. SAVING
---------
  Q (Quit & Save) in the game, or the System Menu (F10) -> Save Game.
  There are three save slots; each keeps its previous save as a backup, so a
  damaged save falls back to the one before. "Journey Onward" continues the
  slot saved last; System Menu -> Load Game picks a slot.
  Closing the window does NOT save: save first.

PC/DOS saves: put the original game's SAVED.GAM and SAVED.OOL in the data
folder's ultima5\import folder, then Title -> PC Save Transfer -> Import.
Export writes ultima5\export\slotN\SAVED.GAM / SAVED.OOL (and EXPORT.TXT),
which the original DOS game can load. Your own files are only read.


6. MUSIC AND SOUND
------------------
Sound effects are the original's PC-speaker effects. The original DOS game
has NO music; Ultima V Native plays music only if your copy has the community
music patch ("Ultima V Upgrade") installed when the packs are made -- it then
builds openu5-audio.bin with the patch's songs. Without it, the Music Volume
setting explains that there is no music. Nothing is invented or substituted.

  Settings -> SFX Volume / Music Volume.  F8 / Ctrl+S: sound off and on;
  F7 / Ctrl+M: music off and on (for this session).


7. FILES AND FOLDERS
--------------------
The data folder holds the packs, your saves and settings:
  %APPDATA%\Ultima V Native\ultima5\
      openu5-*.bin            the packs
      saves\                  the save slots
      settings.json           game settings (the same file the T-Deck uses)
      windows.json            window, scaling and interface mode
      import\  export\        PC/DOS save transfer
      logs\windows.log        the log -- attach it to bug reports

  Ctrl+O in the game opens this folder.
  PORTABLE: create an empty file named portable.txt beside UltimaVNative.exe
  and the data folder becomes the "data" folder next to the program.
  Or start the game with:  UltimaVNative.exe --data-dir "D:\my folder"

Command-line options: UltimaVNative.exe --help


8. KNOWN LIMITATIONS OF THIS BUILD
----------------------------------
  - Keyboard only: no mouse or game-controller play yet.
  - The picture is the original 320x240 layout (the T-Deck's screen design),
    scaled up; there is no higher-resolution mode.
  - Making the packs needs Node.js (or a prepared T-Deck SD card).
  - The rules carry the project's documented, non-blocking differences from
    the 1988 game (see the project's preservation ledger).
  - Development build: see BUILD-INFO.txt for the exact source revision.
