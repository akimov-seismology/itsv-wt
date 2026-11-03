// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file ViewState.h
 *  @brief UI-independent trace selection, time range and amplitude mode state.
 */
#pragma once

#include "Range.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace itsv {

/**
 * @brief Strategy for determining each waveform panel's vertical scale.
 */
enum class AmplitudeMode {
  /// Use explicitly retained amplitude bounds.
  Fixed,
  /// Fit each trace to the samples visible in time.
  Auto,
  /// Place zero at the center of each vertical range.
  ZeroCentered,
  /// Use the first visible trace as the scale reference.
  FirstTrace,
  /// Use a common range across visible traces.
  AllSame
};

/**
 * @brief UI-independent state for visible/marked traces, time range and amplitude options.
 */
class ViewState {
public:
  /**
   * @brief Clear UI state and initialize all channels for a newly loaded recording.
   * @param channelCount Number of channels available in the recording.
   * @param dataRange Complete valid UTC time extent.
   */
  void resetForRecording(std::size_t channelCount, Range dataRange);
  /**
   * @brief Replace the ordered channel selection and reset trace visibility.
   * @param channels Selected recording indices in preferred display order.
   * @param channelCount Number of channels available for validation.
   */
  void setBaseChannels(std::vector<std::size_t> channels, std::size_t channelCount);

  /**
   * @brief Access the complete ordered channel selection.
   */
  const std::vector<std::size_t>& baseChannels() const noexcept { return baseChannels_; }
  /**
   * @brief Access the ordered channels currently drawn in the main view.
   */
  const std::vector<std::size_t>& visibleChannels() const noexcept { return visibleChannels_; }
  /**
   * @brief Access channels explicitly marked for bulk operations.
   */
  const std::vector<std::size_t>& markedChannels() const noexcept { return markedChannels_; }

  /**
   * @brief Select just one displayed trace and clear other marks.
   * @param screenIndex Index within the visible display rows.
   * @return False when the screen row is invalid.
   */
  bool selectOnlyVisible(std::size_t screenIndex);
  /**
   * @brief Toggle the marked state of one visible trace.
   * @param screenIndex Index within the visible display rows.
   * @return False when the screen row is invalid.
   */
  bool toggleVisibleMark(std::size_t screenIndex);
  /**
   * @brief Mark every currently visible trace.
   */
  void markAllVisible();
  /**
   * @brief Clear the set of explicitly marked traces.
   */
  void clearMarks() noexcept { markedChannels_.clear(); }
  /**
   * @brief Keep only the marked traces in the current view.
   * @return Whether the visible trace list changed.
   */
  bool keepMarked();
  /**
   * @brief Remove marked traces from the current view.
   * @return Whether the visible trace list changed.
   */
  bool deleteMarked();
  /**
   * @brief Restore the complete base-channel list to the main display.
   */
  void restoreVisible();

  /**
   * @brief Check whether a recording channel is marked.
   * @param channelIndex Zero-based index in the recording.
   */
  bool isMarked(std::size_t channelIndex) const noexcept;
  /**
   * @brief Find a channel's ordinal in the marked list.
   * @param channelIndex Zero-based recording channel index.
   * @return Zero-based ordinal, or -1 when not marked.
   */
  int markNumber(std::size_t channelIndex) const noexcept;

  /**
   * @brief Return the current full-waveform overview channel.
   */
  std::optional<std::size_t> overviewChannel() const noexcept { return overviewChannel_; }
  /**
   * @brief Choose a channel for the full-waveform overview.
   * @param channel Recording channel index, or no value to clear the selection.
   */
  void setOverviewChannel(std::optional<std::size_t> channel);

  /**
   * @brief Access the complete UTC bounds of loaded data.
   */
  const std::optional<Range>& dataRange() const noexcept { return dataRange_; }
  /**
   * @brief Access the UTC interval visible in the main display.
   */
  const std::optional<Range>& viewRange() const noexcept { return viewRange_; }
  /**
   * @brief Replace the visible time interval when valid.
   * @param range New finite positive-width UTC interval.
   * @return Whether the interval was accepted.
   */
  bool setViewRange(Range range);
  /**
   * @brief Reset the time window to the full loaded recording interval.
   */
  void fitToData();

  /**
   * @brief Return the currently active vertical scaling mode.
   */
  AmplitudeMode amplitudeMode() const noexcept { return amplitudeMode_; }
  /**
   * @brief Set the active vertical scaling mode.
   * @param mode Scaling mode to activate.
   */
  void setAmplitudeMode(AmplitudeMode mode) noexcept { amplitudeMode_ = mode; }
  /**
   * @brief Read the stored vertical bounds for one recording channel.
   * @param channelIndex Zero-based recording channel index.
   */
  const Range& amplitudeRange(std::size_t channelIndex) const { return amplitudeRanges_.at(channelIndex); }
  /**
   * @brief Set stored finite vertical bounds for one channel.
   * @param channelIndex Zero-based recording channel index.
   * @param range Vertical amplitude interval.
   */
  void setAmplitudeRange(std::size_t channelIndex, Range range);
  /**
   * @brief Return the number of stored per-channel amplitude intervals.
   */
  std::size_t amplitudeRangeCount() const noexcept { return amplitudeRanges_.size(); }

  /**
   * @brief Report whether polarity inversion is enabled.
   */
  bool inverted() const noexcept { return inverted_; }
  /**
   * @brief Toggle positive-up versus positive-down plotting.
   */
  void toggleInverted();
  /**
   * @brief Report whether plot traces are clipped to their rows.
   */
  bool clipEnabled() const noexcept { return clipEnabled_; }
  /**
   * @brief Toggle clipping to the active trace panel.
   * @return New clipping state.
   */
  bool toggleClip() noexcept { clipEnabled_ = !clipEnabled_; return clipEnabled_; }

private:
  /// Ordered channel indices selected in the channel dialog.
  std::vector<std::size_t> baseChannels_;
  /// Subset of base channels currently shown in the main view.
  std::vector<std::size_t> visibleChannels_;
  /// Explicitly marked channel indices for bulk operations.
  std::vector<std::size_t> markedChannels_;
  /// Channel whose full recording is shown in the overview.
  std::optional<std::size_t> overviewChannel_;
  /// Available UTC time extent of the loaded recording.
  std::optional<Range> dataRange_;
  /// Currently displayed UTC time interval.
  std::optional<Range> viewRange_;
  /// Active amplitude scaling strategy.
  AmplitudeMode amplitudeMode_{AmplitudeMode::Auto};
  /// Per-channel vertical bounds indexed by recording channel number.
  std::vector<Range> amplitudeRanges_;
  /// Whether positive amplitudes are drawn downward.
  bool inverted_{false};
  /// Whether samples outside a trace's row are clipped.
  bool clipEnabled_{true};

  /**
   * @brief Keep the overview channel assigned to a visible trace.
   */
  void ensureOverviewVisible();
};

} // namespace itsv
