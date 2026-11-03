#!/usr/bin/env python3
"""Test source release contents without Wt, libmseed or network access."""

from pathlib import Path
import os
import subprocess
import sys
import tarfile
import time
from tempfile import TemporaryDirectory

root = Path(__file__).resolve().parent.parent
with TemporaryDirectory(prefix="itsv-src-release-") as temp:
    package = Path(temp) / "it-release.tar.gz"
    # Test the default release date, even if the caller uses SOURCE_DATE_EPOCH
    # for an unrelated reproducible build.
    clean_env = os.environ.copy()
    clean_env.pop("SOURCE_DATE_EPOCH", None)
    started = int(time.time())
    subprocess.run([sys.executable, str(root / "scripts/make_source_release.py"),
                    "--output", str(package)], check=True, env=clean_env)
    with tarfile.open(package, "r:gz") as archive:
        # Archive members must have a meaningful, consistent creation date.
        stamps = [int(member.mtime) for member in archive.getmembers()]
        assert len(stamps) > 1
        assert len(set(stamps)) == 1, stamps
        assert started <= stamps[0] <= int(time.time()), stamps[0]
        entries = {name for name in archive.getnames() if name != "itsv-linux"}
        required = {
            "itsv-linux/LICENSE", "itsv-linux/THIRD_PARTY_NOTICES.md",
            "itsv-linux/SOURCE_RELEASE.md", "itsv-linux/README.md",
            "itsv-linux/src/main.cpp", "itsv-linux/Makefile",
            "itsv-linux/run", "itsv-linux/third_party/fetch-libmseed-2.19.8.sh",
        }
        assert required <= entries, sorted(required - entries)
        assert not any(name.startswith("itsv-linux/resources/") for name in entries)
        assert not any(name.startswith("itsv-linux/third_party/libmseed/") and
                       not name.endswith("/README.VENDORING.md") for name in entries)
        forbidden = (".a", ".o", ".so", ".ttf", ".otf", ".woff", ".woff2", ".eot", ".exe", ".pyc")
        assert not any(name.endswith(forbidden) for name in entries)
        assert all(member.isfile() or member.isdir() for member in archive.getmembers())
        assert archive.extractfile("itsv-linux/THIRD_PARTY_NOTICES.md").read().find(b"GPLv2") >= 0
    # The gzip header date must match the archived members.
    header = package.read_bytes()[:10]
    assert header[:3] == b"\x1f\x8b\x08"
    assert int.from_bytes(header[4:8], "little") == stamps[0]

    # Explicit SOURCE_DATE_EPOCH must make byte-identical archives.
    fixed = 1793577600  # Reproducibility fixture
    env = dict(clean_env, SOURCE_DATE_EPOCH=str(fixed))
    archive1, archive2 = Path(temp) / "fixed1.tar.gz", Path(temp) / "fixed2.tar.gz"
    for candidate in (archive1, archive2):
        subprocess.run([sys.executable, str(root / "scripts/make_source_release.py"),
                        "--output", str(candidate)], check=True, env=env)
    assert archive1.read_bytes() == archive2.read_bytes()
    with tarfile.open(archive1, "r:gz") as fixed_package:
        assert all(m.mtime == fixed for m in fixed_package.getmembers())
    assert int.from_bytes(archive1.read_bytes()[4:8], "little") == fixed

print("PASS: source-only release contains required notices, no binaries/fonts, and correct non-1969 timestamps")
