#!/bin/sh
# Builds the JNI library for the macOS desktop app and copies it to <build>/native-resources/macos/.
# usage: build-native-macos.sh <gradle build dir>        env: ARCHS (default "arm64;x86_64"), DEPLOYMENT_TARGET (default 11.0)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
OUT=${1:?build dir}
ARCHS=${ARCHS:-"arm64;x86_64"}

cmake -S "$ROOT" -B "$OUT/native" \
  -DCMAKE_BUILD_TYPE=Release \
  -DAUTODJ_BUILD_JNI=ON -DAUTODJ_BUILD_TESTS=OFF -DAUTODJ_BUILD_BENCH=OFF -DAUTODJ_RT_GUARD=OFF \
  "-DCMAKE_OSX_ARCHITECTURES=$ARCHS" "-DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-11.0}"
cmake --build "$OUT/native" --target autodj_native --parallel 4

mkdir -p "$OUT/native-resources/macos"
LIB=$(find "$OUT/native" -name libautodj_native.dylib | head -n 1)
[ -n "$LIB" ] || { echo "libautodj_native.dylib was not produced" >&2; exit 1; }
cp "$LIB" "$OUT/native-resources/macos/libautodj_native.dylib"
lipo -info "$OUT/native-resources/macos/libautodj_native.dylib" || true
