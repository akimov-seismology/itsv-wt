// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
#include "ViewState.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace itsv {

void ViewState::resetForRecording(std::size_t channelCount, Range dataRange)
{
  if (!validRange(dataRange))
    throw std::invalid_argument("Recording data range is invalid");

  baseChannels_.resize(channelCount);
  for (std::size_t i = 0; i < channelCount; ++i) baseChannels_[i] = i;
  visibleChannels_ = baseChannels_;
  markedChannels_.clear();
  overviewChannel_ = channelCount ? std::optional<std::size_t>(0) : std::nullopt;
  dataRange_ = dataRange;
  viewRange_ = dataRange;
  amplitudeMode_ = AmplitudeMode::Auto;
  amplitudeRanges_.assign(channelCount, Range{-1.0, 1.0});
  inverted_ = false;
  clipEnabled_ = true;
}

void ViewState::setBaseChannels(std::vector<std::size_t> channels,
                                std::size_t channelCount)
{
  std::unordered_set<std::size_t> seen;
  std::vector<std::size_t> clean;
  clean.reserve(channels.size());
  for (const auto channel : channels) {
    if (channel >= channelCount)
      throw std::out_of_range("Channel selection index is outside the recording");
    if (seen.insert(channel).second) clean.push_back(channel);
  }
  baseChannels_ = std::move(clean);
  visibleChannels_ = baseChannels_;
  markedChannels_.clear();
  ensureOverviewVisible();
}

bool ViewState::selectOnlyVisible(std::size_t screenIndex)
{
  if (screenIndex >= visibleChannels_.size()) return false;
  const auto channel = visibleChannels_[screenIndex];
  markedChannels_.assign(1, channel);
  overviewChannel_ = channel;
  return true;
}

bool ViewState::toggleVisibleMark(std::size_t screenIndex)
{
  if (screenIndex >= visibleChannels_.size()) return false;
  const auto channel = visibleChannels_[screenIndex];
  overviewChannel_ = channel;
  const auto it = std::find(markedChannels_.begin(), markedChannels_.end(), channel);
  if (it == markedChannels_.end()) markedChannels_.push_back(channel);
  else markedChannels_.erase(it);
  return true;
}

void ViewState::markAllVisible()
{
  markedChannels_ = visibleChannels_;
}

bool ViewState::keepMarked()
{
  if (markedChannels_.empty()) return false;
  visibleChannels_ = markedChannels_;
  markedChannels_.clear();
  ensureOverviewVisible();
  return true;
}

bool ViewState::deleteMarked()
{
  if (markedChannels_.empty()) return false;
  std::unordered_set<std::size_t> remove(markedChannels_.begin(), markedChannels_.end());
  visibleChannels_.erase(
      std::remove_if(visibleChannels_.begin(), visibleChannels_.end(),
                     [&](std::size_t channel) { return remove.count(channel) != 0; }),
      visibleChannels_.end());
  markedChannels_.clear();
  ensureOverviewVisible();
  return true;
}

void ViewState::restoreVisible()
{
  visibleChannels_ = baseChannels_;
  markedChannels_.clear();
  ensureOverviewVisible();
}

bool ViewState::isMarked(std::size_t channelIndex) const noexcept
{
  return std::find(markedChannels_.begin(), markedChannels_.end(), channelIndex) !=
         markedChannels_.end();
}

int ViewState::markNumber(std::size_t channelIndex) const noexcept
{
  const auto it = std::find(markedChannels_.begin(), markedChannels_.end(), channelIndex);
  if (it == markedChannels_.end()) return -1;
  return static_cast<int>(std::distance(markedChannels_.begin(), it));
}

void ViewState::setOverviewChannel(std::optional<std::size_t> channel)
{
  overviewChannel_ = channel;
  ensureOverviewVisible();
}

bool ViewState::setViewRange(Range range)
{
  if (!validRange(range)) return false;
  viewRange_ = range;
  return true;
}

void ViewState::fitToData()
{
  if (dataRange_) viewRange_ = dataRange_;
}

void ViewState::setAmplitudeRange(std::size_t channelIndex, Range range)
{
  if (!validRange(range)) return;
  amplitudeRanges_.at(channelIndex) = range;
}

void ViewState::toggleInverted()
{
  if (amplitudeMode_ == AmplitudeMode::Fixed) {
    for (auto& range : amplitudeRanges_)
      range = Range{-range.high, -range.low};
  }
  inverted_ = !inverted_;
}

void ViewState::ensureOverviewVisible()
{
  if (visibleChannels_.empty()) {
    overviewChannel_.reset();
    return;
  }
  if (!overviewChannel_ ||
      std::find(visibleChannels_.begin(), visibleChannels_.end(), *overviewChannel_) ==
          visibleChannels_.end())
    overviewChannel_ = visibleChannels_.front();
}

} // namespace itsv
