#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo"
build_root=${SHADPS4_BUILD_ROOT:-"$repo/build/linux"}
jobs=${SHADPS4_BUILD_JOBS:-4}
# Mesa/KosmicKrisp is an Apple-only dependency; avoid downloading its source on Linux.
mapfile -t modules < <(git config -f .gitmodules --get-regexp path |
    awk '$2 != "externals/mesa-kosmickrisp" { print $2 }')
git submodule update --init --recursive --jobs "$jobs" -- "${modules[@]}"
common=(-G Ninja -DCMAKE_C_COMPILER="${CC:-clang}" -DCMAKE_CXX_COMPILER="${CXX:-clang++}"
    -DCMAKE_CXX_FLAGS="${CXXFLAGS:--stdlib=libc++ -fexperimental-library}")
cmake -S . -B "$build_root/app" "${common[@]}" -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=OFF
cmake --build "$build_root/app" --parallel "$jobs"
cmake -S . -B "$build_root/tests" "${common[@]}" -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON
cmake --build "$build_root/tests" --parallel "$jobs"
test_status=0
ctest --test-dir "$build_root/tests" --output-on-failure -E GcnTest || test_status=$?
python3 tests/check_linux_startup.py "$build_root/tests/tests/shadps4_linux_startup_test"
"$build_root/app/shadps4" --help
exit "$test_status"
