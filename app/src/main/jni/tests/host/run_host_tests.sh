#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Builds and runs the native dictionary tests on the development machine (not on Android) with
# GoogleTest, AddressSanitizer and UndefinedBehaviorSanitizer. Everything stays local.
#
# Needs: clang++, GoogleTest (e.g. `brew install googletest`), a JDK for jni.h.
# Usage:  app/src/main/jni/tests/host/run_host_tests.sh [--jni-dir DIR] [--out DIR] [gtest args...]
#   --jni-dir DIR  native source tree to test (default: this checkout). Use it to run the same tests
#                  against another version of the code, e.g. an export of the code before a fix.
#   --out DIR      build folder (default: app/build/native-host-tests/current or .../other)
#   --all          also run the known upstream failures listed in known_upstream_failures.txt
# Environment: GTEST_PREFIX (default: brew --prefix googletest), JAVA_HOME, JOBS.
set -euo pipefail

HOST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESTS_DIR="$(dirname "$HOST_DIR")"
THIS_JNI_DIR="$(dirname "$TESTS_DIR")"
APP_DIR="$(cd "$THIS_JNI_DIR/../../.." && pwd)"

JNI_DIR="$THIS_JNI_DIR"
OUT_DIR=""
RUN_ALL=0
while [ $# -gt 0 ]; do
    case "$1" in
        --jni-dir) JNI_DIR="$(cd "$2" && pwd)"; shift 2 ;;
        --out) OUT_DIR="$2"; shift 2 ;;
        --all) RUN_ALL=1; shift ;;
        *) break ;;
    esac
done
[ -n "$OUT_DIR" ] || OUT_DIR="$APP_DIR/build/native-host-tests/$( [ "$JNI_DIR" = "$THIS_JNI_DIR" ] && echo current || echo other )"
mkdir -p "$OUT_DIR/obj"

GTEST_PREFIX="${GTEST_PREFIX:-$(brew --prefix googletest 2>/dev/null || echo /usr/local)}"
[ -f "$GTEST_PREFIX/include/gtest/gtest.h" ] || { echo "GoogleTest not found in $GTEST_PREFIX" >&2; exit 2; }
if [ -z "${JAVA_HOME:-}" ]; then JAVA_HOME="$(/usr/libexec/java_home 2>/dev/null || true)"; fi
[ -f "$JAVA_HOME/include/jni.h" ] || { echo "jni.h not found, set JAVA_HOME to a JDK" >&2; exit 2; }
JNI_PLATFORM_INCLUDE="$JAVA_HOME/include/$(uname -s | tr '[:upper:]' '[:lower:]')"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc)}"

SANITIZE=(-fsanitize=address,undefined -fno-omit-frame-pointer)
# UBSan stays recoverable so the suppression file works; halt_on_error=1 below still stops at the first report.
CXXFLAGS=(-std=c++17 -O1 -g -w "${SANITIZE[@]}"
    -I "$JNI_DIR/src" -I "$JNI_DIR" -I "$GTEST_PREFIX/include" -I "$JAVA_HOME/include" -I "$JNI_PLATFORM_INCLUDE")

# Test files: the AOSP native unit test list from NativeFileList.mk of this checkout, plus the host tests.
TEST_FILES=()
while read -r f; do TEST_FILES+=("$TESTS_DIR/$f"); done < <(
    sed -n '/^LATIN_IME_CORE_TEST_FILES/,/^$/p' "$THIS_JNI_DIR/NativeFileList.mk" | grep -o '[A-Za-z0-9_/]*\.cpp')
TEST_FILES+=("$HOST_DIR/dictionary_files_test.cpp")

# Compile the native library sources and tests to objects (in parallel), then link with GoogleTest.
compile_list="$OUT_DIR/sources.txt"
{ find "$JNI_DIR/src" -name '*.cpp'; printf '%s\n' "${TEST_FILES[@]}"; } > "$compile_list"
export OUT_DIR
printf '%s\n' "${CXXFLAGS[@]}" > "$OUT_DIR/cxxflags.txt"
echo "Compiling $(wc -l < "$compile_list" | tr -d ' ') files from $JNI_DIR"
xargs -P "$JOBS" -I{} bash -c '
    src="$1"; obj="$OUT_DIR/obj/$(echo "$src" | shasum | cut -c1-16)_$(basename "$src" .cpp).o"
    flags=(); while IFS= read -r l; do flags+=("$l"); done < "$OUT_DIR/cxxflags.txt"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ "$OUT_DIR/cxxflags.txt" -nt "$obj" ]; then
        clang++ "${flags[@]}" -c "$src" -o "$obj" || { echo "FAILED: $src" >&2; exit 255; }
    fi' _ {} < "$compile_list"
objects=()
while read -r src; do objects+=("$OUT_DIR/obj/$(echo "$src" | shasum | cut -c1-16)_$(basename "$src" .cpp).o"); done < "$compile_list"
clang++ "${SANITIZE[@]}" "${objects[@]}" -L "$GTEST_PREFIX/lib" -lgtest -lgtest_main -o "$OUT_DIR/native_host_tests"

export RCB_DICTS_DIR="${RCB_DICTS_DIR:-$APP_DIR/src/main/assets/dicts}"
export RCB_CORPUS_DIR="${RCB_CORPUS_DIR:-$TESTS_DIR/data/v2_stack_overflow}"
# forgetting_curve_utils.cpp: a known float-to-int conversion when the library initializes, unrelated
# to dictionary reading; suppressed so every other undefined behaviour stops the run.
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1:suppressions=$HOST_DIR/ubsan_suppressions.txt}"
# Old AOSP tests that fail without any change of ours are skipped unless --all is given.
FILTER=()
if [ "$RUN_ALL" = 0 ]; then
    known="$(grep -v '^#' "$HOST_DIR/known_upstream_failures.txt" | grep . | paste -sd: -)"
    [ -z "$known" ] || { FILTER=("--gtest_filter=-$known"); echo "Skipping known upstream failures: $known"; }
fi
echo "Running $OUT_DIR/native_host_tests"
"$OUT_DIR/native_host_tests" "${FILTER[@]+"${FILTER[@]}"}" "$@"
