// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file MiniSeedReader.h
 *  @brief MiniSEED file loader and load-result types.
 */
#pragma once

#include "Recording.h"

#include <string>
#include <vector>

namespace itsv {

/**
 * @brief Decoded waveform recording together with nonfatal import warnings.
 */
struct MiniSeedLoadResult {
  /// Successfully decoded channels and samples.
  Recording recording;
  /// Nonfatal issues encountered during decoding.
  std::vector<std::string> warnings;
};

/**
 * @brief Reads MiniSEED waveform files into the ITSV in-memory recording model.
 */
class MiniSeedReader {
public:
  /**
   * @brief Decode a MiniSEED file, enforcing import size/sample limits.
   * @param fileName Path to an existing MiniSEED file.
   * @return Recording and nonfatal warnings; throws on fatal decode errors.
   */
  static MiniSeedLoadResult read(const std::string& fileName);
};

} // namespace itsv
