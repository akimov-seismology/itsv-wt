// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
#include "WaveformPreview.h"
#include "PreviewSelectionGeometry.h"

#include <Wt/WBrush.h>
#include <Wt/WFont.h>
#include <Wt/WPainter.h>
#include <Wt/WPen.h>
#include <Wt/WRectF.h>
#include <Wt/WString.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <string>

namespace itsv {
namespace {
/**
 * @brief Format a UTC epoch timestamp for an overview-axis label.
 * @param seconds Timestamp in Unix epoch seconds.
 * @return UTC label, or a placeholder for out-of-range values.
 */
std::string utcLabel(double seconds)
{
  if (!std::isfinite(seconds)) return "?";
  const double wholeSeconds = std::floor(seconds);
  if (static_cast<long double>(wholeSeconds) <
          static_cast<long double>(std::numeric_limits<std::time_t>::min()) ||
      static_cast<long double>(wholeSeconds) >
          static_cast<long double>(std::numeric_limits<std::time_t>::max())) return "?";
  const std::time_t whole = static_cast<std::time_t>(wholeSeconds);
  std::tm utc{};
  if (!gmtime_r(&whole, &utc)) return "?";
  char buffer[32];
  if (!std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &utc)) return "?";
  return buffer;
}
}

WaveformPreviewWidget::WaveformPreviewWidget(WaveformDisplayWidget& display)
  : display_(display)
{
  // WPaintedWidget must be told the size supplied by the flex layout.
  // Explicit pixel width in resize() disables layout-size awareness and leaves
  // an undersized drawing buffer stretched across a wider widget.
  setLayoutSizeAware(true);
  setCanReceiveFocus(false);
  setAttributeValue("style", "user-select:none;-webkit-user-select:none;");
  setAttributeValue("oncontextmenu", "return false;");
  setToolTip("Click the full waveform to center the visible time window. Move the pointer over it to read UTC time.");
  mouseWentDown().connect(this, &WaveformPreviewWidget::onMouseDown);
  mouseWentUp().connect(this, &WaveformPreviewWidget::onMouseUp);
  mouseMoved().connect(this, &WaveformPreviewWidget::onMouseMove);
  mouseDragged().connect(this, &WaveformPreviewWidget::onMouseMove);
  mouseWentDown().preventDefaultAction();
  mouseWentUp().preventDefaultAction();
}

void WaveformPreviewWidget::setFontSize(int pixels)
{
  const int next = std::clamp(pixels, 10, 12);
  if (fontSize_ == next) return;
  fontSize_ = next;
  update();
}

void WaveformPreviewWidget::invalidateCache()
{
  leftPressedInPlot_ = false;
  plotReady_ = false;
  bins_.clear();
  cachedChannel_.reset();
  cachedColumns_ = 0;
  update();
}

bool WaveformPreviewWidget::timeAt(const Wt::WMouseEvent& event, double& time) const
{
  if (!plotReady_ || plotWidth_ <= 0 || plotHeight_ <= 0) return false;
  const int x = event.widget().x;
  const int y = event.widget().y;
  if (x < plotLeft_ || x > plotLeft_ + plotWidth_ ||
      y < plotTop_ || y > plotTop_ + plotHeight_) return false;
  const auto data = display_.dataRange();
  if (!data) return false;
  time = data->low + double(x - plotLeft_) * (data->high - data->low) / plotWidth_;
  return std::isfinite(time);
}

void WaveformPreviewWidget::onMouseDown(const Wt::WMouseEvent& event)
{
  if (event.button() != Wt::MouseButton::Left) return;
  double time = 0.0;
  leftPressedInPlot_ = timeAt(event, time);
  if (leftPressedInPlot_) {
    display_.cancelMouseGesture();
    display_.showTimeAt(time);
  }
}

void WaveformPreviewWidget::onMouseUp(const Wt::WMouseEvent& event)
{
  if (event.button() != Wt::MouseButton::Left) return;
  const bool pressedHere = leftPressedInPlot_;
  leftPressedInPlot_ = false;
  double time = 0.0;
  if (pressedHere && timeAt(event, time)) display_.centerTimeViewAt(time);
}

void WaveformPreviewWidget::onMouseMove(const Wt::WMouseEvent& event)
{
  double time = 0.0;
  if (timeAt(event, time)) display_.showTimeAt(time);
}

void WaveformPreviewWidget::rebuildCache(std::size_t channelIndex, int columns,
                                         double start, double end)
{
  cachedChannel_ = channelIndex;
  cachedColumns_ = columns;
  cachedStart_ = start;
  cachedEnd_ = end;
  bins_.clear();
  minValue_ = maxValue_ = 0.0;

  const auto* recording = display_.recording();
  if (!recording || channelIndex >= recording->channelCount() ||
      !validRange({start, end}) || columns < 1) return;
  const auto& channel = recording->channel(channelIndex);
  bins_ = channel.overviewEnvelope({start, end}, columns);
  if (const auto range = channel.fullRange()) {
    minValue_ = range->low;
    maxValue_ = range->high;
  }
}

