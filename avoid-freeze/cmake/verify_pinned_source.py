"""Verify the public BestClient source pin used by the standalone predictor.

This checks only a separately extracted public source tree. It never reads the
installed game and never modifies the supplied dependency.
"""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path, PurePosixPath

EXPECTED_COMMIT = "cf192e3fb8590dbe2e4f66a191aa6616beff6975"
MAX_FILES = 4096
MAX_FILE_BYTES = 16 * 1024 * 1024
MAX_TOTAL_BYTES = 64 * 1024 * 1024


def verify(manifest_path: Path, source_root: Path) -> tuple[int, int]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("source_commit") != EXPECTED_COMMIT:
        raise ValueError("source manifest does not identify the pinned commit")
    files = manifest.get("files")
    if not isinstance(files, dict) or not 1 <= len(files) <= MAX_FILES:
        raise ValueError("invalid pinned file manifest")

    root = source_root.resolve(strict=True)
    total = 0
    for relative, expected in files.items():
        if not isinstance(relative, str) or not isinstance(expected, str):
            raise ValueError("manifest entries must be text")
        name = PurePosixPath(relative)
        if name.is_absolute() or ".." in name.parts or "\\" in relative or ":" in relative:
            raise ValueError("unsafe source path in manifest")
        if len(expected) != 64 or any(ch not in "0123456789abcdef" for ch in expected):
            raise ValueError("invalid SHA-256 in manifest")

        path = (root / relative).resolve(strict=True)
        if not path.is_relative_to(root) or not path.is_file():
            raise ValueError("source file escapes pinned tree")
        size = path.stat().st_size
        total += size
        if size > MAX_FILE_BYTES or total > MAX_TOTAL_BYTES:
            raise ValueError("pinned source exceeds verifier size limits")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != expected:
            raise ValueError(f"pinned source mismatch: {relative}")
    return len(files), total


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: verify_pinned_source.py MANIFEST SOURCE_ROOT", file=sys.stderr)
        return 2
    try:
        count, total = verify(Path(sys.argv[1]), Path(sys.argv[2]))
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"Pinned public source verification failed: {error}", file=sys.stderr)
        return 1
    print(f"Verified {count} files / {total} bytes from public pin cf192e3")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
