@echo off
rem ===========================================================================================
rem  Ultima V Native - prepare the game data from YOUR OWN copy of Ultima V.
rem
rem  Double-click it (it asks for your Ultima V folder), drop the Ultima V folder onto it, or run
rem      Prepare-Game-Data.cmd ["<Ultima V folder>"] ["<output folder>"] [options...]
rem  Options are passed to prepare-assets.mjs (--no-audio, --force, --keep-work, --verbose, ...);
rem  --no-pause skips the final "press a key" (for scripts).
rem
rem  The packs go where Ultima V Native looks for them:
rem      <this folder>\data\ultima5               when portable.txt sits beside this file
rem      %APPDATA%\Ultima V Native\ultima5         otherwise
rem  It needs Node.js 18 or newer (LTS recommended): node on PATH, or a Node.js folder unpacked
rem  next to this file as node\node.exe. No administrator rights; nothing is downloaded; the
rem  original game files are only read.
rem ===========================================================================================
setlocal EnableExtensions DisableDelayedExpansion
title Ultima V Native - prepare game data

set "HERE=%~dp0"
set "SCRIPT=%HERE%prepare-assets.mjs"
set "GAME="
set "OUT="
set "EXTRA="
set "PAUSE_AT_END=1"
set "OLDCP="
for /f "tokens=2 delims=:." %%c in ('chcp') do set /a OLDCP=%%c 2>nul
chcp 65001 >nul 2>nul

rem --- arguments: [game folder] [output folder] [options] -----------------------------------
:args
if "%~1"=="" goto args_done
set "ARG=%~1"
if /i "%ARG%"=="--no-pause" (set "PAUSE_AT_END=" & shift & goto args)
if "%ARG:~0,1%"=="-" goto args_option
if not defined GAME (set "GAME=%~1" & shift & goto args)
if not defined OUT (set "OUT=%~1" & shift & goto args)
:args_option
set EXTRA=%EXTRA% %1
shift
goto args
:args_done

echo.
echo  Ultima V Native - prepare the game data
echo  =======================================
echo  This makes the game's resource packs from your own copy of the DOS Ultima V.
echo.

if not exist "%SCRIPT%" (
  echo  ERROR: prepare-assets.mjs is missing. It must sit next to this file:
  echo    "%SCRIPT%"
  echo  Extract the whole ZIP again, keeping all files together.
  set "RC=1"
  goto end
)

rem --- Node.js -------------------------------------------------------------------------------
set "NODE="
if exist "%HERE%node\node.exe" set "NODE=%HERE%node\node.exe"
if not defined NODE for /f "delims=" %%n in ('where node 2^>nul') do if not defined NODE set "NODE=%%n"
if not defined NODE goto no_node
set "NODE_MAJOR=0"
for /f "tokens=1 delims=." %%v in ('call "%NODE%" -p "process.versions.node" 2^>nul') do set /a NODE_MAJOR=%%v 2>nul
if %NODE_MAJOR% LSS 18 goto old_node

rem --- game folder ---------------------------------------------------------------------------
if defined GAME goto have_game
echo  Choose your Ultima V folder: the one that contains TILES.16, DATA.OVL and BRIT.DAT
echo  (for the GOG or Steam release, the folder that holds ULTIMA.EXE).
for /f "usebackq delims=" %%p in (`powershell -NoProfile -ExecutionPolicy Bypass -STA -Command "try { Add-Type -AssemblyName System.Windows.Forms; $owner = New-Object System.Windows.Forms.Form -Property @{ TopMost = $true }; $dialog = New-Object System.Windows.Forms.FolderBrowserDialog; $dialog.Description = 'Select your Ultima V folder (it contains TILES.16, DATA.OVL and BRIT.DAT)'; $dialog.ShowNewFolderButton = $false; if ($dialog.ShowDialog($owner) -eq [System.Windows.Forms.DialogResult]::OK) { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8; [Console]::Out.Write($dialog.SelectedPath) } } catch { }" 2^>nul`) do set "GAME=%%p"
if defined GAME goto have_game
echo.
echo  No folder was chosen in the window. You can type or paste the folder here instead
echo  (or drag the folder into this window), then press Enter. Leave it empty to stop.
set /p "GAME=  Ultima V folder: "
if not defined GAME (
  echo  Stopped: no Ultima V folder given.
  set "RC=2"
  goto end
)
:have_game
rem Drop quotes (a dragged folder brings them) and a trailing backslash: "C:\U5\" would reach node as
rem C:\U5" because \" escapes the quote. A drive root keeps its backslash as "C:\.".
set "GAME=%GAME:"=%"
if "%GAME:~-2%"==":\" (set "GAME=%GAME%.") else if "%GAME:~-1%"=="\" set "GAME=%GAME:~0,-1%"

rem --- output folder (the same rule as the game: portable.txt beside it, else %APPDATA%) ----
if defined OUT goto have_out
if exist "%HERE%portable.txt" goto out_portable
if not defined APPDATA goto out_portable
set "OUT=%APPDATA%\Ultima V Native\ultima5"
goto have_out
:out_portable
set "OUT=%HERE%data\ultima5"
:have_out
set "OUT=%OUT:"=%"
if "%OUT:~-2%"==":\" (set "OUT=%OUT%.") else if "%OUT:~-1%"=="\" set "OUT=%OUT:~0,-1%"

echo.
echo  Ultima V folder:  "%GAME%"
echo  Output folder:    "%OUT%"
echo.

rem --- packs already there: replace them if they differ? ------------------------------------
set "FORCE="
if exist "%OUT%\openu5-assets.bin" goto ask_replace
if exist "%OUT%\openu5-alpha1-resources.bin" goto ask_replace
if exist "%OUT%\openu5-audio.bin" goto ask_replace
goto run
:ask_replace
echo  The output folder already has game data. Identical files are left as they are.
choice /c YN /n /m "  Replace the files that differ with the new ones? [Y/N] "
if errorlevel 2 goto run
set "FORCE=--force"

:run
echo.
"%NODE%" "%SCRIPT%" --game "%GAME%" --out "%OUT%" %FORCE% %EXTRA%
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo  All done. Start Ultima V Native: it will find the game data in the folder above.
  goto end
)
if "%RC%"=="3" echo  Nothing was replaced. Run this again and answer Y to replace the existing files.
if "%RC%"=="2" echo  Check the folder you chose: see the list of missing files above.
echo  The game data was NOT prepared (code %RC%). The messages above say why.
goto end

:no_node
echo  Node.js is needed to prepare the game data, and it was not found.
echo.
echo    1. Download Node.js LTS from  https://nodejs.org/  ("Windows Installer (.msi)")
echo       and install it with the default options. Or, from a terminal:
echo           winget install OpenJS.NodeJS.LTS
echo       Without administrator rights: download the "Windows Binary (.zip)" instead and
echo       unpack it next to this file as a folder named  node  (so that node\node.exe exists).
echo    2. Run this file again.
echo.
echo  Node.js is only needed for this one-time step, not to play.
set "RC=9009"
goto end

:old_node
echo  Your Node.js is too old (major version %NODE_MAJOR%): version 18 or newer is needed.
echo  Install the current LTS from  https://nodejs.org/  and run this file again.
set "RC=9009"
goto end

:end
if defined OLDCP chcp %OLDCP% >nul 2>nul
if defined PAUSE_AT_END (
  echo.
  pause
)
endlocal & exit /b %RC%
