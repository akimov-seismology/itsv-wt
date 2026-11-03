// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
#include "Recording.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace itsv {
namespace {

/**
 * @brief Extend finite extrema with one amplitude sample.
 * @param target Optional min/max accumulator updated in place.
 * @param value Amplitude sample to include when finite.
 */
void merge(std::optional<Range>& target, double value)
{
  if (!std::isfinite(value)) return;
  if (!target) target = Range{value, value};
  else {
    target->low = std::min(target->low, value);
    target->high = std::max(target->high, value);
  }
}

/**
 * @brief Merge two optional amplitude ranges.
 * @param target Optional min/max accumulator updated in place.
 * @param source Additional range, ignored if absent.
 */
void merge(std::optional<Range>& target, const std::optional<Range>& source)
{
  if (!source) return;
  merge(target, source->low);
  merge(target, source->high);
}

/**
 * @brief Convert a singleton extrema range into a nonzero plotting interval.
 * @param r Optional finite extrema range.
 * @return Usable amplitude interval, or no value if absent.
 */
std::optional<Range> normalize(std::optional<Range> r)
{
  if (!r) return std::nullopt;
  if (r->high > r->low) return r;
  return widenedRange(r->low);
}

} // namespace

std::string ChannelId::seedName() const
{
  return network + "." + station + "." + location + "." + channel;
}

std::string ChannelId::displayName() const
{
  std::string name = station + "::" + channel;
  if (!location.empty()) name += "." + location;
  return name;
}

Segment::Segment(double startTime, double sampleInterval, std::vector<double> samples)
  : startTime_(startTime), sampleInterval_(sampleInterval), samples_(std::move(samples))
{
  if (!std::isfinite(startTime_))
    throw std::invalid_argument("Segment start time must be finite");
  if (!std::isfinite(sampleInterval_) || !(sampleInterval_ > 0.0))
    throw std::invalid_argument("Segment sample interval must be positive and finite");
  if (samples_.empty())
    throw std::invalid_argument("Segment cannot be empty");
  const long double end = static_cast<long double>(startTime_) +
      static_cast<long double>(sampleInterval_) * static_cast<long double>(samples_.size() - 1);
  if (!std::isfinite(end) || end < -std::numeric_limits<double>::max() ||
      end > std::numeric_limits<double>::max())
    throw std::invalid_argument("Segment end time is outside the representable range");
  buildExtrema();
}

double Segment::endTime() const noexcept
{
  return startTime_ + sampleInterval_ * static_cast<double>(samples_.size() - 1);
}

void Segment::buildExtrema()
{
  blocks_.assign((samples_.size() + kBlockSize - 1) / kBlockSize, BlockExtrema{});
  std::optional<Range> full;
  for (std::size_t i = 0; i < samples_.size(); ++i) {
    const double value = samples_[i];
    if (!std::isfinite(value)) continue;
    auto& block = blocks_[i / kBlockSize];
    if (!block.present) {
      block.low = block.high = value;
      block.present = true;
    } else {
      block.low = std::min(block.low, value);
      block.high = std::max(block.high, value);
    }
    merge(full, value);
  }
  fullRange_ = normalize(full);
}

std::optional<Range> Segment::fullRange() const noexcept
{
  return fullRange_;
}

std::optional<std::pair<std::size_t, std::size_t>>
Segment::sampleIndexRange(Range timeRange) const noexcept
{
  if (!validRange(timeRange) || samples_.empty()) return std::nullopt;
  const double lastTime = endTime();
  if (lastTime < timeRange.low || startTime_ > timeRange.high) return std::nullopt;

  const long double firstPosition =
      (static_cast<long double>(timeRange.low) - startTime_) / sampleInterval_;
  const long double lastPosition =
      (static_cast<long double>(timeRange.high) - startTime_) / sampleInterval_;
  if (!std::isfinite(firstPosition) || !std::isfinite(lastPosition)) return std::nullopt;

  const long double maxIndex = static_cast<long double>(samples_.size() - 1);
  if (lastPosition < 0.0L || firstPosition > maxIndex) return std::nullopt;
  const long double clampedFirst = std::max(0.0L, firstPosition);
  const long double clampedLast = std::min(maxIndex, lastPosition);
  const auto first = static_cast<std::size_t>(std::ceil(clampedFirst));
  const auto last = static_cast<std::size_t>(std::floor(clampedLast));
  if (last < first || first >= samples_.size()) return std::nullopt;
  return std::pair<std::size_t, std::size_t>{first, last};
}

