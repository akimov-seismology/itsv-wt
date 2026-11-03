# ITSV source-only distribution

This source archive is designed for publishing on GitHub or sharing by email.
It contains ITSV source, project metadata/configuration, tests, and build
scripts. It does not include Wt libraries, Wt web assets, fonts, libmseed,
or binaries. The `wt_config.xml` configuration originated as a Wt template;
see `THIRD_PARTY_NOTICES.md`.

## Licensing

- Original ITSV code: MIT (`LICENSE`), copyright (c) 2026 Andrei Akimov.
- Wt: GPL-2.0-only with OpenSSL exception, or separate commercial license.
- libmseed 2.19.8: LGPL-3.0-or-later.
- These upstream licenses are **not superseded** by ITSV's MIT license.

See `THIRD_PARTY_NOTICES.md` before distributing any compiled application.
A binary combining the present Wt and libmseed licenses has an unresolved
GPLv2/LGPLv3 incompatibility. This source-only release does not purport to
resolve or authorize binary distribution of that combination.

## Reproduce this archive

From the ITSV project directory:

```sh
make dist-src
# or: python3 scripts/make_source_release.py --output /path/to/itsv-source.tar.gz
```

Archive dates: by default all entries (including the top-level directory)
are stamped with the **date and time the source package was created**.
For a byte-for-byte reproducible archive, set `SOURCE_DATE_EPOCH` to a
meaningful Unix timestamp, for example:

```sh
SOURCE_DATE_EPOCH=1793577600 make dist-src
```

The release packager explicitly selects ITSV source files, docs,
tests, configuration and license notices. It omits:

- `resources/` (installed Wt runtime assets, including any fonts)
- `third_party/libmseed/` contents other than `README.VENDORING.md`
- local binaries, `.a` and `.o` files, build directories, caches, archives

Do not create source releases by simply archiving your entire local working
directory after building ITSV: that may accidentally ship Wt resources,
libmseed's fetched source, and compiled artifacts.

## Build from a source archive

The original Makefile downloads verified libmseed 2.19.8 source and compiles
`third_party/libmseed/libmseed.a`. The Wt libraries must be installed separately.

```sh
make -j4                 # or make -j4 WT_PREFIX=/usr
./run
```

`./run` uses `resources/` from your local Wt installation. If it cannot
locate them, specify `WT_RESOURCES_DIR` as the directory containing Wt's
`themes/default/wt.css`, for example:

```sh
WT_RESOURCES_DIR=/usr/local/share/Wt/resources ./run
```

A local symbolic link called `resources` is created for the running app. This
is not copied into any future source-only release made with `make dist-src`.

## Check archive contents

```sh
tar -tzf dist/itsv-linux-source.tar.gz | less
# Verify no file names under resources/, no .a / .o / .so / executable,
# and no downloaded files from third_party/libmseed/.
```

The packager also performs these checks automatically before writing an archive.
