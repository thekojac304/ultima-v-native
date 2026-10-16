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
  3. It writes the packs into the ultima5 folder inside the data folder (see
     "Files and folders" below) and tells you whether they match what this
     build expects.

Option B -- copy them from a T-Deck SD card
  If you already prepared an SD card for Ultima V Native v0.4.0 or later on
  the same version of the pack builder, copy the card's "ultima5" folder into
  the data folder, so that it becomes (or replaces) the ultima5 folder there:
  %APPDATA%\Ultima V Native\ultima5 (Ctrl+O in the game opens the data
  folder). Saves and settings come along and keep working; the T-Deck's
  Brightness setting has no effect on Windows.

Option C -- from a source checkout (developers)
  After "npm install", from the checkout's root folder:
    node --import tsx native/tools/asset-prep/prepare.ts --game "<Ultima V folder>" --out "%APPDATA%\Ultima V Native\ultima5"
  It runs the same five steps as prepare-assets.mjs (extract, the combat
  fixtures, pack:native, pack:alpha1, pack:audio) in a temporary folder. The
  steps can also be run one by one (npm run extract, then
  node --import tsx native/core/tools/generate-world-flow-fixtures.ts, then
  npm run pack:native / pack:alpha1 / pack:audio; see native/core/README.md).

If the packs are missing or were made by a different version of the pack
builder, the game says so when it starts and offers to open the folder the
packs go in.


3. STARTING THE GAME
--------------------
Double-click UltimaVNative.exe. In Original mode you play on the 1988 DOS
game's own 320x200 screen: the map, the party and food/gold boxes and the
16-column message console where the DOS game has them, in its blue-and-white
frame. Enhanced mode (and the title, menus and Settings in both modes) uses
the T-Deck's 320x240 screen design. The window opens with the picture at
three times its size (more on a high-DPI display: 4x at 150 %, 6x at 200 %
Windows scaling, as far as the screen allows), with sharp, square pixels,
inside a thin frame in the game's own blue and white. The window is sized
for the mode's screen (Window 3x is 960x600 for Original's, 960x720 for
Enhanced's, plus the frame); the 320x240 menus show inside it at the largest
whole size that fits.
Alt+Enter (or F11) switches to fullscreen and back; Ctrl+= / Ctrl+- change
the window's size. Once you choose a size it is remembered.

The picture is never stretched out of shape. Settings -> "Scaling"
chooses how it fills a window or the screen:
  Framed        (the default) whole pixels, centred, in a thin blue frame
                where there is room; black around it, as on the 1988 screen
  Whole pixels  the same picture with no frame
  Fit window    fills as much as it can at the picture's own shape (8:5
                for Original's 320x200 screen, 4:3 for 320x240). When that
                is not a whole multiple of it, the edges between pixels are softened
                slightly (every pixel stays the same size); on graphics
                drivers without that, some pixels are one screen pixel
                wider instead
Framed and Whole pixels are never blurred: each game pixel is an exact
square of screen pixels.
Settings -> "Display" chooses a window size (1x up to the largest that fits
your screen) or Fullscreen. If you resize the window by its edges it shows
the size the picture has then; the size remembered for the next start is
the one you last chose there or with Ctrl+= / Ctrl+-.