std::optional<Range> Segment::rangeInTime(Range timeRange) const noexcept
{
  const auto indices = sampleIndexRange(timeRange);
  if (!indices) return std::nullopt;
  const auto [first, last] = *indices;

  std::optional<Range> result;
  const std::size_t firstBlock = first / kBlockSize;
  const std::size_t lastBlock = last / kBlockSize;

  auto scan = [&](std::size_t a, std::size_t b) {
    for (std::size_t i = a; i <= b; ++i) merge(result, samples_[i]);
  };

  if (firstBlock == lastBlock) {
    scan(first, last);
    return normalize(result);
  }

  const std::size_t firstBlockEnd = std::min(samples_.size() - 1,
                                               (firstBlock + 1) * kBlockSize - 1);
  scan(first, firstBlockEnd);
  for (std::size_t block = firstBlock + 1; block < lastBlock; ++block) {
    const auto& ext = blocks_[block];
    if (ext.present) {
      merge(result, ext.low);
      merge(result, ext.high);
    }
  }
  scan(lastBlock * kBlockSize, last);
  return normalize(result);
}

// Accumulate one segment into an exact per-column min/max envelope. The
// precomputed 256-sample blocks bypass scanning each sample when the complete
// block maps into a single output pixel (the common full-recording case).
void Segment::addToEnvelope(Range bounds, std::vector<EnvelopeBin>& bins) const
{
  if (bins.empty() || !validRange(bounds)) return;
  const auto indices = sampleIndexRange(bounds);
  if (!indices) return;
  const std::size_t columns = bins.size();
  const auto binFor = [&](std::size_t i) -> std::size_t {
    const long double time = static_cast<long double>(startTime_) +
        static_cast<long double>(i) * sampleInterval_;
    const long double fraction = (time - bounds.low) /
        (static_cast<long double>(bounds.high) - bounds.low);
    if (!(fraction > 0.0L)) return 0;
    if (fraction >= 1.0L) return columns - 1;
    return std::min(columns - 1, static_cast<std::size_t>(fraction * columns));
  };
  const auto add = [&](std::size_t x, double low, double high) {
    auto& bin = bins[x];
    if (!bin.present) { bin = {low, high, true}; }
    else { bin.low = std::min(bin.low, low); bin.high = std::max(bin.high, high); }
  };
  const auto [first, last] = *indices;
  for (std::size_t block = first / kBlockSize; block <= last / kBlockSize; ++block) {
    const std::size_t blockStart = block * kBlockSize;
    const std::size_t blockEnd = std::min(samples_.size() - 1, blockStart + kBlockSize - 1);
    const std::size_t begin = std::max(first, blockStart);
    const std::size_t finish = std::min(last, blockEnd);
    const auto& cached = blocks_[block];
    if (cached.present && begin == blockStart && finish == blockEnd &&
        binFor(begin) == binFor(finish)) {
      add(binFor(begin), cached.low, cached.high);
    } else {
      for (std::size_t i = begin; i <= finish; ++i) {
        const double value = samples_[i];
        if (std::isfinite(value)) add(binFor(i), value, value);
      }
    }
  }
}

std::optional<Range> Channel::fullRange() const noexcept
{
  std::optional<Range> result;
  for (const auto& segment : segments) merge(result, segment.fullRange());
  return normalize(result);
}

std::optional<Range> Channel::rangeInTime(Range timeRange) const noexcept
{
  std::optional<Range> result;
  for (const auto& segment : segments) merge(result, segment.rangeInTime(timeRange));
  return normalize(result);
}

std::vector<EnvelopeBin> Channel::overviewEnvelope(Range bounds, int columns) const
{
  if (!validRange(bounds) || columns < 1 || columns > 12000) return {};
  std::vector<EnvelopeBin> bins(static_cast<std::size_t>(columns));
  for (const auto& segment : segments) segment.addToEnvelope(bounds, bins);
  return bins;
}

Recording::Recording(std::vector<Channel> channels) : channels_(std::move(channels))
{
  recomputeTimeRange();
}

void Recording::recomputeTimeRange()
{
  if (channels_.empty()) {
    timeRange_.reset();
    return;
  }

  double low = std::numeric_limits<double>::infinity();
  double high = -std::numeric_limits<double>::infinity();
  for (const auto& channel : channels_) {
    for (const auto& segment : channel.segments) {
      low = std::min(low, segment.startTime());
      high = std::max(high, segment.endTime());
    }
  }
  if (std::isfinite(low) && std::isfinite(high)) {
    if (high > low) timeRange_ = Range{low, high};
    else timeRange_ = widenedRange(low);
  } else {
    timeRange_.reset();
  }
}

} // namespace itsv
