# Mouse and keyboard controls

ITSV is a Linux/Wt waveform viewer. The interaction model is intentionally close
to the original IWV workflow while using browser-safe input handling.

## Time

- **Shift + left click**, then click again: zoom in to the selected time interval.
- **Shift + right click**, then click again: zoom out using the selected interval.
- **Ctrl + Shift + left click** is an alternative first click for time zoom out.
- **Plain left click** in a waveform: shift time forward by the clicked offset.
- **Plain right click**: shift time backward by the clicked offset.
- **Middle-button drag**: pan in time.
- **Left / Right arrow**: shift the time window.
- **Esc**: cancel a pending mouse operation.

## Amplitude

- **Ctrl + left click**, then click again: zoom in to an amplitude interval.
- **Ctrl + right click**, then click again: zoom out from an amplitude interval.
- **Up / Down arrow**: expand / contract amplitude.
- An explicit amplitude zoom freezes scaling in `Fixed` mode.

## Trace labels

- **Left click** a channel label: make that waveform the current single marked
  trace and show it in the lower overview.
- **Ctrl + left click**: add or remove that trace from the marked set.
- Left-drag over labels switches traces; Ctrl + left-drag toggles each label entered.
- The `Traces` menu can keep marked traces, hide marked traces, mark all, clear
  marks, or restore the original selected channel order.

## Lower overview

The overview displays the full extent of the current overview channel. Left-click
inside it to center the main view at that time while retaining the current view
width when possible.

## Layout controls

- Drag the thin gray handle above the full-waveform preview to change preview height (80-300 px).
- View > Layout settings... offers exact numeric widths and heights.
- Drag the vertical boundary at the right edge of the channel information panel to change its width.
- Use **View > Font size** for Small, Normal, or Large text.
- Use **View > Reset layout** to restore the defaults.
- Font size, information-panel width, and preview height are saved in browser `localStorage`.
