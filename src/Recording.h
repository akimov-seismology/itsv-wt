// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file Recording.h
 *  @brief Waveform recording, channel, segment and sample-envelope data types.
 */
#pragma once

#include "Range.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <string>
#include <vector>

namespace itsv {

/**
 * @brief SEED network, station, location, and channel identifiers for one trace.
 */
struct ChannelId {
  /// SEED network code.
  std::string network;
  /// Station code.
  std::string station;
  /// Location identifier (may be empty).
  std::string location;
  /// SEED channel code.
  std::string channel;

  /**
   * @brief Return the dot-separated network.station.location.channel identifier.
   * @return SEED-style channel name.
   */
  std::string seedName() const;
  /**
   * @brief Return a compact station/channel label for the waveform panel.
   * @return Human-readable station/channel label.
   */
  std::string displayName() const;
};

/**
 * @brief Minimum and maximum sample values assigned to one horizontal overview pixel.
 */
struct EnvelopeBin {
  /// Lowest finite sample value in this bin or block.
  double low{0.0};
  /// Highest finite sample value in this bin or block.
  double high{0.0};
  /// True if at least one finite sample contributed.
  bool present{false};
};

/**
 * @brief One contiguous, uniformly sampled waveform segment with cached extrema.
 */
class Segment {
public:
  /**
   * @brief Construct a default, empty segment placeholder.
   */
  Segment() = default;
  /**
   * @brief Construct a segment and precompute extrema caches.
   * @param startTime UTC timestamp of the first sample, in epoch seconds.
   * @param sampleInterval Positive sample spacing in seconds.
   * @param samples Decoded amplitude samples; must be nonempty.
   */
  Segment(double startTime, double sampleInterval, std::vector<double> samples);

  /**
   * @brief Return the first sample time in UTC epoch seconds.
   */
  double startTime() const noexcept { return startTime_; }
  /**
   * @brief Return the sample spacing in seconds.
   */
  double sampleInterval() const noexcept { return sampleInterval_; }
  /**
   * @brief Return the time of the last sample in UTC epoch seconds.
   */
  double endTime() const noexcept;
  /**
   * @brief Access the immutable decoded sample array.
   */
  const std::vector<double>& samples() const noexcept { return samples_; }
  /**
   * @brief Return the number of decoded samples.
   */
  std::size_t sampleCount() const noexcept { return samples_.size(); }

  /**
   * @brief Return finite amplitude extrema for the full segment or channel.
   * @return Absent if no finite amplitude samples are available.
   */
  std::optional<Range> fullRange() const noexcept;
  /**
   * @brief Find sample indices overlapping a UTC time interval.
   * @param timeRange Requested UTC time interval.
   * @return Inclusive first/last indices, or no value if disjoint.
   */
  std::optional<std::pair<std::size_t, std::size_t>> sampleIndexRange(Range timeRange) const noexcept;
  /**
   * @brief Find finite amplitude extrema inside a requested time interval.
   * @param timeRange UTC interval to inspect.
   * @return Amplitude range if finite samples overlap.
   */
  std::optional<Range> rangeInTime(Range timeRange) const noexcept;
  /**
   * @brief Merge this segment's samples into a pixel-wise min/max envelope.
   * @param bounds Full UTC time interval represented by all bins.
   * @param bins Mutable envelope columns receiving this segment's extrema.
   */
  void addToEnvelope(Range bounds, std::vector<EnvelopeBin>& bins) const;

private:
  /// Number of samples represented by one extrema cache block.
  static constexpr std::size_t kBlockSize = 256;

  /**
   * @brief Cached finite extrema for a fixed-size portion of a segment.
   */
  struct BlockExtrema {
    /// Lowest finite sample value in this bin or block.
    double low{0.0};
    /// Highest finite sample value in this bin or block.
    double high{0.0};
    /// True if at least one finite sample contributed.
    bool present{false};
  };

  /// UTC start time in seconds since the Unix epoch.
  double startTime_{0.0};
  /// Time between adjacent samples, in seconds.
  double sampleInterval_{1.0};
  /// Original decoded sample values, in acquisition order.
  std::vector<double> samples_;
  /// Min/max cache used to accelerate range queries and overview plots.
  std::vector<BlockExtrema> blocks_;
  /// Cached finite sample-value range; absent when no finite samples exist.
  std::optional<Range> fullRange_;

  /**
   * @brief Build per-block and full-segment finite extrema caches.
   */
  void buildExtrema();
};

/**
 * @brief A named seismic channel composed of one or more time segments.
 */
struct Channel {
  /// Network/station/location/channel identity.
  ChannelId id;
  /// Time segments belonging to this channel.
  std::vector<Segment> segments;

  /**
   * @brief Return finite amplitude extrema for the full segment or channel.
   * @return Absent if no finite amplitude samples are available.
   */
  std::optional<Range> fullRange() const noexcept;
  /**
   * @brief Find finite amplitude extrema inside a requested time interval.
   * @param timeRange UTC interval to inspect.
   * @return Amplitude range if finite samples overlap.
   */
  std::optional<Range> rangeInTime(Range timeRange) const noexcept;
  /**
   * @brief Compute the min/max envelope used by the lower overview plot.
   * @param bounds UTC time interval to render.
   * @param columns Number of horizontal pixel bins.
   * @return Per-column extrema, including absent-data flags.
   */
  std::vector<EnvelopeBin> overviewEnvelope(Range bounds, int columns) const;
};

/**
 * @brief Collection of channels and their combined absolute UTC time interval.
 */
class Recording {
public:
  /**
   * @brief Construct an empty recording without a time interval.
   */
  Recording() = default;
  /**
   * @brief Construct a recording and derive its overall time range.
   * @param channels Ordered decoded seismic channels.
   */
  explicit Recording(std::vector<Channel> channels);

  /**
   * @brief Report whether any channels are loaded.
   */
  bool empty() const noexcept { return channels_.empty(); }
  /**
   * @brief Return the number of loaded channels.
   */
  std::size_t channelCount() const noexcept { return channels_.size(); }
  /**
   * @brief Retrieve a channel by its recording index.
   * @param index Zero-based channel index.
   * @return Read-only reference; throws if the index is invalid.
   */
  const Channel& channel(std::size_t index) const { return channels_.at(index); }
  /**
   * @brief Access all channels in recording order.
   */
  const std::vector<Channel>& channels() const noexcept { return channels_; }
  /**
   * @brief Access the recording's overall UTC time extent.
   */
  const std::optional<Range>& timeRange() const noexcept { return timeRange_; }

private:
  /// Channels held by this recording in display/import order.
  std::vector<Channel> channels_;
  /// Overall UTC bounds of the recording.
  std::optional<Range> timeRange_;

  /**
   * @brief Recalculate the UTC bounds across every segment of every channel.
   */
  void recomputeTimeRange();
};

} // namespace itsv