The title sequence plays; press a key for the menu:
  Journey Onward          continue the journey you saved last
  Create New Character    a new journey (the original's character creation)
  Transfer from Ultima IV  not yet available
  Ultima V Introduction   the Summoning story (Enter advances; Esc returns)
  Acknowledgements        the original credits
  Return to the View      automatic original four-scene introduction
  Settings                sound, music, text size, interface, display
                          (there is no Brightness: use your monitor's)
  DOS Save Transfer       import / export saves of the original DOS game


4. ORIGINAL AND ENHANCED MODES
------------------------------
Settings -> "Interface" switches between the two. It applies at once and is
remembered.

ORIGINAL (the default) is the preservation mode:
  - the 1988 keyboard and prompts: the original letter commands, arrow keys
    to move, Space to pass; a "Player:" question is answered on the party
    list itself (the row in reverse video; a member's number highlights,
    Enter or Space chooses, Esc = "None!"); in typed answers Esc erases the
    text and only Enter answers; Yes/No questions take only Y or N; a
    direction takes an arrow or Space (Pass), not Esc; Enter or Esc at the
    command prompt says "What?". The questions and answers appear in the
    message console as they did in 1988 ("Player: Iolo", "Save game? Yes",
    "Look-North"), with no key bar under them. CONTROLS.txt lists the rules;
  - the 1988 screen (above); the console starts empty, as in 1988 (the
    build's name and version are on the System Menu, F10, under its title);
  - Cast and Mix ask "Spell name:" / "For what spell?" and you TYPE the
    first letter of each syllable, as in 1988 (I L = IN LOR, V F = VAS FLAM;
    the order does not matter; Backspace removes a syllable, Enter accepts,
    Esc cancels);
  - Q (Quit & Save) asks "Save game?" and takes only Y or N, as in 1988;
  - the 1988 rules: the System Menu has no Difficulty or Cheats;
  - no gameplay shortcut keys that the original did not have (the desktop
    system keys -- F10/Alt+M, F7/F8, Alt+Enter/F11, Ctrl+=/-, F12, Ctrl+O --
    work in both modes; see CONTROLS.txt).

ENHANCED adds the project's optional conveniences:
  - the handheld's menus: lists and panels to choose a member or an item,
    a key bar under every question, Esc to cancel, Enter to pass a turn;
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
  Q (Quit & Save; Original mode only, answer Y), F5 quick save (Enhanced
  mode only), or the System Menu (F10) -> Save Game in either mode.
  There are three save slots; each keeps its previous save as a backup, so a
  damaged save falls back to the one before. "Journey Onward" continues the
  slot saved last; System Menu -> Load Game picks a slot.
  Closing the window does NOT save: save first.

DOS saves: put the original game's SAVED.GAM and SAVED.OOL in
%APPDATA%\Ultima V Native\ultima5\import, then Title -> DOS Save Transfer ->
Import. Export writes ultima5\export\slotN\SAVED.GAM / SAVED.OOL (and
EXPORT.TXT) in the data folder, which the original DOS game can load. Your
own files are only read.


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
The data folder is  %APPDATA%\Ultima V Native  (Ctrl+O in the game opens it):
  ultima5\                    the game's files, laid out like the T-Deck's
                              SD card:
      openu5-*.bin            the packs
      saves\                  the save slots
      settings.json           game settings (the same file the T-Deck uses)
      windows.json            window size, scaling, fullscreen and interface
                              mode (delete it to return to the defaults);
                              "originalScreen": "Handheld" gives Original
                              the T-Deck's 320x240 screen instead of DOS's
      import\  export\        DOS save transfer
      logs\windows.log        the log -- attach it to bug reports
  screenshots\                F12 pictures (the game's own picture: 320x200
                              in Original play, 320x240 otherwise)

  PORTABLE: create an empty file named portable.txt beside UltimaVNative.exe
  and the data folder becomes the "data" folder next to the program (the
  game's files then go in data\ultima5).
  Or start the game with:  UltimaVNative.exe --data-dir "D:\my folder"
  (the game's files then go in D:\my folder\ultima5).
  Only one copy of the game can run on one data folder at a time.

Command-line options: UltimaVNative.exe --help  (--mode, --original-screen,
--fullscreen and --windowed apply to that run only).


8. KNOWN LIMITATIONS OF THIS BUILD
----------------------------------
  - Keyboard only: no mouse or game-controller play yet.
  - Original's game screen is the 1988 320x200 layout; Enhanced and the
    menus are the T-Deck's 320x240 design. Both are scaled up by whole
    pixels; there is no higher-resolution or widescreen layout, so a wide
    screen shows black (and the frame) at the sides.
  - Making the packs needs Node.js (or a prepared T-Deck SD card).
  - The rules carry the project's documented, non-blocking differences from
    the 1988 game (see the project's preservation ledger).
  - Development build: see BUILD-INFO.txt for the exact source revision.
