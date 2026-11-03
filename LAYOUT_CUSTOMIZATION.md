# ITSV layout settings

The **View > Layout settings...** dialog offers four numeric controls:

- **Font size:** 10–12 px. View also offers Small / Normal / Large shortcuts.
- **Left information-panel width:** 100–400 px. Drag the vertical border between the gray channel labels and the waveform; the cursor changes to `col-resize` near the border.
- **Waveform row height:** 56–1600 px, identical for all traces. Changing this field disables automatic viewport fitting.
- **Preview height:** 80–300 px. Drag the **thin gray horizontal splitter** immediately above the overview; the guide follows the pointer and the overview repaints once on release.

**View > Reset layout** restores 11 px font, 150 px left panel, 145 px preview, and **Fit waveform heights to window** (on by default). When the fit option is enabled, row heights adapt to the remaining main pane height but never shrink below 56 px; excess waveforms scroll vertically. Manually dragging a row boundary turns fitting off. The last manual height is restored when fixed mode is active.

All five settings persist in the current browser's **localStorage**, not cookies:
`itsv.layout.fontSize`, `itsv.layout.labelWidth`, `itsv.layout.previewHeight`,
`itsv.layout.autoFitRows`, and `itsv.layout.rowHeight` (manual height only).
Settings persist across browser/server restarts for the same origin
(`http://127.0.0.1:8081`). Clearing browser site data resets them.

The overview has a layout-aware width (`WLength::Auto`) rather than a
fixed-width drawing canvas. Do not replace it with a fixed pixel width, a
`width:100%` CSS stretch, or `WBoxLayout::setResizable()`: each risks
mismatching the Wt canvas backing dimensions and the actual layout width.

**Help > Preview diagnostics (Chrome console)** prints the visible and backing
sizes of the preview canvas/SVG. In Chrome, press F12 and select Console before
using this command.
