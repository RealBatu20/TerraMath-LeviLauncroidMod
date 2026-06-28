#!/usr/bin/env bash
# TerraMath-LeviLauncroidMod build helper.
#
#   ./build.sh tests              Build + run the host unit tests (no NDK needed)
#   ./build.sh android            Cross-compile libterramath.so (arm64-v8a)
#   ./build.sh clean              Remove build directories
#
# Android build requirements (export before running, or pass as flags):
#   ANDROID_NDK_HOME   path to the Android NDK (r26+ recommended, has clang/C++20)
#   PRELOADER_INCLUDE  dir containing pl/Gloss.h, pl/Signature.h, pl/Logger.h
#   PRELOADER_LIB      (optional) Gloss/preloader link library
#
# Example:
#   ANDROID_NDK_HOME=$HOME/Android/Sdk/ndk/26.3.11579264 \
#   PRELOADER_INCLUDE=$HOME/preloader-android/include \
#   ./build.sh android
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ABI="arm64-v8a"
API="24"

cmd="${1:-tests}"

case "$cmd" in
  tests)
    echo ">> Host unit tests"
    cmake -S "$ROOT" -B "$ROOT/build-host" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$ROOT/build-host" --target terramath_tests -j
    ctest --test-dir "$ROOT/build-host" --output-on-failure
    ;;

  android)
    : "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to your NDK path}"
    : "${PRELOADER_INCLUDE:?Set PRELOADER_INCLUDE to the dir containing pl/Gloss.h}"
    echo ">> Android build ($ABI, API $API)"
    cmake -S "$ROOT" -B "$ROOT/build-android" \
      -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
      -DANDROID_ABI="$ABI" \
      -DANDROID_PLATFORM="android-$API" \
      -DCMAKE_BUILD_TYPE=Release \
      -DPRELOADER_INCLUDE="$PRELOADER_INCLUDE" \
      -DPRELOADER_LIB="${PRELOADER_LIB:-}" \
      -G Ninja
    cmake --build "$ROOT/build-android" -j
    echo ">> Output:"
    find "$ROOT/build-android" -name 'libterramath.so' -print
    ;;

  clean)
    rm -rf "$ROOT/build-host" "$ROOT/build-android"
    echo ">> Cleaned."
    ;;

  *)
    echo "Usage: $0 {tests|android|clean}" >&2
    exit 2
    ;;
esac
