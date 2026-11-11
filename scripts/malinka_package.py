#!/usr/bin/env python3
"""Create one flat, checked ZIP for the Malinka updater (Python >= 3.11).

The input is a fresh CMake package_zip staging directory, NOT a user's install.
The source archive is a companion deliverable; its SHA-256 is in the manifest.
Checksums/build markers are not digital signatures or official authorization.
Never overwrites an output. No third-party Python modules are required.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import stat
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = "malinka-update.manifest"
MAX_FILE = 512 * 1024 * 1024
MAX_TOTAL = 2 * 1024 * 1024 * 1024
MAX_FILES = 10000
REQUIRED_NOTICES = ("LICENSE", "license.txt", "docs/MALINKA_SOURCES.md", "docs/MALINKA_STATUS.md")


def no_links(path):
    absolute = path.absolute()
    return all(not part.is_symlink() and not (getattr(part.lstat(), "st_file_attributes", 0) & 0x400)
               for part in (absolute, *absolute.parents) if part.exists() or part.is_symlink())


def safe_relative(name):
    if (not name or len(name.encode("utf-8")) > 240 or name.startswith("/") or name.endswith("/")
            or "//" in name or any(c in name for c in '\\:*?"<>|')
            or any(ord(c) < 32 or ord(c) == 127 for c in name)):
        return False
    # Python strings must also be encodable without lone surrogates.
    name.encode("utf-8", errors="strict")
    for part in name.split("/"):
        if part in ("", ".", "..") or part.endswith((".", " ")):
            return False
        base = part.partition(".")[0].upper()
        if base in {"CON", "NUL", "PRN", "AUX"} or re.fullmatch(r"(?:COM|LPT)[0-9]", base):
            return False
    return True


def ascii_lower(text):
    return text.translate(str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz"))


def digest_file(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def has_marker(path, marker):
    previous = b""
    with path.open("rb") as source:
        while chunk := source.read(65536):
            window = previous + chunk
            if marker in window:
                return True
            previous = window[-len(marker):]
    return False


def identity():
    header = (ROOT / "src/game/client/components/malinka/build_identity.h").read_text(encoding="utf-8")
    def value(name):
        match = re.search(r'^#define ' + name + r' "([^"\r\n]+)"$', header, re.MULTILINE)
        if not match:
            raise ValueError(f"Build identity is missing {name}")
        return match.group(1)
    version = value("MALINKA_VERSION")
    if not re.fullmatch(r"[0-9]{1,6}\.[0-9]{1,6}\.[0-9]{1,6}", version):
        raise ValueError("Updater versions must be numeric major.minor.patch")
    upstream = value("MALINKA_UPSTREAM_COMMIT")
    if not re.fullmatch(r"[0-9a-f]{40}", upstream):
        raise ValueError("Malformed upstream commit")
    return version, f"BestClient-Malinka:api=1;version={version};upstream={upstream}".encode(), value("MALINKA_UPDATER_MARKER").encode()


def collect(stage):
    if not no_links(stage) or not stage.is_dir():
        raise ValueError("Stage must be a real, unlinked directory")
    result, seen = [], set()
    total, directory_count = 0, 0
    for parent, directories, files in os.walk(stage, followlinks=False):
        directory_count += 1
        if directory_count > MAX_FILES:
            raise ValueError("Package directory limit exceeded")
        for name in directories + files:
            path = Path(parent) / name
            relative = path.relative_to(stage).as_posix()
            canonical = ascii_lower(relative)
            if not safe_relative(relative) or not no_links(path):
                raise ValueError(f"Unsafe/linked stage path: {relative!r}")
            if canonical in seen or canonical == MANIFEST or canonical == "update" or canonical.startswith("update/"):
                raise ValueError(f"Duplicate/reserved stage path: {relative!r}")
            seen.add(canonical)
        for name in files:
            path = Path(parent) / name
            info = path.stat()
            if not stat.S_ISREG(info.st_mode) or info.st_size > MAX_FILE:
                raise ValueError(f"Unsupported/oversized file: {path}")
            total += info.st_size
            if total > MAX_TOTAL or len(result) >= MAX_FILES - 1:  # ZIP includes the manifest too.
                raise ValueError("Package file count/size limit exceeded")
            result.append((path.relative_to(stage).as_posix(), path, info.st_size, digest_file(path), info.st_mode))
    return sorted(result)


def publish_exclusive(temporary, output):
    # Same-volume hard-link publication fails rather than overwriting an output
    # that appeared after the initial existence check. Temporary is private.
    os.link(temporary, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True, help="Companion source tar.gz/zip, distributed alongside the ZIP")
    parser.add_argument("--platform", required=True, choices=("windows-x86_64", "linux-x86_64", "linux-aarch64"))
    parser.add_argument("--executable", default=None)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    stage, output = Path(os.path.abspath(args.stage)), Path(os.path.abspath(args.output))
    executable = args.executable or ("DDNet.exe" if args.platform.startswith("windows-") else "DDNet")
    updater = "bestclient-updater.exe" if args.platform.startswith("windows-") else "bestclient-updater"
    if not safe_relative(executable) or len(PurePosixPath(executable).parts) != 1:
        parser.error("Client executable must be a single safe filename")
    if output.name != f"bestclient-malinka-{args.platform}.zip":
        parser.error("Output basename must match the updater's platform-specific asset name")
    sidecar = output.with_name(output.name + ".sha256")
    if any(p.exists() or p.is_symlink() for p in (output, sidecar)):
        parser.error("Output/sidecar already exists; neither was changed")
    output.parent.mkdir(parents=True, exist_ok=True)
    if not no_links(output.parent) or output.is_relative_to(stage):
        parser.error("Output parent must be unlinked and outside the staging tree")
    if not no_links(args.source) or not args.source.is_file() or args.source.stat().st_size > MAX_FILE:
        parser.error("Source archive is missing, oversized, or linked")
    version, client_marker, updater_marker = identity()
    files = collect(stage)
    names = {item[0] for item in files}
    for required in (executable, updater, *REQUIRED_NOTICES):
        if required not in names:
            parser.error(f"Package is missing {required}")
    if not has_marker(stage / executable, client_marker) or not has_marker(stage / updater, updater_marker):
        parser.error("Client/updater build marker mismatch; refusing to package a stock client")
    source_hash = digest_file(args.source)
    manifest = (f'MALINKA_PACKAGE 1\nid BestClient-Malinka\nversion {version}\nplatform {args.platform}\n'
                f'executable "{executable}"\nsource_sha256 {source_hash}\nfiles {len(files)}\n')
    for relative, _, size, digest, _ in files:
        manifest += f'{digest} {size} "{relative}"\n'
    manifest = manifest.encode("utf-8")
    if len(manifest) > 4 * 1024 * 1024 or sum(item[2] for item in files) + len(manifest) > MAX_TOTAL:
        raise ValueError("Manifest/expanded package limit exceeded")
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "315532800"))
    if epoch < 315532800 or epoch > 4354819199:
        raise ValueError("SOURCE_DATE_EPOCH must be in the ZIP date range 1980..2107")
    date = datetime.fromtimestamp(epoch, timezone.utc).timetuple()[:6]
    with tempfile.TemporaryDirectory(prefix=".malinka-package-", dir=output.parent) as scratch_name:
        temporary = Path(scratch_name) / output.name
        with zipfile.ZipFile(temporary, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9, allowZip64=False) as archive:
            for relative, path, size, digest, mode in files:
                info = zipfile.ZipInfo(relative, date)
                info.create_system = 3
                permissions = 0o755 if mode & 0o111 or relative in (executable, updater) else 0o644
                info.external_attr = (stat.S_IFREG | permissions) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                current_hash, copied = hashlib.sha256(), 0
                with path.open("rb") as source, archive.open(info, "w") as dest:
                    while chunk := source.read(65536):
                        copied += len(chunk)
                        if copied > size:
                            raise ValueError(f"Stage file grew while packaging: {relative}")
                        current_hash.update(chunk)
                        dest.write(chunk)
                if copied != size or current_hash.hexdigest() != digest:
                    raise ValueError(f"Stage file changed while packaging: {relative}")
            info = zipfile.ZipInfo(MANIFEST, date)
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, manifest)
        if temporary.stat().st_size > MAX_FILE:
            raise ValueError("Compressed archive exceeds the updater's 512 MiB limit")
        with zipfile.ZipFile(temporary) as archive:
            if archive.testzip() is not None or archive.read(MANIFEST) != manifest:
                raise ValueError("Generated ZIP readback failed")
        archive_hash = digest_file(temporary)
        checksum = Path(scratch_name) / sidecar.name
        checksum.write_text(f"{archive_hash}  {output.name}\n", encoding="ascii")
        publish_exclusive(temporary, output)
        try:
            publish_exclusive(checksum, sidecar)
        except Exception:
            output.unlink()  # This invocation just created it, never a pre-existing file.
            raise
    print(f"{output}: version {version}, {len(files)} files, SHA-256 {archive_hash}")
    print(f"Companion source SHA-256 {source_hash}; no digital signature/official badge authorization")


if __name__ == "__main__":
    main()
