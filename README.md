# ITSV — Interactive Time Series Viewer

Copyright (c) 2026 Andrei Akimov  
Original ITSV source license: MIT (see `LICENSE` and `THIRD_PARTY_NOTICES.md`)

ITSV is a **Linux-only** C++17 waveform viewer built with the Wt web toolkit. It
reads MiniSEED with libmseed 2.19.8 and provides multi-trace viewing, channel
selection, time/amplitude zoom, panning, trace marking, an overview waveform and
several amplitude-scaling modes.

## Design

The Linux/Wt architecture separates waveform data, view state, and rendering:

```text
MiniSeedReader
      |
      v
Recording -> Channel -> Segment<double samples>
      |
      v
ViewState
      |
      +--> WaveformDisplayWidget
      +--> WaveformPreviewWidget
      +--> ChannelSelectionDialog
```

`Recording` owns waveform data. `ViewState` owns UI state such as visible trace
order, marks, overview channel, time range, amplitude mode, clipping and
inversion. Wt widgets are presentation/controllers and do not own duplicate
waveform copies.

## Requirements

- Linux
- C++17 compiler (GCC or Clang)
- Wt 4.x development libraries (`wt`, `wthttp`)
- Boost libraries required by your Wt build
- OpenSSL and zlib
- `curl` or `wget` for the first libmseed vendoring step

## Wt installation

Wt 4.x must be installed separately; ITSV does not bundle it.
Download information: https://www.webtoolkit.eu/wt/download
Source repository: https://github.com/emweb/wt

To build a Wt 4.x checkout from source, install its documented CMake, Boost,
OpenSSL, and other dependencies, then run:

```bash
git clone --depth 1 --branch 4.14.4 https://github.com/emweb/wt.git
cmake -S wt -B wt/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build wt/build -j4
sudo cmake --install wt/build
sudo ldconfig
```

You can also install Wt development packages from your Linux distribution.
When Wt is installed under `/usr` rather than `/usr/local`, use
`make WT_PREFIX=/usr` when building ITSV.

## libmseed 2.19.8

ITSV builds `libmseed.a` locally from source. The first build downloads
the exact libmseed 2.19.8 upstream source archive, verifies its SHA-256 checksum,
and runs `make` inside `third_party/libmseed/` to build
`third_party/libmseed/libmseed.a`. The Makefile then statically links that
archive into the locally built ITSV program.

```bash
make vendor-libmseed
```

The build explicitly uses the `third_party/libmseed` directory.

## Build

If Wt is installed under `/usr/local`:

```bash
make -j4
```

If Wt is under `/usr`:

```bash
make -j4 WT_PREFIX=/usr
```

Run locally:

```bash
./run
```

Then open `http://127.0.0.1:8081/`.

## AddressSanitizer build

```bash
make -f Makefile.asan clean
make -f Makefile.asan -j4
make -f Makefile.asan run
```

The sanitizer build uses AddressSanitizer + UndefinedBehaviorSanitizer by
default and enables leak detection at runtime.

## Tests

The dependency-free model/navigation tests do not require Wt or libmseed:

```bash
make test
```

Sanitized core tests:

```bash
ITSV_SANITIZE=1 bash tests/run_tests.sh
```

For a **live Chrome test**, start ITSV, provide a representative multichannel
MiniSEED file, then run (requires Playwright and Pillow):

```bash
python3 -m pip install playwright pillow
python3 -m playwright install chromium
python3 tests/browser_smoke.py --file /path/to/recording.mseed --show
```

This verifies canvas sizing and the blue waveform background in **both** the
scrolling main display and fixed preview, plus navigation, panel resizing and
settings persistence in a real browser. It cannot be replaced by `make test`.


## Uniform waveform resizing and vertical scrolling

