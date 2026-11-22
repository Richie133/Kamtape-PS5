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
"$out/address_test"
"$out/api_test" "$root/host/fixtures"
