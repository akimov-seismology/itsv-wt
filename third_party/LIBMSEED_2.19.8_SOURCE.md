# libmseed 2.19.8 source provenance

ITSV uses libmseed **2.19.8**.

The vendoring script downloads Debian's unmodified upstream source archive:

- Archive: `libmseed_2.19.8.orig.tar.gz`
- SHA-256: `29080c80ef0bf9dd97056ca20079ad8db75e2d0f283e187ff45065337d3abe05`
- Debian source package version: `2.19.8-1`
- Upstream tag: `v2.19.8`

Run from the ITSV root:

    make vendor-libmseed

or simply:

    make -j4

The first build fetches and verifies the source automatically. Once fetched,
the project builds against `third_party/libmseed/libmseed.a` and no longer
requires `../external_libs/libmseed`.