void WaveformPreviewWidget::paintEvent(Wt::WPaintDevice* device)
{
  plotReady_ = false;
  Wt::WPainter painter(device);
  const int width = std::clamp(static_cast<int>(std::lround(device->width().toPixels())), 1, 12000);
  const int height = std::clamp(static_cast<int>(std::lround(device->height().toPixels())), 1, 5000);
  const int labelWidth = std::min(display_.labelPanelWidth(), std::max(0, width - 1));
  const int left = labelWidth;
  const int top = 5;
  const int plotWidth = std::max(1, width - left - 2);
  const int plotHeight = std::max(1, height - top - 26);
  plotLeft_ = left;
  plotTop_ = top;
  plotWidth_ = plotWidth;
  plotHeight_ = plotHeight;

  painter.setPen(Wt::WPen(Wt::WColor(180, 180, 180)));
  painter.setBrush(Wt::WBrush(Wt::WColor(211, 211, 211)));
  painter.drawRect(0, 0, labelWidth, height);

  Wt::WFont font;
  font.setFamily(Wt::FontFamily::Monospace);
  font.setSize(Wt::WLength(fontSize_));
  painter.setFont(font);

  const auto* recording = display_.recording();
  const auto data = display_.dataRange();
  const auto view = display_.viewRange();
  const auto selected = display_.overviewChannel();
  if (!recording || !data || !view || !selected || *selected >= recording->channelCount()) {
    painter.setPen(Wt::WPen(Wt::WColor(55, 55, 55)));
    painter.drawText(Wt::WRectF(left + 10, 12, plotWidth - 10, 22),
                     Wt::AlignmentFlag::Left, "No waveform data loaded");
    return;
  }

  plotReady_ = width > left + 2 && height > top + 26;
  if (!cachedChannel_ || *cachedChannel_ != *selected || cachedColumns_ != plotWidth ||
      cachedStart_ != data->low || cachedEnd_ != data->high)
    rebuildCache(*selected, plotWidth, data->low, data->high);

  painter.setPen(Wt::WPen(Wt::WColor(0, 0, 0)));
  const auto& id = recording->channel(*selected).id;
  painter.drawText(Wt::WRectF(6, 8, labelWidth - 10, 19), Wt::AlignmentFlag::Left,
                   Wt::WString::fromUTF8(id.displayName()));
  painter.drawText(Wt::WRectF(6, 27, labelWidth - 10, 17), Wt::AlignmentFlag::Left,
                   "Full waveform");

  painter.setPen(Wt::WPen(Wt::WColor(0, 0, 192)));
  painter.setBrush(Wt::WBrush(Wt::WColor(0, 0, 192)));
  painter.drawRect(left, top, plotWidth, plotHeight);

  // Make the current main-window interval recognizable as a rectangle.
  // The very light tint goes UNDER the yellow waveform, never washing it out.
  const auto band = previewTimeBand(*data, *view, left, plotWidth);
  if (band && band->right > band->left) {
    painter.fillRect(band->left, top, band->right - band->left, plotHeight,
                     Wt::WBrush(Wt::WColor(135, 205, 255, 30)));
  }

  // Overview amplitude is intentionally independent of the main display's
  // Fixed/A0/A1/AA modes: it must always reveal the entire selected channel.
  // For large DC offsets, fit to actual min/max rather than forcing zero to
  // the center (which would flatten the waveform to an almost straight line).
  const bool inverted = display_.viewState().inverted();
  const long double a = inverted ? -static_cast<long double>(maxValue_) : minValue_;
  const long double b = inverted ? -static_cast<long double>(minValue_) : maxValue_;
  long double ampSpan = b - a;
  if (!(ampSpan > 0.0L) || !std::isfinite(ampSpan)) ampSpan = 1.0L;
  const long double padding = 0.05L * ampSpan;
  const long double bottom = b + padding;
  const long double fullSpan = ampSpan + 2.0L * padding;
  const auto screenY = [&](double sample) -> double {
    const long double value = inverted ? -static_cast<long double>(sample) : sample;
    const long double y = static_cast<long double>(top + plotHeight) -
        ((value - (bottom - fullSpan)) / fullSpan) * plotHeight;
    return static_cast<double>(std::clamp(y,
        static_cast<long double>(top), static_cast<long double>(top + plotHeight)));
  };

  // Draw the finite min/max envelope and connect successive occupied columns.
  // An empty column breaks continuity so long data gaps remain visible.
  painter.setPen(Wt::WPen(Wt::WColor(255, 255, 0)));
  bool previous = false;
  double previousY = 0.0;
  for (int x = 0; x < plotWidth && static_cast<std::size_t>(x) < bins_.size(); ++x) {
    const auto& bin = bins_[static_cast<std::size_t>(x)];
    if (!bin.present) { previous = false; continue; }
    const double yLow = screenY(bin.low);
    const double yHigh = screenY(bin.high);
    const double yMid = 0.5 * (yLow + yHigh);
    if (previous) painter.drawLine(left + x - 1, previousY, left + x, yMid);
    painter.drawLine(left + x, yLow, left + x, yHigh);
    previous = true;
    previousY = yMid;
  }

  // Draw the complete selection outline inside the plot bounds, with a
  // dark under-stroke and bright border for contrast.
  if (band) {
    if (const auto rect = previewSelectionRect(*band, left, top, plotWidth, plotHeight)) {
      painter.setBrush(Wt::WBrush()); // Never cover waveform samples.
      Wt::WPen shadow(Wt::WColor(7, 28, 82));
      shadow.setWidth(Wt::WLength(4));
      painter.setPen(shadow);
      painter.drawRect(rect->x, rect->y, rect->width, rect->height);
      Wt::WPen outline(Wt::WColor(238, 252, 255));
      outline.setWidth(Wt::WLength(2));
      painter.setPen(outline);
      painter.drawRect(rect->x, rect->y, rect->width, rect->height);
    }
  }

  painter.setPen(Wt::WPen(Wt::WColor(45, 45, 45)));
  painter.drawText(Wt::WRectF(left + 3, height - 20, std::max(0, plotWidth / 2 - 3), 18),
                   Wt::AlignmentFlag::Left, Wt::WString::fromUTF8(utcLabel(data->low)));
  painter.drawText(Wt::WRectF(left + plotWidth / 2, height - 20, plotWidth / 2, 18),
                   Wt::AlignmentFlag::Right, Wt::WString::fromUTF8(utcLabel(data->high)));
}

} // namespace itsv
