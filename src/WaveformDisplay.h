// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file WaveformDisplay.h
 *  @brief Interactive waveform display widget and pointer state.
 */
#pragma once

#include "MouseNavigation.h"
#include "Recording.h"
#include "ViewState.h"
#include "UniformRows.h"

#include <Wt/WEvent.h>
#include <Wt/WPaintedWidget.h>
#include <Wt/WPainter.h>
#include <Wt/WSignal.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace itsv {

/**
 * @brief Interactive waveform canvas with channel labels, time ruler and mouse navigation.
 */
class WaveformDisplayWidget final : public Wt::WPaintedWidget {
public:
  /**
   * @brief Construct the interactive waveform canvas and connect mouse events.
   */
  WaveformDisplayWidget();

  /**
   * @brief Load a recording, resetting selected channels and view state.
   * @param recording Nonempty decoded waveform recording to take ownership of.
   */
  void setRecording(Recording recording);
  /**
   * @brief Return a pointer to the loaded recording, or null before import.
   */
  const Recording* recording() const noexcept { return recording_ ? &*recording_ : nullptr; }
  /**
   * @brief Access the current read-only view state.
   */
  const ViewState& viewState() const noexcept { return state_; }

  /**
   * @brief Apply a new ordered subset of recording channels.
   * @param channels Recording channel indices to display.
   */
  void setChannelSelection(std::vector<std::size_t> channels);
  /**
   * @brief Access channel indices selected in the channel dialog.
   */
  const std::vector<std::size_t>& channelSelection() const noexcept { return state_.baseChannels(); }

  /**
   * @brief Set the visible time range to the recording's full extent.
   */
  void fitToData();
  /**
   * @brief Narrow the visible time interval around its center.
   */
  void zoomTimeIn();
  /**
   * @brief Expand the visible time interval around its center.
   */
  void zoomTimeOut();
  /**
   * @brief Pan the time window toward earlier UTC times.
   */
  void shiftTimeLeft();
  /**
   * @brief Pan the time window toward later UTC times.
   */
  void shiftTimeRight();
  /**
   * @brief Increase vertical magnification of the displayed waveforms.
   */
  void expandAmplitude();
  /**
   * @brief Decrease vertical magnification of the displayed waveforms.
   */
  void contractAmplitude();

  /**
   * @brief Restore the original selected channel list and order.
   */
  void restoreTraces();
  /**
   * @brief Hide all currently unmarked channels.
   */
  void keepMarkedTraces();
  /**
   * @brief Hide the channels currently marked by the user.
   */
  void deleteMarkedTraces();
  /**
   * @brief Mark all visible channels for bulk operations.
   */
  void markAllTraces();
  /**
   * @brief Clear user marks on all channels.
   */
  void clearMarkedTraces();

  /**
   * @brief Scale each trace to its visible amplitude extrema.
   */
  void setAmplitudeAuto();
  /**
   * @brief Keep each trace's current amplitude limits unchanged during time navigation.
   */
  void setAmplitudeFixed();
  /**
   * @brief Choose a symmetric positive/negative range for each trace.
   */
  void setAmplitudeZeroCentered();
  /**
   * @brief Use the first visible trace's scale for all traces.
   */
  void setAmplitudeFirstTrace();
  /**
   * @brief Use a shared scale covering all visible traces.
   */
  void setAmplitudeAllSame();
  /**
   * @brief Toggle clipping at waveform panel boundaries.
   * @return New clipping state.
   */
  bool toggleClip();
  /**
   * @brief Invert displayed waveform polarity.
   */
  void toggleInvert();

  /**
   * @brief Cancel an incomplete zoom or drag gesture and remove overlays.
   */
  void cancelMouseGesture();
  /**
   * @brief Center the current time window on an overview time coordinate.
   * @param time Target UTC time in epoch seconds.
   */
  void centerTimeViewAt(double time);
  /**
   * @brief Emit a formatted UTC status message without changing the view.
   * @param time Target UTC time in epoch seconds.
   */
  void showTimeAt(double time);

