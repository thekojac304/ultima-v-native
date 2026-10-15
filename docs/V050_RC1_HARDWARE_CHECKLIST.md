# v0.5.0 RC1 focused T-Deck acceptance (20–30 minutes)

Status: **NOT RUN — new RC hardware validation pending.** The user reports SPP1 passed on physical hardware; that does not certify SPP2, LOOK-POLISH1, or this RC. Record PASS / FAIL / NOT RUN and a brief note for every row.

## Identity and installation (5 minutes)

Install only `C:\Dev\ultima-v-native-rc1\native\targets\tdeck\dist\UltimaV-Native-v0.5.0-rc1-TDeck-Launcher.bin`. Its SHA-256 and full source commit are in the adjacent `v050-rc1-manifest.json`. The boot identity must show `FW 0.5.0-rc1` and `Git` equal to the first 12 characters of that manifest's `git_commit`, with no `-dirty`. Stop if either differs.

1. Photograph the currently installed FW/Git identity. Back up the complete SD `/ultima5/saves/` directory and `/ultima5/settings.json` to the PC.
2. Keep the known-good SPP1 fallback: `C:\Dev\spp1-hw-package\OpenU5-TDeck-SPP1-5ba2fa46b74f-Launcher.bin` (1,072,912 bytes; SHA-256 `1aeecb70dec68141122e4aa88a9ce140cad87a617ff5668ec3eb8fa1d8b123c2`). It boots with `Git 5ba2fa46b74f` and the historical FW `4.0.0-alpha4-rc5-debug`.
3. Copy the RC BIN to the microSD card. Boot Launcher, choose **SD → that RC BIN**, and install through Launcher, following `native/targets/tdeck/LAUNCHER.md`. It is an app-only ESP image; do not select a bootloader/merged image or use the fixed 1 MiB ota_3 slot.
4. Boot the installed game. Verify FW/Git and normal resource startup. Existing Alpha 4/SPP1 packs remain compatible: resource pack 2,266,819 bytes, CRC32 `5c0d175d`; all three `/ultima5/openu5-*.bin` packs stay on the SD. No proprietary packs are included in the RC downloads.
5. Load a backed-up save or create a separate test party. Do not overwrite the only copy of an existing game.

## Focused play checks (20 minutes)

Developer tools remain available: Alt+D → presets, resources, teleport, party stats and time, as described in the SPP1 handoff. Prefer test saves. Keep a serial log at 115200 baud if already available; it is optional.

| # | Check / steps | Expected result | PASS / FAIL / NOT RUN; notes |
|---|---|---|---|
| 1 | Boot identity and packs, above | `FW 0.5.0-rc1`, manifest Git prefix, matching resources; title and load work | |
| 2 | SPP2: overworld Mix a spell with enough reagents: M, spell, reagents, M, quantity 1, Enter. Note time before; wait through `Mixing...` / `Done!` without keys | Mix pays its world turn immediately at completion (+2 minutes), with no extra key. A subsequent Pass pays just its own turn (+2 more) | |
| 3 | SPP2: arrange a nearby hostile/approaching NPC and perform a timed command such as Mix; do not press another key after completion | NPC approach/encounter updates at completion; no stalled approach waiting for input, no duplicate turn | |
| 4 | SPP2: let the timed turn trigger combat | Combat scene opens and combat music starts immediately; no extra key required and no lingering exploration track | |
| 5 | SPP2: in a town Talk toward an empty adjacent location | `Nobody's here!`; returns to normal play | |
| 6 | SPP2/save: note place, clock, party and inventory; Alt+S, move, Alt+L; also System Menu → Load | Save/load completes, restored values match, normal play resumes; latest/backup selection works | |
| 7 | SPP1 spot-check: overworld R then Mic (cancel); note clock | One world command turn, +2 minutes; no double charge | |
| 8 | SPP1: dungeon L, choose member and direction | Dungeon Look describes the target; no unrelated `No fountain here.`; one dungeon turn | |
| 9 | SPP1: arena J without keys; P on bare floor; Y then ORT | `No Keys!`, `Won't budge!`, `No effect!`; each action ends that combatant's turn; prompts remain responsive | |
| 10 | SPP1: cast Mani then cancel target; use a potion then cancel recipient (record resources first) | Spell/MP and potion are consumed as in the accepted SPP1 behavior | |
| 11 | D-117: Britain, stand at (9,21), directly south of the armoury's hanging sign frame; L north at its supporting post | `Thou dost see the sign of an armoury`, rather than `parched desert`; nearby road still describes cobble | |
| 12 | Shop: Enhanced/handheld blacksmith Buy and Sell, reagent seller, healer companion list | Prices right-aligned as `<n> gp`; owned Sell counts visible; companions have no `0g`; buying/selling works | |
| 13 | Ready/Use: long equipment names, equipped gear, several scrolls | Navigation/select/cancel works; `[eq]` and owned counts stay visible on long rows; no overlap | |
| 14 | Keyboard and trackball: movement, menus, confirm/back; open Settings and change a harmless setting then restore it | Both input paths work; no stuck cursor or dropped menu action; settings persist | |
| 15 | Audio/display/performance: walk surface, enter dungeon and fight, open/close menus, adjust music/SFX and restore | Correct music transitions and SFX; stable display and movement; no hangs, reset, obvious slowdown or corruption | |

If a timed encounter cannot be arranged within the session, mark checks 3–4 NOT RUN and record a reproducible save for follow-up; do not call them PASS from automated evidence. The full historical SPP1 checklist need not be repeated.

## Failure and rollback

For a failure record FW/Git, row number, exact keys, time before/after, location and save; retain a photo/video and serial log if available. Reboot to Launcher and install the SPP1 fallback through **SD → fallback BIN**. Verify `Git 5ba2fa46b74f`. Restore backed-up saves/settings if needed; retain failing RC saves separately. Do not overwrite the fallback or the backups.

After all applicable rows PASS, report the checklist and identity. Public release still requires deliberate approval; this task does not push, publish, merge into main or flash hardware.
