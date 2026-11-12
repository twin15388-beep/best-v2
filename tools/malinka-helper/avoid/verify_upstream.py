"""Verify the exact public prediction dependency, never the installed client."""
import hashlib
import json
import sys
from pathlib import Path, PurePosixPath


def main():
    if len(sys.argv) != 3:
        raise ValueError("usage: verify_upstream.py manifest.json extracted-public-source")
    manifest = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    root = Path(sys.argv[2]).resolve(strict=True)
    if manifest["source_commit"] != "cf192e3fb8590dbe2e4f66a191aa6616beff6975":
        raise ValueError("unrecognized public source revision")
    files = manifest["files"]
    if not 1 <= len(files) <= 4096:
        raise ValueError("source manifest file count")
    total = 0
    for relative, expected in files.items():
        name = PurePosixPath(relative)
        if name.is_absolute() or ".." in name.parts or "\\" in relative or ":" in relative:
            raise ValueError("unsafe manifest path")
        path = (root / relative).resolve(strict=True)
        if not path.is_relative_to(root) or not path.is_file():
            raise ValueError("source path escapes the verified dependency")
        size = path.stat().st_size
        total += size
        if size > 16 * 1024 * 1024 or total > 64 * 1024 * 1024:
            raise ValueError("public source byte limit")
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError("pinned source differs: " + relative)
    print(f"Verified {len(files)} public dependency files, {total} bytes, cf192e3")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError) as error:
        print("Public prediction source verification failed:", error, file=sys.stderr)
        sys.exit(1)
