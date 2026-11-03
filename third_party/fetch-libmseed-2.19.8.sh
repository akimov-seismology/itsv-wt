#!/usr/bin/env bash
set -euo pipefail

VERSION="2.19.8"
DEST="$(cd "$(dirname "$0")" && pwd)/libmseed"
ARCHIVE="libmseed_${VERSION}.orig.tar.gz"
URL="https://deb.debian.org/debian/pool/main/libm/libmseed/${ARCHIVE}"
SHA256="29080c80ef0bf9dd97056ca20079ad8db75e2d0f283e187ff45065337d3abe05"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

if [[ -f "$DEST/libmseed.h" ]] && grep -q 'LIBMSEED_VERSION.*2\.19\.8\|LIBMSEED_RELEASE.*2\.19\.8' "$DEST/libmseed.h" 2>/dev/null; then
  echo "libmseed ${VERSION} is already present in $DEST"
  exit 0
fi

echo "Fetching libmseed ${VERSION} from Debian's unmodified upstream source archive..."
if command -v curl >/dev/null 2>&1; then
  curl -fL --retry 3 -o "$TMPDIR/$ARCHIVE" "$URL"
elif command -v wget >/dev/null 2>&1; then
  wget -O "$TMPDIR/$ARCHIVE" "$URL"
else
  echo "ERROR: curl or wget is required to fetch libmseed ${VERSION}." >&2
  exit 1
fi

actual="$(sha256sum "$TMPDIR/$ARCHIVE" | awk '{print $1}')"
if [[ "$actual" != "$SHA256" ]]; then
  echo "ERROR: libmseed archive checksum mismatch." >&2
  echo "Expected: $SHA256" >&2
  echo "Actual:   $actual" >&2
  exit 1
fi

mkdir -p "$TMPDIR/src"
tar -xzf "$TMPDIR/$ARCHIVE" -C "$TMPDIR/src"
srcdir="$(find "$TMPDIR/src" -mindepth 1 -maxdepth 1 -type d | head -n1)"
if [[ -z "$srcdir" || ! -f "$srcdir/libmseed.h" ]]; then
  echo "ERROR: unexpected libmseed archive layout." >&2
  exit 1
fi

rm -rf "$DEST"
mkdir -p "$DEST"
cp -a "$srcdir"/. "$DEST"/

echo "Vendored libmseed ${VERSION} into $DEST"
echo "Original libmseed license: $DEST/LICENSE.txt"
