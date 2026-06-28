# Build Guide

## Prerequisites
| Build | Needs |
|---|---|
| Host tests | CMake ≥ 3.18, a C++20 compiler (g++/clang++) |
| Android `.so` | Android NDK **r26+** (clang, C++20), CMake, Ninja, the Levi Launchroid / Gloss preloader SDK |

The Android build pulls **ImGui** automatically via CMake `FetchContent`
(pinned `v1.91.5`). It needs network access on first configure.

## 1. Host unit tests (no Android tooling)
```bash
./build.sh tests
# or manually:
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host --target terramath_tests -j
ctest --test-dir build-host --output-on-failure
```
Expected: `100% tests passed` (47 checks in the formula-engine suite).

## 2. Levi Launchroid / Gloss SDK
The Android target needs the preloader headers (`pl/Gloss.h`, `pl/Signature.h`,
`pl/Logger.h`) and the Gloss link library. Obtain them from the official Levi
Launchroid native-mod template / preloader. Then point the build at them:
- `PRELOADER_INCLUDE` → the directory that contains `pl/Gloss.h`.
- `PRELOADER_LIB` → the Gloss/preloader library to link (optional if the SDK is
  header-only in your setup).

API references:
- Getting started: <https://liteldev.github.io/LeviLaunchroid/guide/getting-started>
- Hook API: <https://liteldev.github.io/LeviLaunchroid/api/hook>
- Signature API: <https://liteldev.github.io/LeviLaunchroid/api/signature>

## 3. Android build (arm64-v8a)
```bash
ANDROID_NDK_HOME=/path/to/android-ndk-r26d \
PRELOADER_INCLUDE=/path/to/preloader/include \
PRELOADER_LIB=/path/to/libgloss.a \
./build.sh android
```
Equivalent manual invocation:
```bash
cmake -S . -B build-android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DCMAKE_BUILD_TYPE=Release \
  -DPRELOADER_INCLUDE="$PRELOADER_INCLUDE" \
  -DPRELOADER_LIB="$PRELOADER_LIB"
cmake --build build-android -j
```
Output: `build-android/libterramath.so`.

### Compiler/linker flags (set in CMakeLists.txt)
`-std=c++20 -O2 -fvisibility=hidden -ffunction-sections -fdata-sections`,
linked with `-Wl,--gc-sections` and, for `Release`, `-Wl,--strip-all`. This
keeps the shared object small and strips debug symbols for distribution.

## 4. Install on device
1. Copy `libterramath.so` into the Levi Launchroid mods directory.
2. Launch Minecraft Bedrock; tap the **TerraMath** button to open the menu.
3. (For terrain override) set `terrainSignature` in
   `…/games/com.mojang/terramath/terramath.json` — see SIGNATURE_ANALYSIS.md.

## 5. CI / releases
`.github/workflows/build.yml`:
- `tests` job: builds + runs host tests on every push/PR (the gate).
- `android` job: builds the `.so` and uploads it as an artifact — runs only when
  the repo variable `PRELOADER_GIT` is set to a git URL whose checkout exposes
  `include/pl/Gloss.h` (so the SDK isn't fetched from a guessed location).
- Tagging publishes a GitHub Release with the `.so` attached:
  ```bash
  git tag v1.0.0
  git push origin v1.0.0
  ```

## Notes on shared libraries / ABI
- The mod is an ELF shared object loaded into the Minecraft process; its
  `__attribute__((constructor))` runs at load and installs the hooks.
- Target ABI is **arm64-v8a** only (modern Bedrock). Building other ABIs would
  require matching `libminecraftpe.so` variants and is out of scope.
- Keep the NDK C++ runtime consistent with the loader's expectations; the
  default static libc++ from the NDK toolchain is used.
