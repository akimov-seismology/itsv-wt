#!/usr/bin/env python3
"""Build an ITSV *source-only* archive, never bundling downloaded dependencies.

Run `make dist-src` or `python3 scripts/make_source_release.py`.
"""

import argparse
import gzip
import io
import os
from pathlib import Path
import tarfile
import time

ROOT = Path(__file__).resolve().parent.parent
TOP_LEVEL_FILES = [
    ".gitignore", "Doxyfile",
    "DESIGN.md", "LAYOUT_CUSTOMIZATION.md", "LICENSE", "Makefile",
    "Makefile.asan", "MOUSE_CONTROLS.md", "README.md",
    "SOURCE_RELEASE.md", "THIRD_PARTY_NOTICES.md", "VALIDATION.md",
    "run", "wt_config.xml",
    "third_party/LIBMSEED_2.19.8_SOURCE.md",
    "third_party/fetch-libmseed-2.19.8.sh",
]
SOURCE_SUFFIXES = {".cpp", ".h", ".py", ".sh"}
FORBIDDEN_SUFFIXES = {".a", ".o", ".so", ".so.0", ".ttf", ".otf", ".woff", ".woff2", ".eot", ".class", ".exe", ".pyc"}


def choose_files():
    result = []
    for name in TOP_LEVEL_FILES:
        path = ROOT / name
        if not path.is_file():
            raise SystemExit("Required source-release file is missing: " + name)
        result.append(path)
    for folder in ("src", "tests", "scripts"):
        for file in sorted((ROOT / folder).rglob("*")):
            if file.is_file() and file.suffix in SOURCE_SUFFIXES:
                result.append(file)
    # An original placeholder may be overwritten by the first vendoring run.
    placeholder = ROOT / "third_party/libmseed/README.VENDORING.md"
    if placeholder.is_file():
        result.append(placeholder)
    for path in result:
        if path.is_symlink():
            raise SystemExit("Refusing a symlink in the source-release allowlist: " + str(path))
        rel = path.relative_to(ROOT)
        if "resources" in rel.parts or "build" in rel.parts or "build-asan" in rel.parts:
            raise SystemExit("Forbidden generated directory: " + str(rel))
        if any(str(rel).endswith(x) for x in FORBIDDEN_SUFFIXES):
            raise SystemExit("Binary/font discovered in source release: " + str(rel))
        if rel.parts[:2] == ("third_party", "libmseed") and rel.name != "README.VENDORING.md":
            raise SystemExit("Forbidden vendored libmseed file: " + str(rel))
    if len(set(result)) != len(result):
        raise SystemExit("Repeated source files selected")
    return sorted(result, key=lambda f: str(f.relative_to(ROOT)))


def source_archive_timestamp():
    """Return the archive timestamp; optionally use SOURCE_DATE_EPOCH."""
    setting = os.environ.get("SOURCE_DATE_EPOCH")
    if setting is None:
        return int(time.time())
    try:
        stamp = int(setting)
    except ValueError as exc:
        raise SystemExit("SOURCE_DATE_EPOCH must be an integer Unix timestamp") from exc
    if not 0 <= stamp <= 0xFFFFFFFF:
        raise SystemExit("SOURCE_DATE_EPOCH must be between 0 and 4294967295")
    return stamp


def archive(output):
    files = choose_files()
    stamp = source_archive_timestamp()
    output.parent.mkdir(parents=True, exist_ok=True)
    # Give every archive entry and the gzip header the same creation timestamp.
    # SOURCE_DATE_EPOCH can be set for reproducible packages.
    with output.open("wb") as f, gzip.GzipFile(fileobj=f, mode="wb", filename="", mtime=stamp) as z:
        with tarfile.open(fileobj=z, mode="w", format=tarfile.PAX_FORMAT) as tf:
            root = tarfile.TarInfo("itsv-linux/")
            root.type = tarfile.DIRTYPE
            root.mode, root.uid, root.gid, root.mtime = 0o755, 0, 0, stamp
            root.uname = root.gname = ""
            tf.addfile(root)
            for source in files:
                dest = "itsv-linux/" + source.relative_to(ROOT).as_posix()
                data = source.read_bytes()
                info = tarfile.TarInfo(dest)
                info.size = len(data)
                info.uid, info.gid, info.mtime = 0, 0, stamp
                info.uname = info.gname = ""
                info.mode = 0o755 if source.stat().st_mode & 0o111 else 0o644
                tf.addfile(info, io.BytesIO(data))
    print("Created:", output)
    print("Source files:", len(files))
    print("Release timestamp (UTC):", time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(stamp)))
    print("Bundled Wt resources, downloaded libmseed sources, binaries, fonts: 0")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "dist/itsv-linux-source.tar.gz")
    options = parser.parse_args()
    archive(options.output.resolve())


if __name__ == "__main__":
    main()
