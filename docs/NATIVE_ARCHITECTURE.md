# Native architecture

`native/core` contains the portable C++ rules, orchestration, presentation state, save formats and tests. `native/targets/tdeck/main` contains the shared application/runtime, board drawing, resource loading, saves and audio policy. T-Deck builds these with ESP-IDF and device drivers; `native/targets/windows` compiles the same core/application over a small ESP-IDF compatibility layer with SDL3 presentation, keyboard/audio and Windows storage.

Windows Original additionally composes the DOS 320×200 screen. Enhanced and handheld presentation use 320×240. A platform capability profile selects appropriate labels, input and Settings rows. Firmware uses a fixed handheld capability so desktop-only paths can be removed during linking; this is checked in the real compile commands.

Player resource packs are generated locally from commercial game files and loaded at runtime. Windows redirects the shared `/sd` paths to its configured data root. Both targets share save formats and SD-style `/ultima5/` names.

OpenU5's TypeScript browser implementation is retained as a separate reference, with its own lifecycle. Shared lineage and evidence do not make its browser UI or features part of Native. [History](history/PROJECT_HISTORY.md), [Windows design](../WINDOWS_PORT_PLAN.md), [core guide](../native/core/README.md), [contributing](../CONTRIBUTING.md).
