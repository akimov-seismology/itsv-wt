// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file ImportBudget.h
 *  @brief Upper bounds used to constrain MiniSEED imports before decoding.
 */
#pragma once

#include <cstdint>

namespace itsv {
namespace import_limits {
// MiniSEED is often compressed: cap both the encoded input and the metadata
// sample count before asking libmseed to unpack it into memory.
/// Maximum accepted compressed MiniSEED file size, in bytes.
constexpr std::uintmax_t maxInputBytes = 256ULL * 1024 * 1024;
/// Maximum decoded sample count across all channels.
constexpr std::uint64_t maxDecodedSamples = 30000000ULL;
/**
 * @brief Check whether adding samples stays under the decoding budget.
 * @param total Samples counted so far.
 * @param addition Additional samples reported by the next segment.
 * @return True if the sum is within the permitted limit.
 */
inline bool canAdd(std::uint64_t total, std::uint64_t addition) noexcept {
  return total <= maxDecodedSamples && addition <= maxDecodedSamples - total;
}
} // namespace import_limits
} // namespace itsv
