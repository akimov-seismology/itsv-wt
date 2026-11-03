// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file WaveformPreview.h
 *  @brief Full-recording waveform overview widget and selection cache.
 */
#pragma once

#include "WaveformDisplay.h"

#include <Wt/WPaintedWidget.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace itsv {

/**
 * @brief Full-recording overview with a rectangle indicating the main time selection.
 */
class WaveformPreviewWidget final : public Wt::WPaintedWidget {
public:
  /**
   * @brief Construct the overview and connect pointer navigation to the main viewer.
   * @param display Main waveform viewer; must outlive this widget.
   */
  explicit WaveformPreviewWidget(WaveformDisplayWidget& display);
  /**
   * @brief Discard cached envelope data so the next draw recomputes it.
   */
  void invalidateCache();
  /**
   * @brief Choose the size of overview time labels.
   * @param pixels Requested font size in pixels.
   */
  void setFontSize(int pixels);

protected:
  /**
   * @brief Draw the selected channel's full waveform and selected time window.
   * @param device Wt drawing target.
   */
  void paintEvent(Wt::WPaintDevice* device) override;

private:
  /// Source waveform viewer; this reference is not owned.
  WaveformDisplayWidget& display_;
  /// Cached per-column min/max envelope for the selected channel.
  std::vector<EnvelopeBin> bins_;
  /// Minimum amplitude of the cached envelope.
  double minValue_{0.0};
  /// Maximum amplitude of the cached envelope.
  double maxValue_{0.0};
  /// Index of the channel currently represented by the cache.
  std::optional<std::size_t> cachedChannel_;
  /// Number of horizontal plot columns used to construct the cache.
  int cachedColumns_{0};
  /// Start UTC time covered by the cache.
  double cachedStart_{0.0};
  /// End UTC time covered by the cache.
  double cachedEnd_{0.0};

  /// Left edge of the interactive overview plot.
  int plotLeft_{0};
  /// Top edge of the interactive overview plot.
  int plotTop_{0};
  /// Overview waveform width in pixels.
  int plotWidth_{0};
  /// Overview waveform height in pixels.
  int plotHeight_{0};
  /// Whether plot geometry is ready for pointer events.
  bool plotReady_{false};
  /// Whether a primary-button gesture began inside the plot.
  bool leftPressedInPlot_{false};
  /// Label text size in pixels.
  int fontSize_{11};

  /**
   * @brief Recompute the min/max envelope for a given channel and viewport width.
   * @param channelIndex Selected recording channel index.
   * @param columns Number of horizontal bins.
   * @param start UTC start of the full data interval.
   * @param end UTC end of the full data interval.
   */
  void rebuildCache(std::size_t channelIndex, int columns, double start, double end);
  /**
   * @brief Convert an overview mouse event to UTC time.
   * @param event Mouse event in overview coordinates.
   * @param time Output UTC time in epoch seconds when successful.
   * @return True if the pointer maps to a valid time.
   */
  bool timeAt(const Wt::WMouseEvent& event, double& time) const;
  /**
   * @brief Start overview pointer interaction.
   * @param event Mouse-button press event.
   */
  void onMouseDown(const Wt::WMouseEvent& event);
  /**
   * @brief Finish overview pointer interaction and update main time selection.
   * @param event Mouse-button release event.
   */
  void onMouseUp(const Wt::WMouseEvent& event);
  /**
   * @brief Update overview pointer interaction as the cursor moves.
   * @param event Mouse-motion event.
   */
  void onMouseMove(const Wt::WMouseEvent& event);
};

} // namespace itsv
