#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to Android NDK r27c}"
case "$(uname -s)" in Darwin) host=darwin-x86_64;; Linux) host=linux-x86_64;; *) exit 1;; esac
bin="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$host/bin"
for pair in arm64-v8a:aarch64-linux-android armeabi-v7a:armv7a-linux-androideabi x86_64:x86_64-linux-android x86:i686-linux-android; do
 abi=${pair%%:*}; target=${pair#*:};mkdir -p app/src/main/jniLibs/$abi
 "$bin/${target}26-clang" -O2 -fPIC -shared -Wl,-z,max-page-size=16384 -Iapp/src/main/cpp app/src/main/cpp/android_bridge.c -ldl -o app/src/main/jniLibs/$abi/libfuyi_rime.so
done
