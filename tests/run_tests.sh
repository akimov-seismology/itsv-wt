#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="$(mktemp -d "${TMPDIR:-/tmp}/itsv-linux-tests.XXXXXX")"
trap 'rm -rf "$build"' EXIT
cxx="${CXX:-g++}"
flags=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -O2)
if [[ "${ITSV_SANITIZE:-0}" == 1 ]]; then
  flags=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -O1 -g \
         -fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"$cxx" "${flags[@]}" -I"$root/src" \
  "$root/tests/core_tests.cpp" "$root/src/Recording.cpp" "$root/src/ViewState.cpp" \
  -o "$build/core_tests"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1}" "$build/core_tests"
