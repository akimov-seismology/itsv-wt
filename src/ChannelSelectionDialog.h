// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
/** @file ChannelSelectionDialog.h
 *  @brief Wt dialog for choosing which waveform channels to display.
 */
#pragma once

#include "WaveformDisplay.h"

#include <Wt/WDialog.h>

#include <cstddef>
#include <unordered_set>
#include <vector>

namespace Wt {
class WTable;
class WTreeTable;
}

namespace itsv {

/**
 * @brief Modal station/channel picker that commits its selection to the waveform display.
 */
class ChannelSelectionDialog final : public Wt::WDialog {
public:
  /**
   * @brief Create a channel selector initialized from the current display.
   * @param display Viewer whose channel selection is edited; must outlive the dialog.
   */
  explicit ChannelSelectionDialog(WaveformDisplayWidget& display);

private:
  class ChannelNode;

  /// Waveform viewer receiving the committed selection; not owned.
  WaveformDisplayWidget& display_;
  /// Station hierarchy widget; owned by the Wt dialog.
  Wt::WTreeTable* tree_{nullptr};
  /// Table listing channels included in the pending selection.
  Wt::WTable* selectedTable_{nullptr};
  /// Checkbox leaf widgets indexed by channel number; not owned.
  std::vector<ChannelNode*> channelNodes_;
  /// Set of channel indices staged for confirmation.
  std::unordered_set<std::size_t> selected_;

  /**
   * @brief Build the network/station tree, selected-channel list and action buttons.
   */
  void buildUi();
  /**
   * @brief Add or remove a channel in the pending selection.
   * @param channelIndex Zero-based recording channel index.
   * @param selected True to include the channel; false to exclude it.
   */
  void setSelected(std::size_t channelIndex, bool selected);
  /**
   * @brief Refresh the right-hand list from the staged selection.
   */
  void rebuildSelectedList();
  /**
   * @brief Stage every loaded channel for inclusion.
   */
  void selectAll();
  /**
   * @brief Remove every channel from the pending selection.
   */
  void clearSelection();
  /**
   * @brief Apply the staged channel list and close the dialog successfully.
   */
  void commitAndAccept();
};

} // namespace itsv