  /**
   * @brief Return the overall available data time range.
   */
  std::optional<Range> dataRange() const noexcept { return state_.dataRange(); }
  /**
   * @brief Return the time interval shown in the waveform viewer.
   */
  std::optional<Range> viewRange() const noexcept { return state_.viewRange(); }
  /**
   * @brief Return the channel used for the overview.
   */
  std::optional<std::size_t> overviewChannel() const noexcept { return state_.overviewChannel(); }
  /**
   * @brief Return the information-panel width in pixels.
   */
  int labelPanelWidth() const noexcept { return labelPanelWidth_; }
  /**
   * @brief Return the waveform label size in pixels.
   */
  int fontSize() const noexcept { return fontSize_; }
  /**
   * @brief Return the height shared by every visible waveform row.
   */
  int rowHeight() const noexcept { return rowHeight_; }
  /**
   * @brief Return the time-ruler height in pixels.
   */
  int rulerHeight() const noexcept { return timeBarHeight_; }
  /**
   * @brief Compute total required content height for all rows and the ruler.
   */
  int minimumContentHeight() const noexcept;
  /**
   * @brief Choose a uniform row height that uses the available viewport.
   * @param viewportHeight Waveform viewport height in pixels.
   * @return Clamped height of each trace row.
   */
  int fittedRowHeight(int viewportHeight) const noexcept;
  /**
   * @brief Set the left channel-information panel width.
   * @param width Requested width in pixels (clamped to supported limits).
   */
  void setLabelPanelWidth(int width);
  /**
   * @brief Set text size used for waveform labels and time marks.
   * @param pixels Requested font size in pixels.
   */
  void setFontSize(int pixels);
  /**
   * @brief Set uniform waveform row height.
   * @param pixels Requested height in pixels, subject to minimum and maximum.
   */
  void setRowHeight(int pixels);

  /**
   * @brief Access the signal carrying status messages.
   */
  Wt::Signal<std::string>& statusText() noexcept { return statusText_; }
  /**
   * @brief Access the signal fired after the time/channel view changes.
   */
  Wt::Signal<>& viewChanged() noexcept { return viewChanged_; }
  /**
   * @brief Access the signal fired when loaded waveform data changes.
   */
  Wt::Signal<>& dataChanged() noexcept { return dataChanged_; }
  /**
   * @brief Access the signal fired after label-panel resizing.
   */
  Wt::Signal<int>& labelPanelWidthChanged() noexcept { return labelPanelWidthChanged_; }
  /**
   * @brief Access the signal fired after waveform row resizing.
   */
  Wt::Signal<int>& rowHeightChanged() noexcept { return rowHeightChanged_; }
  /**
   * @brief Access the signal fired after the scaling mode changes.
   */
  Wt::Signal<AmplitudeMode>& amplitudeModeChanged() noexcept { return amplitudeModeChanged_; }

protected:
  /**
   * @brief Paint the complete main canvas, including all traces, labels and ruler.
   * @param device Wt paint target for the current refresh.
   */
  void paintEvent(Wt::WPaintDevice* device) override;

private:
  /**
   * @brief Computed pixel and amplitude geometry for a single visible trace.
   */
  struct TraceLayout {
    /// Index of this row's channel in the recording.
    std::size_t channelIndex{0};
    /// Row number among the currently visible channels.
    std::size_t screenIndex{0};
    /// Top pixel of the full trace row.
    int panelTop{0};
    /// Bottom pixel of the full trace row.
    int panelBottom{0};
    /// Top of the trace's amplitude plotting region.
    int plotTop{0};
    /// Bottom of the trace's amplitude plotting region.
    int plotBottom{0};
    /// Vertical data bounds mapped to this row.
    Range amplitude{-1.0, 1.0};
  };

  /**
   * @brief Two-click gesture currently pending on the waveform canvas.
   */
  enum class MouseZoom {
    /// No zoom gesture in progress.
    None,
    /// Select a smaller time interval.
    TimeIn,
    /// Expand the time interval.
    TimeOut,
    /// Select a smaller amplitude interval.
    AmplitudeIn,
    /// Expand the amplitude interval.
    AmplitudeOut
  };

  /// Loaded waveform recording, absent until a file is opened.
  std::optional<Recording> recording_;
  /// Visible channels, zoom and amplitude state.
  ViewState state_;
  /// Cached paint-time layout of each visible channel row.
  std::vector<TraceLayout> layouts_;

  /// Width of the station/channel information area in pixels.
  int labelPanelWidth_{150};
  /// Text font size in pixels.
  int fontSize_{11};
  /// Uniform height in pixels shared by all visible traces.
  int rowHeight_{uniform_rows::defaultHeight};
  /// Reserved height of the bottom time ruler in pixels.
  int timeBarHeight_{64};
  /// Measured canvas width in pixels.
  int widgetWidth_{0};
  /// Measured canvas height in pixels.
  int widgetHeight_{0};
  /// First x coordinate of the waveform plotting area.
  int plotLeft_{0};
  /// Last x coordinate of the waveform plotting area.
  int plotRight_{0};

