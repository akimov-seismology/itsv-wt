# ITSV Linux Architecture

## Goals

- Linux-only C++17/Wt application.
- No Windows/MFC compatibility layer.
- One owner for waveform samples.
- Transactional MiniSEED decoding before replacing the current recording.
- Preserve MiniSEED channel order.
- Separate data, view state and rendering.
- Make memory ownership explicit through values/RAII.

## Data layer

### `Recording`

Owns an ordered `std::vector<Channel>` and the recording time range.

### `Channel`

Contains a `ChannelId` and ordered waveform `Segment`s.

### `Segment`

Owns `std::vector<double>` samples, start time and sample interval. It also
builds 256-sample min/max blocks. Auto-scaling can therefore query large time
windows without rescanning every sample.

NaNs/infinities remain in the sample vector as gaps but are excluded from
min/max calculations.

### `MiniSeedReader`

The only component that includes libmseed. It decodes into temporary RAII-owned
libmseed structures, copies numeric data once into `Recording`, and frees the
libmseed trace list before returning. Unsupported/non-waveform segments produce
warnings instead of half-created channels.

## View layer

### `ViewState`

Owns:

- base channel selection from the channel dialog;
- current visible trace order;
- marked traces in click order;
- overview channel;
- data and visible time ranges;
- per-channel amplitude ranges;
- amplitude scaling mode;
- inversion and clipping state.

Trace commands operate only on `ViewState`; they never delete waveform data.
`Original traces/order` restores the base channel selection in its original
recording order.

## Wt layer

### `WaveformDisplayWidget`

Renders the current `Recording` using `ViewState`, owns mouse/keyboard gesture
state, and emits status/view/data signals. Trace geometry is calculated directly
in top-down Wt coordinates; there is no legacy window-coordinate object.

### `WaveformPreviewWidget`

Renders a cached pixel-envelope of the overview channel and centers the main
view on clicks.

### `ChannelSelectionDialog`

Edits a local set of recording channel indices. `OK` commits once; Cancel does
nothing. The selected-channel pane is read-only text rather than disconnected
checkbox widgets.

## Error handling

- A new `Recording` is constructed completely before `setRecording()` commits it.
- Invalid sample rates/times are skipped with warnings.
- Unsupported MiniSEED sample types are skipped with warnings.
- Files with no supported numeric waveform data fail with an exception.
- Index-based APIs validate indices before mutating state.

## Performance

- Samples are copied once from libmseed.
- Segment min/max block caches accelerate auto-scaling.
- Main trace drawing preserves per-pixel min/max envelopes so narrow spikes are
  visible when zoomed out.
- Preview pixel envelopes are cached until recording/channel/width changes.
