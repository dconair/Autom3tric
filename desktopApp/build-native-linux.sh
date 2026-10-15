#!/bin/sh
# Builds the JNI library without an audio device (null driver) for the Linux test run and copies it to
# <build>/native-resources/linux/. Only used by `gradle :desktopApp:test` on Linux; the shipped app is the macOS one.
# usage: build-native-linux.sh <gradle build dir>
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
OUT=${1:?build dir}

cmake -S "$ROOT" -B "$OUT/native" \
  -DCMAKE_BUILD_TYPE=Release \
  -DAUTODJ_BUILD_JNI=ON -DAUTODJ_BUILD_TESTS=OFF -DAUTODJ_BUILD_BENCH=OFF -DAUTODJ_RT_GUARD=OFF
cmake --build "$OUT/native" --target autodj_native --parallel 4

mkdir -p "$OUT/native-resources/linux"
LIB=$(find "$OUT/native" -name libautodj_native.so | head -n 1)
[ -n "$LIB" ] || { echo "libautodj_native.so was not produced" >&2; exit 1; }
cp "$LIB" "$OUT/native-resources/linux/libautodj_native.so"
