#!/bin/sh
# Builds the C examples into WebAssembly modules with wasi-sdk (https://github.com/WebAssembly/wasi-sdk, version 34):
#   tools/wasm-sdk/examples/c/build.sh
# WASI_SDK names the toolchain's directory; the default is where this tree's download script unpacks it.
set -e
D=$(cd "$(dirname "$0")" && pwd)
SDK=${WASI_SDK:-$HOME/git/tivolicloud/wasi-sdk/wasi-sdk-34.0-arm64-macos}
CC="$SDK/bin/clang"
[ -x "$CC" ] || { echo "no wasi-sdk clang at $CC (set WASI_SDK)" >&2; exit 1; }
for name in orbit hang alert entity assets callbacks; do
    # a reactor: the module has no main, it is initialised once and called into; the attributes in darkride.h
    # name the exports, so no linker flags are needed for them
    "$CC" --target=wasm32-wasip1 -O2 -mexec-model=reactor -Wall -o "$D/$name.wasm" "$D/$name.c" -lm
    echo "$name.wasm: $(wc -c < "$D/$name.wasm") bytes"
done
