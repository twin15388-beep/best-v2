#!/usr/bin/env python3
"""Fetch the pinned ddnet-libs SDK. Never replaces an existing directory.

Python >= 3.11, stdlib only. Use --archive for an already downloaded tarball.
Build tools/Rust and Linux development packages are installed separately; see
MALINKA_BUILD.md. This does not fetch the proprietary KRX client or any secrets.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import shutil
import tarfile
import tempfile
import urllib.request


COMMIT = "c0e6703fbcdbe03df2f26875427ec3951ce4ec21"
SHA256 = "5539f55924a88c53f09bb3f2b9fbb8e20d87351aee43210eceddc7994385806f"
URL = f"https://codeload.github.com/ddnet/ddnet-libs/tar.gz/{COMMIT}"
MAX_ARCHIVE = 512 * 1024 * 1024
MAX_EXTRACTED = 2 * 1024 * 1024 * 1024


def unpack(archive, destination):
    with tarfile.open(archive, "r:gz") as tar:
        entries = tar.getmembers()
        if not entries or len(entries) > 10000:
            raise ValueError("Invalid SDK archive entry count")
        prefix = PurePosixPath(entries[0].name).parts[0]
        seen, links = set(), []
        total = 0
        for entry in entries:
            parts = PurePosixPath(entry.name).parts
            if not parts or parts[0] != prefix or entry.name.startswith("/") or ".." in parts:
                raise ValueError("Unsafe SDK archive path")
            relative = PurePosixPath(*parts[1:])
            if str(relative) == ".":
                continue
            if "\\" in str(relative) or str(relative) in seen:
                raise ValueError("Duplicate/ambiguous SDK archive path")
            seen.add(str(relative))
            output = destination.joinpath(*relative.parts)
            # Symlinks are processed LAST; no extracted file is written through one.
            if entry.issym():
                target = posixpath.normpath(posixpath.join(str(relative.parent), entry.linkname))
                if entry.linkname.startswith("/") or "\\" in entry.linkname or target == ".." or target.startswith("../"):
                    raise ValueError("SDK symlink escapes destination")
                links.append((output, entry.linkname))
                continue
            if entry.isdir():
                output.mkdir(parents=True, exist_ok=True)
                continue
            if not entry.isfile() or entry.size < 0:
                raise ValueError("Unsupported SDK entry type")
            total += entry.size
            if entry.size > MAX_ARCHIVE or total > MAX_EXTRACTED:
                raise ValueError("SDK extraction limit exceeded")
            output.parent.mkdir(parents=True, exist_ok=True)
            with tar.extractfile(entry) as source, output.open("xb") as dest:
                shutil.copyfileobj(source, dest, 1024 * 1024)
            if output.stat().st_size != entry.size:
                raise ValueError("Short SDK file read")
            output.chmod(entry.mode & 0o777)
        # These SDK links are for macOS/Linux shared libraries, not Windows .lib
        # or .dll files. Windows can build without developer-mode symlink rights.
        if os.name != "nt":
            for output, target in links:
                output.symlink_to(target)
            for output, _ in links:
                resolved = output.resolve(strict=True)
                if not resolved.is_relative_to(destination.resolve()):
                    raise ValueError("Invalid SDK symlink chain")
    (destination / ".malinka-sdk.json").write_text(json.dumps({
        "repository": "ddnet/ddnet-libs", "commit": COMMIT,
        "archive_sha256": SHA256, "url": URL,
        "windows_skips_non_windows_symlinks": os.name == "nt",
    }, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=Path("ddnet-libs"))
    parser.add_argument("--archive", type=Path)
    args = parser.parse_args()
    destination = args.destination.absolute()
    if destination.exists() or destination.is_symlink():
        parser.error(f"Destination already exists (not modified): {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    # All scratch files are beneath a newly created private directory; never
    # delete a caller-supplied directory on failure.
    with tempfile.TemporaryDirectory(prefix=".malinka-sdk-", dir=destination.parent) as scratch_name:
        scratch = Path(scratch_name)
        archive = args.archive
        if archive is None:
            archive = scratch / "sdk.tar.gz"
            request = urllib.request.Request(URL, headers={"User-Agent": "BestClient-Malinka-bootstrap/1"})
            with urllib.request.urlopen(request, timeout=120) as response, archive.open("xb") as dest:
                size = 0
                while chunk := response.read(1024 * 1024):
                    size += len(chunk)
                    if size > MAX_ARCHIVE:
                        raise ValueError("SDK download limit exceeded")
                    dest.write(chunk)
        if not archive.is_file() or archive.stat().st_size > MAX_ARCHIVE:
            raise ValueError("SDK archive missing/oversized")
        with archive.open("rb") as source:
            digest = hashlib.file_digest(source, "sha256").hexdigest()
        if digest != SHA256:
            raise ValueError(f"SDK SHA-256 mismatch: {digest}")
        unpacked = scratch / "sdk"
        unpacked.mkdir()
        unpack(archive, unpacked)
        if destination.exists() or destination.is_symlink():
            raise FileExistsError(f"Destination appeared during bootstrap: {destination}")
        unpacked.rename(destination)
    print(f"SDK {COMMIT}, SHA-256 verified; installed at {destination}")


if __name__ == "__main__":
    main()
