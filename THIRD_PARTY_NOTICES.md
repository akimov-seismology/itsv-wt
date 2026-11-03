# Third-party software and license notices

Copyright (c) 2026 Andrei Akimov — original ITSV source: MIT License (`LICENSE`).

The MIT license covers original ITSV source files, **not** Wt, libmseed, Wt
runtime resources, or other software installed or downloaded separately.
The source-only distribution deliberately does **not** bundle Wt, its
web resources/fonts, libmseed source, or compiled third-party libraries.

## Wt — C++ Web Toolkit (Emweb)

- Project: https://www.webtoolkit.eu/wt
- License information: https://github.com/emweb/wt/blob/master/LICENSE
- Detailed component licenses: https://github.com/emweb/wt/blob/master/doc/licenses.md
- Copyright: Emweb bv and relevant contributors.
- License: GNU GPL **version 2 only**, with the Wt OpenSSL exception, **or** a
  separate commercial license. GPLv3 does not apply to Wt itself.

ITSV uses Wt C++ APIs, but does not redistribute Wt libraries or Wt resources.
The `wt_config.xml` configuration file is adapted from the Wt configuration
template and is not claimed as independently MIT-licensed Wt material.
Install Wt separately and provide its installed resources directory when
running ITSV (`WT_RESOURCES_DIR` if automatic detection fails). The Wt logo or
website link in Help > About is acknowledgment, not a replacement for its GPL
license terms.

## libmseed 2.19.8 (EarthScope / IRIS, Chad Trabant and contributors)

- Project: https://github.com/EarthScope/libmseed
- Debian original source release: https://deb.debian.org/debian/pool/main/libm/libmseed/libmseed_2.19.8.orig.tar.gz
- Library license: GNU LGPL **version 3 or later**.
- License text is included in libmseed upstream's `LICENSE.txt`, downloaded
  with the source; retain it when independently redistributing libmseed.

The ITSV build downloads the original libmseed **source** after verifying its
SHA-256 digest, then builds `third_party/libmseed/libmseed.a` on the user's
machine. Neither the `.a` file nor the downloaded libmseed source is part of
ITSV's public source-only release. See
`third_party/LIBMSEED_2.19.8_SOURCE.md` for provenance.

## Important limitation for binary redistribution

Wt's GPLv2-only and libmseed's LGPLv3-or-later are license-incompatible when
combined into a single distributable executable under the standard GPL/LGPL
terms. Merely releasing ITSV's original source under MIT or adding attribution
**does not** solve that problem. The build currently statically links
`libmseed.a` to the Wt-based ITSV application.

This release is **source code only**. Do **not** treat these notices as
permission to redistribute the resulting executable. Before distributing
binaries, resolve the combined-work license issue, e.g., obtain suitable
additional permissions/licenses from the relevant rights holders or use a
compatible alternative dependency. Consult a qualified licensing adviser if
needed. Private local builds and use are a different case from distribution.

Reference: https://www.gnu.org/licenses/gpl-faq.en.html#v2v3Compatibility

## Source release packaging

Use `make dist-src` to create an archive. It excludes local build products,
`resources/`, fonts and other Wt assets, and fetched libmseed files.
Original ITSV copyright statements and this notice must be retained when
redistributing source. Do not change third-party license declarations to MIT.