  /// Active two-click time or amplitude zoom mode.
  MouseZoom mouseZoom_{MouseZoom::None};
  /// Channel associated with an amplitude zoom gesture, if any.
  std::optional<std::size_t> zoomChannel_;
  /// Normalized position of the first zoom click.
  double zoomFirst_{0.0};
  /// Time or amplitude bounds captured when zoom began.
  Range zoomRange_{0.0, 1.0};
  /// True while a middle-button time pan is underway.
  bool middleDrag_{false};
  /// True while the label-panel divider is being dragged.
  bool labelResize_{false};
  /// True while a uniform row-height divider is dragged.
  bool rowResize_{false};
  /// One-based ordinal of the row boundary being dragged.
  int rowResizeDivider_{0};
  /// Shared row height at the start of the drag.
  int rowResizeInitialHeight_{uniform_rows::defaultHeight};
  /// Initial vertical pointer coordinate of a row-height drag.
  int rowResizeStartY_{0};
  /// Whether a channel-label selection gesture is active.
  bool panelSelectionActive_{false};
  /// Whether selection toggles marks instead of replacing selection.
  bool panelSelectionAdditive_{false};
  /// Last channel row visited during selection dragging.
  int lastPanel_{-1};
  /// Horizontal pointer coordinate at the start of panning.
  double panStartX_{0.0};
  /// Plot width used to convert drag distance to time.
  double panWidth_{0.0};
  /// View time interval at the beginning of a pan.
  Range panRange_{0.0, 1.0};
  /// Latest mouse x coordinate in canvas pixels.
  int mouseX_{0};
  /// Latest mouse y coordinate in canvas pixels.
  int mouseY_{0};
  /// Right edge of the previous time label, used to avoid overlap.
  int lastTimeLabelRight_{0};

  /// Emitted when pointer/navigation status text changes.
  Wt::Signal<std::string> statusText_;
  /// Emitted when the displayed time window or channel view changes.
  Wt::Signal<> viewChanged_;
  /// Emitted when the recording or selected data changes.
  Wt::Signal<> dataChanged_;
  /// Emitted after manual changes to the label-panel width.
  Wt::Signal<int> labelPanelWidthChanged_;
  /// Emitted after the uniform waveform row height changes.
  Wt::Signal<int> rowHeightChanged_;
  /// Emitted when the vertical scaling mode changes.
  Wt::Signal<AmplitudeMode> amplitudeModeChanged_;

  /**
   * @brief Refresh the viewer after time or amplitude state changes.
   */
  void refreshView();
  /**
   * @brief Set the time interval and emit update signals when accepted.
   * @param range New visible UTC interval.
   * @return Whether the state accepted the interval.
   */
  bool setViewRange(Range range);
  /**
   * @brief Rebuild per-channel view state after channel visibility changes.
   * @param dataChanged True when downstream caches must be invalidated.
   */
  void updateVisibleState(bool dataChanged);
  /**
   * @brief Compute amplitude limits according to the selected scaling mode.
   */
  void applyAutoScaling();
  /**
   * @brief Compute a channel's finite amplitude extrema in the current time window.
   * @param channelIndex Zero-based recording channel index.
   * @return Usable positive-width amplitude range.
   */
  Range channelAutoRange(std::size_t channelIndex) const;
  /**
   * @brief Capture current automatic scales before entering fixed scaling mode.
   */
  void freezeAmplitude();
  /**
   * @brief Activate a scaling mode and synchronize the menu via signal.
   * @param mode Requested amplitude scaling strategy.
   */
  void chooseAmplitudeMode(AmplitudeMode mode);
  /**
   * @brief Assign one channel's vertical scale and schedule repaint.
   * @param channelIndex Zero-based recording channel index.
   * @param range New amplitude bounds.
   */
  void setAmplitudeRange(std::size_t channelIndex, Range range);
  /**
   * @brief Report whether at least one channel is explicitly marked.
   */
  bool haveMarks() const noexcept { return !state_.markedChannels().empty(); }

