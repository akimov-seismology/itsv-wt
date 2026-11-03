// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file PreviewSelectionGeometry.h
 *  @brief Projection of the visible time interval into overview coordinates.
 */
#pragma once

#include "Range.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace itsv {

/**
 * @brief Horizontal span of the visible time window mapped into overview coordinates.
 * @details Clipped to the full recording interval. Long-double arithmetic
 * keeps the projection stable for large, valid UTC timestamps.
 */
struct PreviewTimeBand {
  /// Left x coordinate of the selected time band.
  double left;
  /// Right x coordinate of the selected time band.
  double right;
};
/**
 * @brief Map the visible UTC time window into the full-waveform overview.
 * @param full Complete recording UTC interval.
 * @param visible Current main display UTC interval.
 * @param plotLeft Left x coordinate of the overview plotting region.
 * @param plotWidth Overview plotting width in pixels.
 * @return Clipped overview x bounds, or no value if the inputs are unusable.
 */

inline std::optional<PreviewTimeBand> previewTimeBand(
    Range full, Range visible, double plotLeft, double plotWidth)
{
  if (!validRange(full) || !validRange(visible) ||
      !std::isfinite(plotLeft) || !std::isfinite(plotWidth) ||
      plotWidth < 8.0 || visible.high <= full.low || visible.low >= full.high)
    return std::nullopt;

  const long double start = static_cast<long double>(full.low);
  const long double duration = static_cast<long double>(full.high) - start;
  const auto fraction = [&](double time) {
    const long double offset = (static_cast<long double>(time) - start) / duration;
    return std::clamp(offset, 0.0L, 1.0L);
  };
  const double left = plotLeft + static_cast<double>(fraction(visible.low)) * plotWidth;
  const double right = plotLeft + static_cast<double>(fraction(visible.high)) * plotWidth;
  return PreviewTimeBand{left, right};
}

/**
 * @brief Inset outline rectangle drawn over the overview waveform.
 * @details Its inset top and bottom edges remain visible over the plot frame.
 */
struct PreviewSelectionRect {
  /// Left x coordinate of the selection outline.
  double x;
  /// Top y coordinate of the selection outline.
  double y;
  /// Width of the selection outline in pixels.
  double width;
  /// Height of the selection outline in pixels.
  double height;
};
/**
 * @brief Build a visible inset rectangle from the overview time band.
 * @param band Horizontal overview selection band.
 * @param plotLeft Left edge of the overview plot.
 * @param plotTop Top edge of the overview plot.
 * @param plotWidth Overview width in pixels.
 * @param plotHeight Overview height in pixels.
 * @return Inset selection outline, or no value if geometry is invalid.
 */

inline std::optional<PreviewSelectionRect> previewSelectionRect(
    PreviewTimeBand band, double plotLeft, double plotTop,
    double plotWidth, double plotHeight)
{
  if (!std::isfinite(plotLeft) || !std::isfinite(plotTop) ||
      !std::isfinite(plotWidth) || !std::isfinite(plotHeight) ||
      !std::isfinite(band.left) || !std::isfinite(band.right) ||
      plotWidth < 8.0 || plotHeight < 14.0 || band.right < band.left)
    return std::nullopt;

  const double minX = plotLeft + 2.0;
  const double maxX = plotLeft + plotWidth - 2.0;
  double x0 = std::clamp(band.left, minX, maxX);
  double x1 = std::clamp(band.right, minX, maxX);
  if (x1 - x0 < 3.0) {
    // A very narrow visible interval still needs a recognizable rectangle.
    const double center = 0.5 * (x0 + x1);
    x0 = std::clamp(center - 1.5, minX, maxX - 3.0);
    x1 = x0 + 3.0;
  }

  const double inset = std::clamp(plotHeight * 0.08, 4.0, 10.0);
  return PreviewSelectionRect{x0, plotTop + inset, x1 - x0,
                              plotHeight - 2.0 * inset};
}

} // namespace itsv
