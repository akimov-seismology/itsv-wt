// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file PlotGeometry.h
 *  @brief Bounded conversion from waveform time and amplitude to plot pixels.
 */
#pragma once

#include "Range.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace itsv {
namespace geometry {

// Compute a pixel coordinate in long double before converting to int. A
// narrow fixed amplitude interval and a large sample can otherwise overflow
// lround()/int even when the sample and limits are individually finite.
/**
 * @brief Map an amplitude sample to a safe vertical pixel coordinate.
 * @param value Sample amplitude.
 * @param amplitude Displayed vertical amplitude interval.
 * @param top Top of the plot area in pixels.
 * @param bottom Bottom of the plot area in pixels.
 * @param clipToPanel True to confine the sample to its trace panel.
 * @param deviceHeight Full canvas height used for safety bounds.
 * @return Integer vertical coordinate safe for painting.
 */
inline int amplitudeY(double value, Range amplitude, int top, int bottom,
                      bool clipToPanel, int deviceHeight) noexcept
{
  if (!std::isfinite(value) || !validRange(amplitude) || bottom <= top)
    return top;
  const long double v = static_cast<long double>(value);
  const long double lo = static_cast<long double>(amplitude.low);
  const long double hi = static_cast<long double>(amplitude.high);
  const long double fraction = (v - lo) / (hi - lo);
  const long double y = static_cast<long double>(bottom) -
      fraction * static_cast<long double>(bottom - top);
  // With clipping disabled, crossing another trace panel is allowed but
  // coordinates far beyond the canvas never need to reach the paint device.
  const long double lower = clipToPanel ? top : -4096.0L;
  const long double upper = clipToPanel ? bottom :
      static_cast<long double>(std::max(0, deviceHeight)) + 4096.0L;
  if (!std::isfinite(y)) return y < 0 ? static_cast<int>(lower) : static_cast<int>(upper);
  return static_cast<int>(std::lround(std::clamp(y, lower, upper)));
}
/**
 * @brief Map a UTC time to a bounded horizontal plot coordinate.
 * @param time Time in UTC epoch seconds.
 * @param view Visible UTC time interval.
 * @param left Left edge of the plot in pixels.
 * @param right Right edge of the plot in pixels.
 * @return Horizontal pixel coordinate within the plotted range.
 */

inline int timeX(double time, Range view, int left, int right) noexcept
{
  if (!std::isfinite(time) || !validRange(view) || right <= left) return left;
  const long double fraction =
      (static_cast<long double>(time) - view.low) /
      (static_cast<long double>(view.high) - view.low);
  const long double pixel = static_cast<long double>(left) +
      fraction * static_cast<long double>(right - left);
  if (!std::isfinite(pixel)) return pixel < 0 ? left : right;
  return static_cast<int>(std::lround(std::clamp(
      pixel, static_cast<long double>(left), static_cast<long double>(right))));
}

} // namespace geometry
} // namespace itsv
