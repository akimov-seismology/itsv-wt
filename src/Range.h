// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file Range.h
 *  @brief Numeric intervals and safe amplitude-range utilities.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace itsv {

/**
 * @brief Finite numeric interval used for UTC time or waveform amplitude.
 */
struct Range {
  /// Inclusive lower endpoint of the numeric interval.
  double low{0.0};
  /// Upper endpoint, greater than low for a valid range.
  double high{0.0};
};
/**
 * @brief Check that both endpoints are finite and the interval has positive finite width.
 * @param r Range being validated.
 * @return True for a usable numeric interval.
 */

inline bool validRange(const Range& r) noexcept
{
  return std::isfinite(r.low) && std::isfinite(r.high) && r.high > r.low &&
         std::isfinite(r.high - r.low);
}
/**
 * @brief Create a finite nonzero-width interval around a single amplitude value.
 * @param value Center value, or a fallback trigger if nonfinite.
 * @return An interval suitable for plotting.
 */

inline Range widenedRange(double value) noexcept
{
  if (!std::isfinite(value)) return {-1.0, 1.0};
  const double pad = std::max(1.0, std::abs(value) * 1.0e-6);
  const double limit = std::numeric_limits<double>::max();
  double low = value <= -limit + pad ? -limit : value - pad;
  double high = value >= limit - pad ? limit : value + pad;
  // Ensure both endpoints are representable and their difference is finite.
  if (!(high > low)) {
    low = std::nextafter(value, -std::numeric_limits<double>::infinity());
    high = std::nextafter(value, std::numeric_limits<double>::infinity());
    if (!std::isfinite(low)) low = value;
    if (!std::isfinite(high)) high = value;
  }
  if (!std::isfinite(high - low)) {
    // The input is finite, so a one-sided interval at this point is safe.
    if (value >= 0.0) low = std::nextafter(value, -std::numeric_limits<double>::infinity());
    else high = std::nextafter(value, std::numeric_limits<double>::infinity());
    if (value >= 0.0) high = value;
    else low = value;
  }
  return {low, high};
}

} // namespace itsv
