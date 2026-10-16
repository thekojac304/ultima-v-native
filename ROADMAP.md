# Ultima V Native roadmap

This roadmap covers the C++ Native implementation. The separate [OpenU5 browser roadmap](docs/browser/OPENU5_BROWSER_ROADMAP.md) retains its own scope. No dates are promised.

## Delivered in v0.5.0

- Windows x64 joins LilyGO T-Deck Plus in one release.
- Shared native engine/application, Windows Original DOS screen and interaction, Enhanced conveniences, fullscreen/scaling and asset preparation.
- Shared preservation corrections, Enhanced selection improvements, final Settings/menu polish and firmware-size optimization.
- User-reported RC2 T-Deck hardware PASS; accepted payloads promoted without recompilation. [Release record](docs/V050_RELEASE_RECORD.md).

## Preservation and compatibility

Continue adjudicating documented discrepancies against the original DOS executable. The historical preservation ledgers remain the evidence trail; their old open-row numbers are not a current list of unresolved defects. Review current [fidelity records](docs/FIDELITY.md), [deliberate divergences](re/deliberate-divergences.md) and [release limitations](RELEASE_NOTES_v0.5.0.md).

Ultima IV character transfer remains unavailable. A complete real-DOS save round trip and broader edge-case/hardware coverage remain useful future validation. Preserve Original behavior; keep rules-affecting conveniences optional.

## Future work

- Compatibility-aware cleanup of historical `openu5` identifiers: [rename plan](docs/INTERNAL_RENAME_PLAN.md).
- Further native portability may be explored; no platform commitments.
- Continue clear player documentation and reproducible evidence without distributing commercial game data.
