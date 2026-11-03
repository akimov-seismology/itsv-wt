# Validation and runtime verification

The dependency-free C++ test suite checks numeric ranges, waveform
recording logic, navigation, import limits, uniform row geometry, and
per-pixel envelopes. It includes deterministic and reproducible randomized
tests.

These tests do not build Wt, libmseed, or the browser UI. Run the full Wt
application and browser tests on a Linux host with those dependencies installed.

To build and test on that host:

```bash
make -j4
bash tests/run_tests.sh
./run
# In another terminal, after installing Playwright and Pillow:
python3 tests/browser_smoke.py --file /path/to/multichannel.mseed --show
```

The browser smoke test checks waveform rendering in both the main view and
overview, client/backing canvas sizing, channel selection, navigation,
resizable panels, scrolling, amplitude menus, and browser preference storage.
It requires an installed Wt application, a running server, and a multichannel
MiniSEED test file. Unit tests alone cannot validate Wt/browser layout behavior.

`Help > Preview diagnostics` can report backing and display dimensions to
Chrome's developer console when diagnosing rendering issues.
