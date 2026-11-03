// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
#include "ChannelSelectionDialog.h"

#include <Wt/WBorder.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WCssDecorationStyle.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WTable.h>
#include <Wt/WText.h>
#include <Wt/WTree.h>
#include <Wt/WTreeTable.h>
#include <Wt/WTreeTableNode.h>
#include <Wt/WVBoxLayout.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace itsv {

/**
 * @brief Selectable leaf in the station/channel tree, with a checkbox callback.
 */
class ChannelSelectionDialog::ChannelNode final : public Wt::WTreeTableNode {
public:
  /// Receives the recording channel index and new checked state.
  using Callback = std::function<void(std::size_t, bool)>;

  /**
   * @brief Create a checkbox tree leaf bound to one recording channel.
   * @param channelIndex Channel index in the recording.
   * @param label Visible channel name.
   * @param checked Initial selected state.
   * @param callback Invoked with channel index and new selected state.
   */
  ChannelNode(std::size_t channelIndex, const std::string& label, bool checked, Callback callback)
    : Wt::WTreeTableNode(label), channelIndex_(channelIndex), callback_(std::move(callback))
  {
    checkBox_ = labelArea()->addWidget(std::make_unique<Wt::WCheckBox>());
    checkBox_->setChecked(checked);
    checkBox_->checked().connect([this] { callback_(channelIndex_, true); });
    checkBox_->unChecked().connect([this] { callback_(channelIndex_, false); });
  }

  /**
   * @brief Synchronize the checkbox with the staged selection.
   * @param checked True to check the channel.
   */
  void setChecked(bool checked) { checkBox_->setChecked(checked); }
  /**
   * @brief Return the associated recording channel index.
   */
  std::size_t channelIndex() const noexcept { return channelIndex_; }

private:
  /// Index of the channel represented by this checkbox leaf.
  std::size_t channelIndex_;
  /// Notifies the parent dialog when this checkbox changes.
  Callback callback_;
  /// Checkbox owned by the tree node's label area.
  Wt::WCheckBox* checkBox_{nullptr};
};

ChannelSelectionDialog::ChannelSelectionDialog(WaveformDisplayWidget& display)
  : Wt::WDialog("Select Channels"), display_(display)
{
  setModal(true);
  setClosable(true);
  setResizable(true);
  rejectWhenEscapePressed();
  resize(Wt::WLength(1000), Wt::WLength(650));
  setMaximumSize(Wt::WLength(95, Wt::WLength::Unit::Percentage),
                 Wt::WLength(90, Wt::WLength::Unit::Percentage));
  setMinimumSize(Wt::WLength(1), Wt::WLength(1));
  contents()->setOverflow(Wt::Overflow::Hidden);
  footer()->setContentAlignment(Wt::AlignmentFlag::Right);
  footer()->setPadding(Wt::WLength(8));

  for (const auto channel : display_.channelSelection()) selected_.insert(channel);
  buildUi();
}

