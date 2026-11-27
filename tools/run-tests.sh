#!/usr/bin/env bash
# KamTape PS5 - builds and runs the PC tests (addresses, settings, XML, API answers).
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cxx=$(command -v "${HOST_CXX:-clang++}")
out="$root/build/tests"
mkdir -p "$out"
flags=(-std=c++20 -O1 -Wall -Wextra -Werror -I"$root/src")
common=("$root/src/kt/json.cpp" "$root/src/core/save_file.cpp")
"$cxx" "${flags[@]}" "$root/host/address_test.cpp" "$root/src/kt/address.cpp" "$root/src/kt/settings.cpp" "$root/src/kt/text_field.cpp" \
    "${common[@]}" -o "$out/address_test"
"$cxx" "${flags[@]}" "$root/host/api_test.cpp" "$root/src/kt/api.cpp" "$root/src/kt/xml.cpp" \
    "$root/src/kt/http_escape.cpp" "${common[@]}" -o "$out/api_test"
cc=$(command -v "${HOST_CC:-clang}")
kit="$root/src/third_party/update_check"
for c in update_check self_update self_update_sha256; do
    "$cc" -std=c11 -O1 -w -DUPDATE_CHECK_NO_NETWORK -I"$root/src" -c "$kit/$c.c" -o "$out/$c.o"
done
"$cxx" "${flags[@]}" "$root/host/release_test.cpp" "$root/src/kt/release.cpp" "${common[@]}" \
    "$out/update_check.o" "$out/self_update.o" "$out/self_update_sha256.o" -lpthread -o "$out/release_test"
"$out/address_test"
"$out/api_test" "$root/host/fixtures"
"$out/release_test" "$root"
# the parsers that read network data, against mangled input (SANITIZE=1: under ASan/UBSan)
fuzz_flags=("${flags[@]}")
[[ -z ${SANITIZE:-} ]] || fuzz_flags+=(-g -fsanitize=address,undefined -fno-sanitize-recover=undefined)
fuzz_c=()
for c in update_check self_update self_update_sha256; do
    "$cc" -std=c11 -O1 -w -DUPDATE_CHECK_NO_NETWORK "${fuzz_flags[@]:2}" -I"$root/src" -c "$kit/$c.c" -o "$out/fuzz-$c.o"
    fuzz_c+=("$out/fuzz-$c.o")
done
"$cxx" "${fuzz_flags[@]}" "$root/host/fuzz_test.cpp" "$root/src/kt/api.cpp" "$root/src/kt/xml.cpp" \
    "$root/src/kt/address.cpp" "$root/src/kt/http_escape.cpp" "$root/src/kt/release.cpp" "${common[@]}" \
    "${fuzz_c[@]}" -lpthread -o "$out/fuzz_test"
ASAN_OPTIONS=detect_leaks=0 "$out/fuzz_test" "$root/host/fixtures" "${FUZZ_ROUNDS:-3000}"
