#!/usr/bin/env bash
# KamTape PS5 - builds and runs the PC tests (address handling, settings).
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cxx=$(command -v "${HOST_CXX:-clang++}")
mkdir -p "$root/build/tests"
"$cxx" -std=c++20 -O1 -Wall -Wextra -Werror -I"$root/src" \
    "$root/host/address_test.cpp" "$root/src/kt/address.cpp" "$root/src/kt/settings.cpp" \
    "$root/src/kt/json.cpp" "$root/src/core/save_file.cpp" -o "$root/build/tests/address_test"
"$root/build/tests/address_test"
