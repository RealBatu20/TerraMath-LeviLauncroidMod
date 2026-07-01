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
Expected: `100% tests passed` (76 checks in the formula-engine suite).

## 2. Levi Launchroid / Gloss SDK
The Android target needs the preloader header `pl/Gloss.h` (for `GlossInit` /
`GlossHook`) and the prebuilt Gloss static library. Both ship in the official
repo — just clone it:
```bash
git clone https://github.com/LiteLDev/preloader-android
```
Then point the build at:
- `PRELOADER_INCLUDE` → `preloader-android/src` (contains `pl/Gloss.h`).
- `PRELOADER_LIB` → `preloader-android/lib/ARM64/libGlossHook.a`.

(The manual byte-signature scan is implemented in-repo — `AutoResolver.cpp` — so
no separate signature library is needed.)

API references:
- Getting started: <https://liteldev.github.io/LeviLaunchroid/guide/getting-started>
- Hook API: <https://liteldev.github.io/LeviLaunchroid/api/hook>
- Signature API: <https://liteldev.github.io/LeviLaunchroid/api/signature>

## 3. Android build (arm64-v8a)
```bash
ANDROID_NDK_HOME=/path/to/android-ndk-r26d \
PRELOADER_INCLUDE=/path/to/preloader-android/src \
PRELOADER_LIB=/path/to/preloader-android/lib/ARM64/libGlossHook.a \
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
3. Pick a preset or type a formula, **Apply**, **Save**. The terrain hook
   auto-installs at launch (adjust in the menu's Advanced section if a build is
   symbol-stripped — see SIGNATURE_ANALYSIS.md).

## 5. CI / releases
`.github/workflows/build.yml`:
- `tests` job: builds + runs host tests on every push/PR (the gate).
- `android` job: clones `preloader-android` (or the `PRELOADER_GIT` repo variable
  if you override it), then builds `libterramath.so` and uploads it as an
  artifact. Runs on every push/PR.
- `release` job: on every `v*` tag, once tests pass, publishes a GitHub Release
  with auto-generated notes. The `libterramath.so` is attached **only if the
  android job built it** (i.e. when `PRELOADER_GIT` is set); otherwise the
  Release is still created without the binary.
- Tagging:
  ```bash
  git tag v1.0.0
  git push origin v1.0.0
  ```
  To get the `.so` attached to releases, set the repo variable once:
  *Settings → Secrets and variables → Actions → Variables →* `PRELOADER_GIT` =
  your Levi Launchroid SDK git URL.

## Notes on shared libraries / ABI
- The mod is an ELF shared object loaded into the Minecraft process; its
  `__attribute__((constructor))` runs at load and installs the hooks.
- Target ABI is **arm64-v8a** only (modern Bedrock). Building other ABIs would
  require matching `libminecraftpe.so` variants and is out of scope.
- Keep the NDK C++ runtime consistent with the loader's expectations; the
  default static libc++ from the NDK toolchain is used.
