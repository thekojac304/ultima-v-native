# Native saves and DOS Save Transfer

Windows and T-Deck use the shared native save format and `/ultima5/` layout. Windows stores it under its data root; T-Deck uses the SD root. Copy saves only while the game is closed and back up the entire folder first. Three save slots retain a previous generation for recovery. Journey Onward continues the last saved slot; the System Menu selects slots explicitly. Closing Windows does not save.

To import DOS progress, copy both `SAVED.GAM` and `SAVED.OOL` into `ultima5/import`, then select Title → DOS Save Transfer → Import. Export writes both files and `EXPORT.TXT` into `ultima5/export/slotN`. Read that report and back up the original DOS installation before copying exported files into it.

This bridge carries ordinary party, inventory and quest progress. Dungeon saves are refused: leave the dungeon first. Town NPC/chest state may reset or be reseeded; dropped items, parked monsters, carpets and some other transient objects do not survive the bridge. Difficulty and cheats remain part of native journey state, even after switching interface modes.

Automated Windows checks cover import/export. Earlier device transfer testing is recorded in the historical engineering notes. The RC2 user report is general hardware acceptance; no itemized DOS-transfer test is inferred. A complete Native → real DOS play/save → Native round trip is not claimed.

Technical format and edge cases: [Alpha 4 save bridge record](../native/targets/tdeck/ALPHA4_UI.md), section 5.9. [Windows locations](WINDOWS.md), [T-Deck layout](TDECK.md).