void ChannelSelectionDialog::buildUi()
{
  auto* layout = contents()->setLayout(std::make_unique<Wt::WHBoxLayout>());
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  auto* treePanel = layout->addWidget(std::make_unique<Wt::WContainerWidget>(), 1);
  auto* selectedPanel = layout->addWidget(std::make_unique<Wt::WContainerWidget>(), 1);
  treePanel->decorationStyle().setBorder(
      Wt::WBorder(Wt::WBorder::Style::Solid, Wt::WLength(1), Wt::WColor(150, 150, 150)));
  selectedPanel->decorationStyle().setBorder(
      Wt::WBorder(Wt::WBorder::Style::Solid, Wt::WLength(1), Wt::WColor(150, 150, 150)));
  treePanel->setMinimumSize(Wt::WLength(0), Wt::WLength(0));
  selectedPanel->setMinimumSize(Wt::WLength(0), Wt::WLength(0));
  treePanel->setOverflow(Wt::Overflow::Hidden);
  selectedPanel->setOverflow(Wt::Overflow::Auto);
  selectedPanel->setPadding(Wt::WLength(6));

  auto* treeLayout = treePanel->setLayout(std::make_unique<Wt::WVBoxLayout>());
  treeLayout->setContentsMargins(0, 0, 0, 0);
  treeLayout->setSpacing(0);
  tree_ = treeLayout->addWidget(std::make_unique<Wt::WTreeTable>(), 1);
  tree_->setMinimumSize(Wt::WLength(0), Wt::WLength(0));

  selectedTable_ = selectedPanel->addWidget(std::make_unique<Wt::WTable>());
  selectedTable_->setWidth(Wt::WLength(100, Wt::WLength::Unit::Percentage));
  selectedTable_->setHeaderCount(1);
  selectedTable_->elementAt(0, 0)->addNew<Wt::WText>("Selected Channels");

  auto* ok = footer()->addWidget(std::make_unique<Wt::WPushButton>("OK"));
  auto* cancel = footer()->addWidget(std::make_unique<Wt::WPushButton>("Cancel"));
  auto* selectAllButton = footer()->addWidget(std::make_unique<Wt::WPushButton>("Select all"));
  auto* clearButton = footer()->addWidget(std::make_unique<Wt::WPushButton>("Clear selection"));
  ok->setDefault(true);
  for (auto* button : {ok, cancel, selectAllButton, clearButton}) {
    button->setMinimumSize(Wt::WLength(80), Wt::WLength(32));
    button->setMargin(Wt::WLength(6), Wt::Side::Left);
  }
  clearButton->setMinimumSize(Wt::WLength(130), Wt::WLength(32));

  ok->clicked().connect(this, &ChannelSelectionDialog::commitAndAccept);
  cancel->clicked().connect([this] { reject(); });
  selectAllButton->clicked().connect(this, &ChannelSelectionDialog::selectAll);
  clearButton->clicked().connect(this, &ChannelSelectionDialog::clearSelection);

  tree_->tree()->setSelectionMode(Wt::SelectionMode::Extended);
  tree_->setTreeRoot(std::make_unique<Wt::WTreeTableNode>("All Stations"), "Stations");
  tree_->treeRoot()->setInteractive(true);

  const auto* recording = display_.recording();
  if (!recording) return;

  std::unordered_map<std::string, Wt::WTreeTableNode*> networkNodes;
  std::unordered_map<std::string, Wt::WTreeTableNode*> stationNodes;
  channelNodes_.clear();
  channelNodes_.reserve(recording->channelCount());

  for (std::size_t index = 0; index < recording->channelCount(); ++index) {
    const auto& id = recording->channel(index).id;
    const std::string networkLabel = id.network.empty() ? "(no network)" : id.network;
    const std::string stationLabel = id.station.empty() ? "(no station)" : id.station;

    auto netIt = networkNodes.find(id.network);
    Wt::WTreeTableNode* networkNode = nullptr;
    if (netIt == networkNodes.end()) {
      auto node = std::make_unique<Wt::WTreeTableNode>(networkLabel);
      networkNode = node.get();
      tree_->treeRoot()->addChildNode(std::move(node));
      networkNodes.emplace(id.network, networkNode);
    } else networkNode = netIt->second;

    const std::string stationKey = id.network + "\x1f" + id.station;
    auto staIt = stationNodes.find(stationKey);
    Wt::WTreeTableNode* stationNode = nullptr;
    if (staIt == stationNodes.end()) {
      auto node = std::make_unique<Wt::WTreeTableNode>(stationLabel);
      stationNode = node.get();
      networkNode->addChildNode(std::move(node));
      stationNodes.emplace(stationKey, stationNode);
    } else stationNode = staIt->second;

    std::string label = id.channel;
    if (!id.location.empty()) label += "." + id.location;
    auto node = std::make_unique<ChannelNode>(
        index, label, selected_.count(index) != 0,
        [this](std::size_t channel, bool selected) { setSelected(channel, selected); });
    auto* nodePtr = node.get();
    stationNode->addChildNode(std::move(node));
    stationNode->expand();
    channelNodes_.push_back(nodePtr);
  }

  tree_->treeRoot()->expand();
  tree_->refresh();
  rebuildSelectedList();
}

void ChannelSelectionDialog::setSelected(std::size_t channelIndex, bool selected)
{
  if (selected) selected_.insert(channelIndex);
  else selected_.erase(channelIndex);
  rebuildSelectedList();
}

void ChannelSelectionDialog::rebuildSelectedList()
{
  if (!selectedTable_) return;
  for (int row = selectedTable_->rowCount() - 1; row > 0; --row)
    selectedTable_->removeRow(row);

  const auto* recording = display_.recording();
  if (!recording) return;
  int row = 1;
  for (std::size_t index = 0; index < recording->channelCount(); ++index) {
    if (selected_.count(index) == 0) continue;
    selectedTable_->elementAt(row++, 0)->addNew<Wt::WText>(
        Wt::WString::fromUTF8(recording->channel(index).id.seedName()));
  }
}

void ChannelSelectionDialog::selectAll()
{
  const auto* recording = display_.recording();
  if (!recording) return;
  selected_.clear();
  for (std::size_t i = 0; i < recording->channelCount(); ++i) selected_.insert(i);
  for (auto* node : channelNodes_) node->setChecked(true);
  rebuildSelectedList();
}

void ChannelSelectionDialog::clearSelection()
{
  selected_.clear();
  for (auto* node : channelNodes_) node->setChecked(false);
  rebuildSelectedList();
}

void ChannelSelectionDialog::commitAndAccept()
{
  const auto* recording = display_.recording();
  if (!recording) {
    reject();
    return;
  }
  std::vector<std::size_t> selected;
  selected.reserve(selected_.size());
  for (std::size_t i = 0; i < recording->channelCount(); ++i)
    if (selected_.count(i) != 0) selected.push_back(i);
  display_.setChannelSelection(std::move(selected));
  accept();
}

} // namespace itsv
