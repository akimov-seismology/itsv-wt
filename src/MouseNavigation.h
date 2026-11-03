// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file MouseNavigation.h
 *  @brief Numerically safe range calculations for mouse navigation.
 */
#pragma once

#include "Range.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace itsv::mouse {

namespace detail {
/**
 * @brief Convert high-precision interval bounds to a valid double range.
 * @param low Lower bound in extended precision.
 * @param high Upper bound in extended precision.
 * @return Converted range, or no value on overflow/invalid endpoints.
 */
inline std::optional<Range> checked(long double low, long double high)
{
  constexpr long double limit = std::numeric_limits<double>::max();
  if (!std::isfinite(low) || !std::isfinite(high) ||
      low < -limit || low > limit || high < -limit || high > limit)
    return std::nullopt;

  const Range result{static_cast<double>(low), static_cast<double>(high)};
  return validRange(result) ? std::optional<Range>(result) : std::nullopt;
}
} // namespace detail
/**
 * @brief Compute an inward or outward zoom from two normalized pointer positions.
 * @param current Current time or amplitude interval.
 * @param firstFraction Normalized position of first click in [0, 1].
 * @param secondFraction Normalized position of second click in [0, 1].
 * @param zoomOut True to zoom out, false to zoom in.
 * @return New range if positions and arithmetic are valid.
 */

inline std::optional<Range> zoom(Range current, double firstFraction,
                                 double secondFraction, bool zoomOut)
{
  if (!validRange(current) || !std::isfinite(firstFraction) ||
      !std::isfinite(secondFraction) || firstFraction < 0.0 ||
      firstFraction > 1.0 || secondFraction < 0.0 || secondFraction > 1.0)
    return std::nullopt;

  const long double first = std::min(firstFraction, secondFraction);
  const long double last = std::max(firstFraction, secondFraction);
  if (last <= first) return std::nullopt;

  const long double low = current.low;
  const long double span = static_cast<long double>(current.high) - low;
  if (!zoomOut)
    return detail::checked(low + first * span, low + last * span);

  const long double expandedSpan = span / (last - first);
  const long double expandedLow = low - first * expandedSpan;
  return detail::checked(expandedLow, expandedLow + expandedSpan);
}
/**
 * @brief Translate an interval without changing its span.
 * @param current Current interval.
 * @param delta Signed amount to add to both endpoints.
 * @return Shifted range, or no value on overflow.
 */

inline std::optional<Range> shift(Range current, double delta)
{
  if (!validRange(current) || !std::isfinite(delta)) return std::nullopt;
  if (delta == 0.0) return current;
  return detail::checked(static_cast<long double>(current.low) + delta,
                         static_cast<long double>(current.high) + delta);
}
/**
 * @brief Convert a horizontal mouse drag into a time-range translation.
 * @param current Current UTC time interval.
 * @param dx Mouse displacement in pixels.
 * @param width Horizontal plot width in pixels.
 * @return Panned range when valid.
 */

inline std::optional<Range> pan(Range current, double dx, double width)
{
  if (!validRange(current) || !std::isfinite(dx) || !std::isfinite(width) ||
      width <= 0.0)
    return std::nullopt;
  if (dx == 0.0) return current;
  const long double span = static_cast<long double>(current.high) - current.low;
  const long double delta = -(static_cast<long double>(dx) / width) * span;
  return detail::checked(static_cast<long double>(current.low) + delta,
                         static_cast<long double>(current.high) + delta);
}
/**
 * @brief Center the visible interval on a time while staying inside data bounds.
 * @param current Currently displayed time interval.
 * @param data Complete recording time interval.
 * @param center Preferred UTC center in epoch seconds.
 * @return Clamped visible interval when valid.
 */

inline std::optional<Range> centerInData(Range current, Range data, double center)
{
  if (!validRange(current) || !validRange(data) || !std::isfinite(center))
    return std::nullopt;

  const long double span = static_cast<long double>(current.high) - current.low;
  const long double dataLow = data.low;
  const long double dataHigh = data.high;
  if (span >= dataHigh - dataLow) return data;

  const long double low = std::clamp(static_cast<long double>(center) - span / 2,
                                     dataLow, dataHigh - span);
  return detail::checked(low, low + span);
}

} // namespace itsv::mouse
