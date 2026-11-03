// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file UniformRows.h
 *  @brief Shared waveform-row sizing, auto-fit and divider dragging.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <limits>

namespace itsv::uniform_rows {

/// Initial manual waveform row height, in pixels.
constexpr int defaultHeight = 110;
/// Smallest row height that retains readable labels.
constexpr int minimumHeight = 56; // Fits the channel name and both amplitude labels
/// Largest row height allowed to protect painting performance.
constexpr int maximumHeight = 1600; // Avoid unbounded browser paint surfaces

// Choose a shared row height that fills the main waveform viewport, leaving
// room for the time ruler. If too many traces are visible, use the minimum
// readable height and let the parent viewport scroll vertically.  Integer
// division intentionally avoids an unnecessary 1-pixel scrollbar. When the
// unclamped ideal height fits, at most (rows - 1) unused pixels remain.
/**
 * @brief Choose a common waveform-row height that fills available viewport space.
 * @param rows Number of visible waveform rows.
 * @param viewportHeight Usable scroll viewport height in pixels.
 * @param timeRulerHeight Height reserved for the bottom time ruler.
 * @return Clamped uniform row height; minimum height implies scrolling if needed.
 */
inline int fitHeight(std::size_t rows, int viewportHeight, int timeRulerHeight) noexcept
{
  if (rows == 0) return defaultHeight;
  const int available = std::max(0, std::max(0, viewportHeight) -
                                            std::max(0, timeRulerHeight));
  const std::size_t ideal = static_cast<std::size_t>(available) / rows;
  if (ideal < static_cast<std::size_t>(minimumHeight)) return minimumHeight;
  if (ideal > static_cast<std::size_t>(maximumHeight)) return maximumHeight;
  return static_cast<int>(ideal);
}
/**
 * @brief Constrain a row height to readable and safe painting limits.
 * @param pixels Requested row height in pixels.
 * @return Height between minimumHeight and maximumHeight.
 */

inline int clampHeight(int pixels) noexcept
{
  return std::clamp(pixels, minimumHeight, maximumHeight);
}

// A single row-height value is used for *every* waveform. The time ruler is
// below the last row and remains part of the scrollable waveform drawing.
/**
 * @brief Calculate scrollable waveform content height including the ruler.
 * @param rows Number of visible waveform rows.
 * @param rowHeight Uniform waveform row height in pixels.
 * @param timeRulerHeight Height of the time ruler in pixels.
 * @return Total pixel height, saturating at the maximum signed integer.
 */
inline int contentHeight(std::size_t rows, int rowHeight, int timeRulerHeight) noexcept
{
  const std::size_t height = static_cast<std::size_t>(clampHeight(rowHeight));
  const std::size_t ruler = static_cast<std::size_t>(std::max(0, timeRulerHeight));
  const std::size_t cap = static_cast<std::size_t>(std::numeric_limits<int>::max());
  if (rows > (cap - ruler) / height) return std::numeric_limits<int>::max();
  return static_cast<int>(rows * height + ruler);
}

// Return the 1-based ordinal of a row divider when the cursor is near it;
// include the bottom edge of the final row to support a single visible trace.
/**
 * @brief Find a waveform row divider near a pointer position.
 * @param y Vertical pointer coordinate in waveform canvas.
 * @param rowCount Number of visible rows.
 * @param rowHeight Uniform height of one row.
 * @param tolerance Maximum number of pixels from a divider to count as a hit.
 * @return One-based divider ordinal or zero when no divider is hit.
 */
inline int dividerAt(int y, std::size_t rowCount, int rowHeight, int tolerance = 5) noexcept
{
  if (y < 0 || rowCount == 0 || tolerance < 0) return 0;
  const int height = clampHeight(rowHeight);
  const int ordinal = static_cast<int>(std::lround((static_cast<double>(y) + 1.0) / height));
  if (ordinal < 1 || static_cast<std::size_t>(ordinal) > rowCount) return 0;
  const long long boundary = static_cast<long long>(ordinal) * height - 1;
  return std::llabs(static_cast<long long>(y) - boundary) <= tolerance ? ordinal : 0;
}

// The divider actually grabbed stays under the pointer: divider k moves by
// k*(newHeight-oldHeight) when all rows resize uniformly.
/**
 * @brief Determine a shared row height after dragging a particular divider.
 * @param originalHeight Uniform row height at drag start.
 * @param dividerOrdinal One-based index of the grabbed divider.
 * @param deltaPixels Vertical pointer movement since drag start.
 * @return New clamped uniform row height.
 */
inline int heightFromDrag(int originalHeight, int dividerOrdinal, int deltaPixels) noexcept
{
  if (dividerOrdinal < 1) return clampHeight(originalHeight);
  const double newHeight = static_cast<double>(clampHeight(originalHeight)) +
                           static_cast<double>(deltaPixels) / dividerOrdinal;
  // Check the limits before narrowing to int: extreme mouse deltas must never
  // overflow the conversion and unexpectedly shrink a large waveform.
  if (newHeight <= minimumHeight) return minimumHeight;
  if (newHeight >= maximumHeight) return maximumHeight;
  return static_cast<int>(std::lround(newHeight));
}

} // namespace itsv::uniform_rows