The main waveform pane uses a native vertical scrollbar and a single shared
row height for all channels (adjustable from 56 to 1600 px). By default,
**Fit waveform heights to window** distributes the available pane height
among the selected waveforms, reserving the time axis. If they cannot all fit
at 56 px, they keep that readable minimum and the waveform pane scrolls.
The shared height also adapts to browser and preview resizes. Drag the
horizontal divider under **any** channel to resize **all** waveforms equally;
that switches into fixed-height mode. Use **View > Fit waveform heights to
window** to resume automatic fitting, or **View > Reset waveform heights
(110 px)** for a fixed 110-pixel height. The preview and status remain fixed.
The current mode (`itsv.layout.autoFitRows`) and last manually chosen height
(`itsv.layout.rowHeight`) persist in browser `localStorage`. The time axis
remains at the end of the scrollable waveform list.

## Amplitude menu check marks

**Ampl > Auto**, **Fixed**, **Zero-centered (A0)**, **First trace (A1)** and
**All same (AA)** form one exclusive group: a check mark identifies the active
scaling mode. A new recording starts in Auto. Ctrl-mouse amplitude zoom,
**View > Expand/Contract vertical**, and the Up/Down keys switch to Fixed and
update the check mark immediately. Clicking the already-active item keeps it
checked. Clip and Invert remain separate actions, not scaling modes.

## Important behavioral details

- MiniSEED integer, float and double samples are retained internally as `double`;
  float/double traces are not rounded to integers.
- Non-finite samples are treated as gaps and ignored by amplitude scaling.
- Negative Unix timestamps are supported, so pre-1970 seismic data are valid.
- Original channel order follows the ordering delivered by libmseed.
- Loaded libmseed trace structures are freed after import; ITSV keeps only one
  owned waveform representation.
- The channel-selection dialog's right pane is informational text, not inactive
  checkboxes that appear clickable.
- `Clip to trace panel` controls actual amplitude clipping during rendering;
  extreme unclipped coordinates are bounded safely before conversion to pixels.
- MiniSEED files are checked against **256 MiB encoded input** and
  **30 million decoded samples**. The sample-count limit is checked from record
  metadata before unpacking; oversized uploads fail with an explanatory message.
- Overview amplitude is independently auto-fitted to the full channel range,
  even for channels with large DC offsets, and follows the Invert option.

## Runtime files and Wt resources

The ITSV source-only release contains `wt_config.xml`, but deliberately does
**not** bundle Wt's `resources/` directory or fonts. Wt requires those assets
at runtime. `./run` creates a local `resources` symlink to your installed Wt
resources directory, if one is not already present.

For a nonstandard Wt installation, specify the path to Wt's resources:

```bash
WT_RESOURCES_DIR=/path/to/Wt/resources ./run
```

## Publishing source code and license notices

Use `make dist-src` (Python 3 required) to create a source-only release archive,
without compiled objects, Wt assets/fonts or fetched libmseed files. The project
can be shared as original ITSV source under MIT with its copyright notice,
provided the third-party dependencies are documented separately.

**Important:** Wt is GPLv2-only (OpenSSL exception) and libmseed 2.19.8 is
LGPLv3-or-later. Those license versions are incompatible for distributing the
current **combined executable** under ordinary GPL/LGPL terms. Merely releasing
ITSV's original files as MIT source code does not resolve that conflict. Do not
distribute the resulting binary without first obtaining suitable licensing
permissions or replacing an incompatible dependency.

See `SOURCE_RELEASE.md` and `THIRD_PARTY_NOTICES.md` for complete details and
upstream license links. The **Help > About** dialog acknowledges Wt and libmseed.


## Layout customization

The View menu provides Small/Normal/Large font sizes, **Layout settings...**
and Reset layout. Drag the thin horizontal splitter above the preview to resize
its height, or drag the channel-information boundary to change panel width.
Font, panel width and preview height persist in browser `localStorage`.
See `LAYOUT_CUSTOMIZATION.md` for details and preview diagnostics.

## Source-code documentation

Classes, member functions, parameters, member fields, constants and core geometry
helpers have Doxygen-style comments in `src/`. These describe units, ownership,
index types, and the relationship between the data model and Wt widgets.

To generate an optional browsable API reference, install Doxygen and run:

```bash
doxygen Doxyfile
```

Open `docs/api/html/index.html` in a browser. Building ITSV does **not**
require Doxygen, and the generated HTML is not included in source releases.