  /**
   * @brief Compute row rectangles and amplitude bounds for painting.
   * @param device Paint device whose dimensions define the canvas.
   */
  void buildLayouts(Wt::WPaintDevice* device);
  /**
   * @brief Paint channel identifiers, extrema, and the information-panel background.
   * @param painter Active Wt painter.
   */
  void drawLeftPanel(Wt::WPainter& painter);
  /**
   * @brief Paint the bottom time ruler and tick labels.
   * @param painter Active Wt painter.
   */
  void drawTimePanel(Wt::WPainter& painter);
  /**
   * @brief Paint time-window boundary information along the time ruler.
   * @param painter Active Wt painter.
   */
  void drawTimeBounds(Wt::WPainter& painter);
  /**
   * @brief Paint one UTC time tick and its label.
   * @param x Horizontal tick position in pixels.
   * @param time Tick UTC timestamp in epoch seconds.
   * @param painter Active Wt painter.
   */
  void drawTimeMark(int x, double time, Wt::WPainter& painter);
  /**
   * @brief Paint the waveform envelope or samples of one visible trace.
   * @param layout Computed row geometry and amplitude bounds.
   * @param painter Active Wt painter.
   */
  void drawTrace(const TraceLayout& layout, Wt::WPainter& painter);
  /**
   * @brief Paint shared row boundaries and resize handles.
   * @param painter Active Wt painter.
   */
  void drawRowDividers(Wt::WPainter& painter);
  /**
   * @brief Show a lightweight browser-side two-click zoom overlay.
   * @param timeZoom True for horizontal time zoom; false for vertical amplitude.
   * @param zoomOut Whether the pending gesture enlarges rather than narrows the interval.
   * @param x Initial cursor x position.
   * @param y Initial cursor y position.
   */
  void startSelectionOverlay(bool timeZoom, bool zoomOut, int x, int y);
  /**
   * @brief Remove the browser-side zoom overlay.
   */
  void stopSelectionOverlay();
  /**
   * @brief Start visual feedback for horizontal information-panel resizing.
   * @param x Initial pointer x position.
   */
  void startLabelResizeOverlay(int x);
  /**
   * @brief Remove the label-panel resize guide.
   */
  void stopLabelResizeOverlay();
  /**
   * @brief Start visual feedback for a shared row-height resize.
   * @param y Initial pointer y position.
   * @param dividerOrdinal One-based row divider being dragged.
   */
  void startRowResizeOverlay(int y, int dividerOrdinal);
  /**
   * @brief Remove the waveform-height resize guide.
   */
  void stopRowResizeOverlay();

  /**
   * @brief Resolve a pointer position to a visible waveform row.
   * @param x Pointer x coordinate in canvas pixels.
   * @param y Pointer y coordinate in canvas pixels.
   * @param labels True to restrict hit testing to the label panel.
   * @return Zero-based display row index or -1 if no row is hit.
   */
  int screenAt(int x, int y, bool labels) const;
  /**
   * @brief Select or mark a waveform by its visible row.
   * @param screenIndex Zero-based display row index.
   * @param additive True to toggle a mark instead of selecting only one row.
   */
  void selectLabel(int screenIndex, bool additive);
  /**
   * @brief Convert the current pointer location to a normalized time or amplitude fraction.
   * @param x Pointer x coordinate.
   * @param y Pointer y coordinate.
   * @return Fraction between zero and one.
   */
  double zoomFraction(int x, int y) const;
  /**
   * @brief Capture the first click of a time/amplitude zoom gesture.
   * @param mode Requested gesture type.
   * @param channel Channel for vertical zoom, if applicable.
   * @param x Initial pointer x coordinate.
   * @param y Initial pointer y coordinate.
   */
  void beginZoom(MouseZoom mode, std::optional<std::size_t> channel, int x, int y);
  /**
   * @brief Apply a two-click zoom using the second pointer position.
   * @param x Second pointer x coordinate.
   * @param y Second pointer y coordinate.
   */
  void finishZoom(int x, int y);
  /**
   * @brief Publish UTC time and amplitude under the cursor.
   * @param x Pointer x coordinate.
   * @param y Pointer y coordinate.
   */
  void showCursorAt(int x, int y);

  /**
   * @brief Handle click modifiers, beginning selection, panning or resize.
   * @param event Wt mouse-button press event.
   */
  void onMouseDown(const Wt::WMouseEvent& event);
  /**
   * @brief Complete mouse-driven selection and resize interactions.
   * @param event Wt mouse-button release event.
   */
  void onMouseUp(const Wt::WMouseEvent& event);
  /**
   * @brief Update hover readout and mouse gesture state.
   * @param event Wt mouse-motion event.
   */
  void onMouseMove(const Wt::WMouseEvent& event);
  /**
   * @brief Handle keyboard time/amplitude navigation and Escape.
   * @param event Wt key-press event.
   */
  void onKeyDown(const Wt::WKeyEvent& event);
};

} // namespace itsv
