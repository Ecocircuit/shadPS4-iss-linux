#!/usr/bin/env python3
"""Apply narrow, idempotent compatibility fixes to the pinned dependencies.

Keep upstream submodule revisions intact. Modern libc++ no longer supplies some
headers transitively; its unique_ptr overloads also require an explicit callable.
"""
from pathlib import Path


def prepare(root: Path) -> None:
    for relative, anchor, include in (
        ("externals/zarchive/src/zarchivewriter.cpp", b"#include <cassert>", b"#include <stdlib.h>"),
        ("externals/sirit/src/stream.h", b"#include <cstddef>", b"#include <cstdlib>"),
    ):
        path = root / relative
        data = path.read_bytes()
        if include in data:
            continue
        if anchor not in data:
            raise RuntimeError(f"Expected include anchor not found: {path}")
        newline = b"\r\n" if b"\r\n" in data else b"\n"
        path.write_bytes(data.replace(anchor, anchor + newline + include, 1))
        print(f"Applied libc++ include fix: {relative}")

    path = root / "externals/openal-soft/alc/alc.cpp"
    data = path.read_bytes()
    old = b"std::views::transform(&ContextBase::EffectSlotCluster::operator*)"
    new = b"std::views::transform([](auto const& cluster) -> decltype(auto) { return *cluster; })"
    if old in data:
        path.write_bytes(data.replace(old, new))
        print("Applied libc++ callable fix: externals/openal-soft/alc/alc.cpp")
    elif new not in data:
        raise RuntimeError(f"Expected OpenAL transform not found: {path}")


if __name__ == "__main__":
    prepare(Path(__file__).resolve().parents[1])
