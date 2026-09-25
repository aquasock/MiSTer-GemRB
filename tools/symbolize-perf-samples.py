#!/usr/bin/env python3
"""Symbolicate noodles-perf-sampler output against the unstripped ARM build."""

from __future__ import annotations

import argparse
import collections
import pathlib
import re
import subprocess
import sys


MAP_RE = re.compile(
    r"^([0-9a-f]+)-([0-9a-f]+)\s+([-rwxps]+)\s+([0-9a-f]+)\s+\S+\s+\d+\s*(.*)$"
)
SAMPLE_RE = re.compile(r"^SAMPLE\s+([0-9a-f]+)\s+(\d+)$")


def local_binary(root: pathlib.Path, remote: str) -> pathlib.Path | None:
    relative = remote.removeprefix("/media/fat/gemrb/")
    candidates = []
    if remote.startswith("/media/fat/gemrb/"):
        candidates.extend(
            [
                root / "work/gemrb-install/media/fat/gemrb" / relative,
                root / "work/prefix/lib" / pathlib.Path(relative).name,
                root / "work/bundle/gemrb" / relative,
            ]
        )
    name = pathlib.Path(remote).name
    candidates.extend(
        [
            root / "work/prefix/lib" / name,
            pathlib.Path("/usr/arm-linux-gnueabihf/lib") / name,
            pathlib.Path("/usr/arm-linux-gnueabihf/libc") / name,
        ]
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    for base in (root / "work/gemrb-install", root / "work/prefix"):
        if base.exists():
            matches = list(base.rglob(name))
            if matches:
                return matches[0].resolve()
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=pathlib.Path)
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--limit", type=int, default=50)
    args = parser.parse_args()

    mappings = []
    samples = []
    summary = ""
    in_maps = False
    for raw in args.capture.read_text(errors="replace").splitlines():
        if raw == "MAPS_BEGIN":
            in_maps = True
            continue
        if raw == "MAPS_END":
            in_maps = False
            continue
        if in_maps:
            match = MAP_RE.match(raw)
            if match and "x" in match.group(3) and match.group(5).startswith("/"):
                mappings.append(
                    (
                        int(match.group(1), 16),
                        int(match.group(2), 16),
                        int(match.group(4), 16),
                        match.group(5),
                    )
                )
            continue
        match = SAMPLE_RE.match(raw)
        if match:
            samples.append((int(match.group(1), 16), int(match.group(2))))
        elif raw.startswith("SUMMARY "):
            summary = raw

    grouped: dict[tuple[str, pathlib.Path], list[tuple[int, int]]] = collections.defaultdict(list)
    unresolved = 0
    for ip, count in samples:
        mapping = next((entry for entry in mappings if entry[0] <= ip < entry[1]), None)
        if not mapping:
            unresolved += count
            continue
        start, _end, offset, remote = mapping
        binary = local_binary(args.root, remote)
        if not binary:
            unresolved += count
            continue
        grouped[(remote, binary)].append((ip - start + offset, count))

    functions: collections.Counter[tuple[str, str, str]] = collections.Counter()
    for (remote, binary), addresses in grouped.items():
        command = ["arm-linux-gnueabihf-addr2line", "-f", "-C", "-e", str(binary)]
        command.extend(f"0x{address:x}" for address, _count in addresses)
        result = subprocess.run(command, check=True, text=True, capture_output=True)
        lines = result.stdout.splitlines()
        if len(lines) != len(addresses) * 2:
            raise RuntimeError(f"unexpected addr2line output for {binary}")
        for index, (_address, count) in enumerate(addresses):
            function = lines[index * 2]
            location = lines[index * 2 + 1]
            functions[(remote, function, location)] += count

    total = sum(functions.values()) + unresolved
    print(summary or f"SUMMARY samples={total}")
    print(f"symbolicated={total - unresolved} unresolved={unresolved}")
    print("samples percent object function location")
    for (remote, function, location), count in functions.most_common(args.limit):
        percent = 100.0 * count / total if total else 0.0
        print(f"{count:7d} {percent:6.2f}% {pathlib.Path(remote).name} {function} {location}")
    if unresolved:
        percent = 100.0 * unresolved / total if total else 0.0
        print(f"{unresolved:7d} {percent:6.2f}% [unresolved]")
    return 0 if total else 1


if __name__ == "__main__":
    sys.exit(main())
