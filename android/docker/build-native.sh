#!/bin/bash
# Configures and builds Darkride's native Android library (arm64-v8a) inside the darkride-android image.
#
#   docker run --rm -v darkride-android-work:/work -v <repo>:/src darkride-android:dev /src/android/docker/build-native.sh [target...]
#
# /work keeps vcpkg (its Android dependencies take a while the first time) and the build folder between runs.
set -euo pipefail

SRC=${SRC:-/src}
BUILD=${BUILD:-/work/build-android}
export TIVOLI_VCPKG_BASE=${TIVOLI_VCPKG_BASE:-/work/vcpkg}
# vcpkg's binary cache, kept on the volume (the container's home folder goes with --rm)
export VCPKG_DEFAULT_BINARY_CACHE=${VCPKG_DEFAULT_BINARY_CACHE:-/work/vcpkg-archives}
mkdir -p "$VCPKG_DEFAULT_BINARY_CACHE"
API_LEVEL=${API_LEVEL:-26}

cmake -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-$API_LEVEL \
    -DANDROID_STL=c++_shared \
    -DANDROID_TOOLCHAIN=clang \
    -DANDROID_NDK="$ANDROID_NDK_HOME" \
    -DHIFI_ANDROID=1 \
    -DHIFI_ANDROID_APP=interface \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# KEEP_GOING=1 builds everything it can past errors, to see them all at once
EXTRA=()
if [ "${KEEP_GOING:-0}" = "1" ]; then
    EXTRA=(-- -k 0)
fi
if [ "$#" -gt 0 ]; then
    cmake --build "$BUILD" --target "$@" "${EXTRA[@]}"
else
    cmake --build "$BUILD" --target native-lib pcm-codec-plugin "${EXTRA[@]}"
fi
