# Building Ultima V Native

Clone the repository and use tag `v0.5.0` for the exact executable/firmware source. The tag intentionally retains `0.5.0-rc2` build labels; final player documentation lives on `main`. Do not relabel rebuilt binaries as the hardware-accepted image: a rebuild has a new identity and needs its own validation.

Windows requires Windows 10/11 x64, Visual Studio with Desktop development with C++, Windows SDK, CMake ≥3.24 and Ninja (or a supported Visual Studio generator). From a developer shell:

```powershell
cmake -S native/targets/windows -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DUV_VERSION_SUFFIX=rc2
cmake --build build/windows
ctest --test-dir build/windows --output-on-failure
```

SDL3 3.4.16 is pinned by the target. The accepted RC2 was built with a local SDL source override; its path was not archive-hash-checked at configure time. This provenance qualification remains recorded. The [Windows target guide](../native/targets/windows/README.md) documents overrides, static runtime, scenario data and packaging.

For T-Deck, activate ESP-IDF 6.1 with the ESP32-S3 toolchain, then from `native/targets/tdeck`:

```text
idf.py --no-ccache -B build-release build
python package_launcher.py --build-dir build-release
```

The packager verifies the app header, segment checksum, appended SHA-256 and size budget. Use Launcher to install its bare app output. [Target details](../native/targets/tdeck/README.md), [Launcher](../native/targets/tdeck/LAUNCHER.md).

The independent native host suite builds from `native/core`; consult its [README](../native/core/README.md) for your host compiler and locally generated fixtures. Full RC2 verification used 213 host and 40 Windows tests, developer-enabled scenarios, private data and generated fixtures. A clean clone may omit fixture-dependent cases. Never commit generated game packs, proprietary fixtures, private saves or build outputs. Asset preparation requires Node.js and the user's own game files; see the [player guide](WINDOWS.md).

Architecture: [Native layers](NATIVE_ARCHITECTURE.md). Contribution/provenance rules: [CONTRIBUTING](../CONTRIBUTING.md).
