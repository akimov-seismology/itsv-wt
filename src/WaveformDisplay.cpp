// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
#include "WaveformDisplay.h"
#include "PlotGeometry.h"

#include <Wt/WApplication.h>
#include <Wt/WBrush.h>
#include <Wt/WCssDecorationStyle.h>
#include <Wt/WFont.h>
#include <Wt/WPainter.h>
#include <Wt/WPen.h>
#include <Wt/WRectF.h>
#include <Wt/WString.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace itsv {
namespace {

/**
 * @brief Format an epoch timestamp using UTC rather than the browser's local zone.
 * @param seconds UTC seconds since 1970-01-01, including fractional seconds.
 * @param date True to show date and time; false for time only.
 * @param milliseconds True to append a three-digit fractional field.
 * @return Formatted UTC string, or an invalid-time indicator.
 */
std::string formatUtc(double seconds, bool date, bool milliseconds = false)
{
  if (!std::isfinite(seconds)) return "invalid time";
  const double floorSeconds = std::floor(seconds);
  if (static_cast<long double>(floorSeconds) <
          static_cast<long double>(std::numeric_limits<std::time_t>::min()) ||
      static_cast<long double>(floorSeconds) >=
          static_cast<long double>(std::numeric_limits<std::time_t>::max()))
    return "invalid time";
  std::time_t whole = static_cast<std::time_t>(floorSeconds);
  int ms = static_cast<int>(std::llround((seconds - floorSeconds) * 1000.0));
  if (ms == 1000) { ++whole; ms = 0; }
  std::tm utc{};
  if (!gmtime_r(&whole, &utc)) return "invalid time";
  char buffer[40];
  if (!std::strftime(buffer, sizeof(buffer), date ? "%Y-%m-%d %H:%M:%S" : "%H:%M:%S", &utc))
    return "invalid time";
  std::string result(buffer);
  if (milliseconds) {
    std::ostringstream out;
    out << '.' << std::setw(3) << std::setfill('0') << ms;
    result += out.str();
  }
  return result;
}

/**
 * @brief Round a time-axis tick spacing up to a readable 1-2-5 interval.
 * @param desiredSeconds Requested interval between adjacent ticks, in seconds.
 * @return Nonzero tick step in seconds.
 */
double niceTickStep(double desiredSeconds)
{
  if (!(desiredSeconds > 0.0) || !std::isfinite(desiredSeconds)) return 1.0;
  const double power = std::pow(10.0, std::floor(std::log10(desiredSeconds)));
  for (const double multiple : {1.0, 2.0, 5.0, 10.0}) {
    const double step = multiple * power;
    if (step >= desiredSeconds) return step;
  }
  return 10.0 * power;
}

/**
 * @brief Draw UTF-8 text inside a bounded painter rectangle.
 * @param painter Active painter.
 * @param x Left coordinate in pixels.
 * @param y Top coordinate in pixels.
 * @param width Available text-box width in pixels.
 * @param height Available text-box height in pixels.
 * @param text UTF-8 content to draw.
 * @param alignment Horizontal alignment of the content.
 */
void drawText(Wt::WPainter& painter, double x, double y, double width, double height,
              const std::string& text,
              Wt::AlignmentFlag alignment = Wt::AlignmentFlag::Left)
{
  painter.drawText(Wt::WRectF(x, y, std::max(0.0, width), std::max(0.0, height)),
                   Wt::WFlags<Wt::AlignmentFlag>(alignment),
                   Wt::WString::fromUTF8(text));
}

/**
 * @brief Guarantee finite, nonzero amplitude limits for plotting.
 * @param range Proposed amplitude extrema.
 * @return Valid positive-width range or a safe fallback.
 */
Range ensureUsableRange(Range range)
{
  if (!std::isfinite(range.low) || !std::isfinite(range.high)) return {-1.0, 1.0};
  if (range.high > range.low) return range;
  return widenedRange(range.low);
}

} // namespace

WaveformDisplayWidget::WaveformDisplayWidget()
{
  setCanReceiveFocus(true);
  setFocus(true);
  setAttributeValue("style", "user-select:none;-webkit-user-select:none;touch-action:pan-y;");
  setAttributeValue("oncontextmenu", "event.preventDefault();return false;");
  setAttributeValue("ondragstart", "return false;");
  setToolTip("Shift + left/right then click again: time zoom. Ctrl + left/right then click again: amplitude zoom. "
             "Channel label: click to switch; Ctrl-click to mark multiple traces. Middle drag: pan. Esc: cancel.");

  mouseWentDown().connect(this, &WaveformDisplayWidget::onMouseDown);
  mouseWentDown().preventDefaultAction(true);
  mouseWentDown().preventPropagation(true);
  mouseWentUp().connect(this, &WaveformDisplayWidget::onMouseUp);
  mouseWentUp().preventDefaultAction(true);
  mouseMoved().connect(this, &WaveformDisplayWidget::onMouseMove);
  mouseDragged().connect(this, &WaveformDisplayWidget::onMouseMove);

  // The temporary zoom rectangle is moved entirely in the browser.  Sending
  // every mouse move through paintEvent() redraws all waveform traces and is
  // noticeably slow for large recordings.  The server only needs the first
  // and final click coordinates; this client handler keeps the box fluid.
  const std::string moveSelectionOverlay =
      "function(o,e){"
      "var w=o,s=w&&w.__itsvSelection,d=w&&w.__itsvSelectionOverlay;"
      "if(!w||!s||!d)return;"
      "var r=w.getBoundingClientRect();"
      "var cx=(e.clientX!=null?e.clientX:e.pageX-window.pageXOffset)-r.left;"
      "var cy=(e.clientY!=null?e.clientY:e.pageY-window.pageYOffset)-r.top;"
      "if(s.time){"
        "cx=Math.max(s.plotLeft,Math.min(s.plotRight,cx));"
        "var x0=Math.min(s.firstX,cx),x1=Math.max(s.firstX,cx);"
        "d.style.left=Math.round(r.left+x0)+'px';"
        "d.style.top=Math.round(r.top+s.top)+'px';"
        "d.style.width=Math.max(1,Math.round(x1-x0))+'px';"
        "d.style.height=Math.max(1,Math.round(s.bottom-s.top))+'px';"
      "}else{"
        "cy=Math.max(s.top,Math.min(s.bottom,cy));"
        "var y0=Math.min(s.firstY,cy),y1=Math.max(s.firstY,cy);"
        "d.style.left=Math.round(r.left+s.plotLeft)+'px';"
        "d.style.top=Math.round(r.top+y0)+'px';"
        "d.style.width=Math.max(1,Math.round(s.plotRight-s.plotLeft))+'px';"
        "d.style.height=Math.max(1,Math.round(y1-y0))+'px';"
      "}"
      "}";
  mouseMoved().connect(moveSelectionOverlay);
  mouseDragged().connect(moveSelectionOverlay);

  // The left information-panel width uses the same pattern: a client-side
  // guide follows the pointer, then the expensive waveform repaint happens
  // only once on mouse release.
  const std::string moveLabelResizeOverlay =
      "function(o,e){"
      "var w=o,s=w&&w.__itsvLabelResize,d=w&&w.__itsvLabelResizeOverlay;"
      "if(!w||!s||!d)return;"
      "var r=w.getBoundingClientRect();"
      "var cx=(e.clientX!=null?e.clientX:e.pageX-window.pageXOffset)-r.left;"
      "cx=Math.max(s.min,Math.min(s.max,cx));"
      "d.style.left=Math.round(r.left+cx)+'px';"
      "}";
  mouseMoved().connect(moveLabelResizeOverlay);
  mouseDragged().connect(moveLabelResizeOverlay);

  const std::string moveRowResizeOverlay =
      "function(o,e){"
      "var w=o,s=w&&w.__itsvRowResize,d=w&&w.__itsvRowResizeOverlay;"
      "if(!w||!s||!d)return;"
      "var r=w.getBoundingClientRect();"
      "var y=(e.clientY!=null?e.clientY:e.pageY-window.pageYOffset)-r.top;"
      "var delta=Math.round((y-s.start)/s.ordinal);"
      "var h=Math.max(s.min,Math.min(s.max,s.initial+delta));"
      "d.style.top=Math.round(r.top+s.start+(h-s.initial)*s.ordinal)+'px';"
      "}";
  mouseMoved().connect(moveRowResizeOverlay);
  mouseDragged().connect(moveRowResizeOverlay);

  // Show resize cursors for both dividers.  The full drawing is not
  // repainted on motion; both operations use a temporary browser-side guide.
  const std::string resizeCursor =
      "function(o,e){"
      "if(!o)return;if(o.__itsvLabelResize){o.style.cursor='col-resize';return;}"
      "if(o.__itsvRowResize){o.style.cursor='row-resize';return;}"
      "var r=o.getBoundingClientRect();"
      "var x=(e.clientX!=null?e.clientX:e.pageX-window.pageXOffset)-r.left;"
      "var y=(e.clientY!=null?e.clientY:e.pageY-window.pageYOffset)-r.top;"
      "var boundary=parseInt(o.__itsvPanelWidth||150,10);"
      "var rh=parseInt(o.getAttribute('data-itsv-row-height')||'110',10);"
      "var n=parseInt(o.getAttribute('data-itsv-row-count')||'0',10);"
      "if(Math.abs(x-boundary)<=6&&y>=0&&y<r.height-64){o.style.cursor='col-resize';return;}"
      "var k=Math.round((y+1)/rh);"
      "o.style.cursor=(k>=1&&k<=n&&Math.abs(y-(k*rh-1))<=5)"
      "?'row-resize':'default';"
      "}";
  mouseMoved().connect(resizeCursor);

  setAttributeValue("data-itsv-row-height", std::to_string(rowHeight_));
  setAttributeValue("data-itsv-row-count", "0");


  keyWentDown().connect(this, &WaveformDisplayWidget::onKeyDown);
  keyWentDown().connect(std::string(
      "function(o,e){if(['Escape','ArrowLeft','ArrowRight','ArrowUp','ArrowDown']"
      ".indexOf(e.key)>=0)e.preventDefault();}"));
  blurred().connect(this, &WaveformDisplayWidget::cancelMouseGesture);
}

void WaveformDisplayWidget::setFontSize(int pixels)
{
  const int next = std::clamp(pixels, 10, 12);
  if (fontSize_ == next) return;
  fontSize_ = next;
  timeBarHeight_ = 64 + (fontSize_ - 11) * 4;
  update();
}

int WaveformDisplayWidget::minimumContentHeight() const noexcept
{
  return uniform_rows::contentHeight(state_.visibleChannels().size(),
                                     rowHeight_, timeBarHeight_);
}

int WaveformDisplayWidget::fittedRowHeight(int viewportHeight) const noexcept
{
  return uniform_rows::fitHeight(state_.visibleChannels().size(),
                                 viewportHeight, timeBarHeight_);
}

void WaveformDisplayWidget::setRowHeight(int pixels)
{
  const int next = uniform_rows::clampHeight(pixels);
  if (next == rowHeight_) return;
  cancelMouseGesture();
  rowHeight_ = next;
  setAttributeValue("data-itsv-row-height", std::to_string(next));
  rowHeightChanged_.emit(next);
  update();
}

void WaveformDisplayWidget::setLabelPanelWidth(int width)
{
  const int next = std::clamp(width, 100, 400);
  if (labelPanelWidth_ == next) return;
  labelPanelWidth_ = next;
  if (auto* app = Wt::WApplication::instance())
    app->doJavaScript("(function(){var w=" + jsRef() +
                      ";if(w)w.__itsvPanelWidth=" + std::to_string(next) + ";})();");
  labelPanelWidthChanged_.emit(labelPanelWidth_);
  viewChanged_.emit();
  update();
}

void WaveformDisplayWidget::setRecording(Recording recording)
{
  if (recording.empty() || !recording.timeRange())
    throw std::invalid_argument("Cannot display an empty recording");

  std::optional<Recording> nextRecording(std::move(recording));
  ViewState nextState;
  nextState.resetForRecording(nextRecording->channelCount(), *nextRecording->timeRange());

  recording_.swap(nextRecording);
  state_ = std::move(nextState);
  // Loading another recording resets scaling to Auto: update Ampl menu marks.
  amplitudeModeChanged_.emit(state_.amplitudeMode());
  layouts_.clear();
  setAttributeValue("data-itsv-row-count", std::to_string(state_.visibleChannels().size()));
  cancelMouseGesture();
  dataChanged_.emit();
  refreshView();
  statusText_.emit("Loaded " + std::to_string(recording_->channelCount()) + " MiniSEED channel(s)");
}

void WaveformDisplayWidget::setChannelSelection(std::vector<std::size_t> channels)
{
  if (!recording_) return;
  state_.setBaseChannels(std::move(channels), recording_->channelCount());
  setAttributeValue("data-itsv-row-count", std::to_string(state_.visibleChannels().size()));
  cancelMouseGesture();
  dataChanged_.emit();
  refreshView();
}

void WaveformDisplayWidget::refreshView()
{
  if (const auto range = state_.viewRange()) {
    const double span = range->high - range->low;
    std::ostringstream out;
    out << "UTC: " << formatUtc(range->low, true, span < 60.0)
        << " - " << formatUtc(range->high, true, span < 60.0)
        << "  (" << std::fixed << std::setprecision(span < 60.0 ? 3 : 1)
        << span << " s)";
    statusText_.emit(out.str());
  }
  viewChanged_.emit();
  update();
}

bool WaveformDisplayWidget::setViewRange(Range range)
{
  if (!state_.setViewRange(range)) return false;
  refreshView();
  return true;
}

void WaveformDisplayWidget::fitToData()
{
  cancelMouseGesture();
  state_.fitToData();
  refreshView();
}

void WaveformDisplayWidget::zoomTimeIn()
{
  cancelMouseGesture();
  const auto current = state_.viewRange();
  if (!current) return;
  if (const auto range = mouse::zoom(*current, 1.0 / 6.0, 5.0 / 6.0, false))
    setViewRange(*range);
}

void WaveformDisplayWidget::zoomTimeOut()
{
  cancelMouseGesture();
  const auto current = state_.viewRange();
  if (!current) return;
  if (const auto range = mouse::zoom(*current, 1.0 / 6.0, 5.0 / 6.0, true))
    setViewRange(*range);
}

void WaveformDisplayWidget::shiftTimeLeft()
{
  cancelMouseGesture();
  const auto current = state_.viewRange();
  if (!current) return;
  if (const auto range = mouse::shift(*current, -0.2 * (current->high - current->low)))
    setViewRange(*range);
}

void WaveformDisplayWidget::shiftTimeRight()
{
  cancelMouseGesture();
  const auto current = state_.viewRange();
  if (!current) return;
  if (const auto range = mouse::shift(*current, 0.2 * (current->high - current->low)))
    setViewRange(*range);
}

Range WaveformDisplayWidget::channelAutoRange(std::size_t channelIndex) const
{
  if (!recording_ || channelIndex >= recording_->channelCount() || !state_.viewRange())
    return {-1.0, 1.0};
  auto range = recording_->channel(channelIndex).rangeInTime(*state_.viewRange());
  Range result = range.value_or(Range{-1.0, 1.0});
  if (state_.inverted()) result = {-result.high, -result.low};
  return ensureUsableRange(result);
}

void WaveformDisplayWidget::applyAutoScaling()
{
  if (!recording_ || state_.amplitudeMode() == AmplitudeMode::Fixed) return;
  const auto& visible = state_.visibleChannels();
  if (visible.empty()) return;

  switch (state_.amplitudeMode()) {
    case AmplitudeMode::Fixed:
      return;
    case AmplitudeMode::Auto:
      for (const auto channel : visible)
        state_.setAmplitudeRange(channel, channelAutoRange(channel));
      break;
    case AmplitudeMode::ZeroCentered:
      for (const auto channel : visible) {
        const auto range = channelAutoRange(channel);
        const double amplitude = std::max({1.0, std::abs(range.low), std::abs(range.high)});
        state_.setAmplitudeRange(channel, {-amplitude, amplitude});
      }
      break;
    case AmplitudeMode::FirstTrace: {
      const auto range = channelAutoRange(visible.front());
      for (const auto channel : visible) state_.setAmplitudeRange(channel, range);
      break;
    }
    case AmplitudeMode::AllSame: {
      double amplitude = 1.0;
      for (const auto channel : visible) {
        const auto range = channelAutoRange(channel);
        amplitude = std::max({amplitude, std::abs(range.low), std::abs(range.high)});
      }
      for (const auto channel : visible) state_.setAmplitudeRange(channel, {-amplitude, amplitude});
      break;
    }
  }
}

void WaveformDisplayWidget::chooseAmplitudeMode(AmplitudeMode mode)
{
  state_.setAmplitudeMode(mode);
  // Emit even for repeated selections. WMenuItem checkboxes can otherwise be
  // toggled off by a click on the already-active menu item.
  amplitudeModeChanged_.emit(mode);
}

void WaveformDisplayWidget::freezeAmplitude()
{
  applyAutoScaling();
  chooseAmplitudeMode(AmplitudeMode::Fixed);
}

void WaveformDisplayWidget::setAmplitudeRange(std::size_t channelIndex, Range range)
{
  if (!validRange(range)) return;
  state_.setAmplitudeRange(channelIndex, range);
}

void WaveformDisplayWidget::expandAmplitude()
{
  cancelMouseGesture();
  freezeAmplitude();
  const bool markedOnly = haveMarks();
  for (const auto channel : state_.visibleChannels()) {
    if (markedOnly && !state_.isMarked(channel)) continue;
    const auto range = state_.amplitudeRange(channel);
    const double middle = 0.5 * (range.low + range.high);
    const double half = std::max(1.0e-12, (range.high - range.low) / 2.4);
    setAmplitudeRange(channel, {middle - half, middle + half});
  }
  update();
}

void WaveformDisplayWidget::contractAmplitude()
{
  cancelMouseGesture();
  freezeAmplitude();
  const bool markedOnly = haveMarks();
  for (const auto channel : state_.visibleChannels()) {
    if (markedOnly && !state_.isMarked(channel)) continue;
    const auto range = state_.amplitudeRange(channel);
    const double middle = 0.5 * (range.low + range.high);
    const double half = std::max(1.0e-12, (range.high - range.low) * 0.6);
    setAmplitudeRange(channel, {middle - half, middle + half});
  }
  update();
}

void WaveformDisplayWidget::updateVisibleState(bool dataChanged)
{
  cancelMouseGesture();
  if (dataChanged) {
    setAttributeValue("data-itsv-row-count", std::to_string(state_.visibleChannels().size()));
    dataChanged_.emit();
  }
  refreshView();
}

void WaveformDisplayWidget::restoreTraces()
{
  state_.restoreVisible();
  updateVisibleState(true);
}

void WaveformDisplayWidget::keepMarkedTraces()
{
  if (!state_.keepMarked()) {
    statusText_.emit("Mark channel labels first; no traces were hidden.");
    return;
  }
  updateVisibleState(true);
}

void WaveformDisplayWidget::deleteMarkedTraces()
{
  if (!state_.deleteMarked()) {
    statusText_.emit("Mark channel labels first; no traces were hidden.");
    return;
  }
  updateVisibleState(true);
}

void WaveformDisplayWidget::markAllTraces()
{
  cancelMouseGesture();
  state_.markAllVisible();
  update();
}

void WaveformDisplayWidget::clearMarkedTraces()
{
  cancelMouseGesture();
  state_.clearMarks();
  update();
}

void WaveformDisplayWidget::setAmplitudeAuto()
{
  cancelMouseGesture();
  chooseAmplitudeMode(AmplitudeMode::Auto);
  update();
}

void WaveformDisplayWidget::setAmplitudeFixed()
{
  cancelMouseGesture();
  freezeAmplitude();
  update();
}

void WaveformDisplayWidget::setAmplitudeZeroCentered()
{
  cancelMouseGesture();
  chooseAmplitudeMode(AmplitudeMode::ZeroCentered);
  update();
}

void WaveformDisplayWidget::setAmplitudeFirstTrace()
{
  cancelMouseGesture();
  chooseAmplitudeMode(AmplitudeMode::FirstTrace);
  update();
}

void WaveformDisplayWidget::setAmplitudeAllSame()
{
  cancelMouseGesture();
  chooseAmplitudeMode(AmplitudeMode::AllSame);
  update();
}

bool WaveformDisplayWidget::toggleClip()
{
  const bool enabled = state_.toggleClip();
  update();
  return enabled;
}

void WaveformDisplayWidget::toggleInvert()
{
  cancelMouseGesture();
  state_.toggleInverted();
  viewChanged_.emit(); // preview must repaint with the same polarity
  update();
}

void WaveformDisplayWidget::centerTimeViewAt(double time)
{
  cancelMouseGesture();
  if (!state_.viewRange() || !state_.dataRange()) return;
  if (const auto range = mouse::centerInData(*state_.viewRange(), *state_.dataRange(), time))
    setViewRange(*range);
}

void WaveformDisplayWidget::showTimeAt(double time)
{
  if (std::isfinite(time)) statusText_.emit("UTC: " + formatUtc(time, true, true));
}

void WaveformDisplayWidget::cancelMouseGesture()
{
  stopSelectionOverlay();
  stopLabelResizeOverlay();
  stopRowResizeOverlay();
  mouseZoom_ = MouseZoom::None;
  zoomChannel_.reset();
  middleDrag_ = false;
  labelResize_ = false;
  rowResize_ = false;
  rowResizeDivider_ = 0;
  panelSelectionActive_ = false;
  panelSelectionAdditive_ = false;
  lastPanel_ = -1;
  decorationStyle().setCursor(Wt::Cursor::Auto);
}

void WaveformDisplayWidget::buildLayouts(Wt::WPaintDevice* device)
{
  widgetWidth_ = std::max(0, static_cast<int>(std::lround(device->width().toPixels())));
  widgetHeight_ = std::max(0, static_cast<int>(std::lround(device->height().toPixels())));
  plotLeft_ = std::min(labelPanelWidth_, std::max(0, widgetWidth_ - 1));
  plotRight_ = std::max(plotLeft_, widgetWidth_ - 1);
  layouts_.clear();

  const auto& visible = state_.visibleChannels();
  if (visible.empty()) return;
  const int usableHeight = std::max(0, widgetHeight_ - timeBarHeight_);
  layouts_.reserve(visible.size());
  for (std::size_t screen = 0; screen < visible.size(); ++screen) {
    // Each trace has exactly the same height, independent of count or scroll
    // position.  The enclosing container owns the native vertical scrollbar.
    const long long start = static_cast<long long>(screen) * rowHeight_;
    if (start >= usableHeight) break; // Guard against an oversized canvas.
    const int top = static_cast<int>(start);
    const int bottom = static_cast<int>(std::min<long long>(
        usableHeight - 1, start + rowHeight_ - 1));
    TraceLayout layout;
    layout.channelIndex = visible[screen];
    layout.screenIndex = screen;
    layout.panelTop = top;
    layout.panelBottom = bottom;
    layout.plotTop = top + 1;
    layout.plotBottom = std::max(layout.plotTop + 1, bottom - 2);
    layout.amplitude = state_.amplitudeRange(layout.channelIndex);
    layouts_.push_back(layout);
  }
}

void WaveformDisplayWidget::drawLeftPanel(Wt::WPainter& painter)
{
  painter.setBrush(Wt::WBrush(Wt::WColor(192, 192, 192)));
  painter.setPen(Wt::WPen(Wt::WColor(0, 0, 0)));
  painter.drawRect(0, 0, labelPanelWidth_, widgetHeight_);
  painter.setPen(Wt::WPen(Wt::WColor(145, 145, 145)));
  painter.drawLine(labelPanelWidth_ - 1, 0, labelPanelWidth_ - 1,
                   std::max(0, widgetHeight_ - timeBarHeight_));

  Wt::WFont font;
  font.setFamily(Wt::FontFamily::Monospace);
  font.setSize(Wt::WLength(fontSize_));
  painter.setFont(font);

  for (const auto& layout : layouts_) {
    const bool marked = state_.isMarked(layout.channelIndex);
    const bool overview = state_.overviewChannel() && *state_.overviewChannel() == layout.channelIndex;
    painter.setBrush(Wt::WBrush(marked ? Wt::WColor(255, 237, 181) :
                                 (overview ? Wt::WColor(189, 214, 238) : Wt::WColor(210, 210, 210))));
    painter.setPen(Wt::WPen(Wt::WColor(160, 160, 160)));
    painter.drawRect(0, layout.panelTop, labelPanelWidth_,
                     std::max(1, layout.panelBottom - layout.panelTop + 1));
    if (overview) {
      painter.setPen(Wt::WPen(Wt::WColor(27, 102, 188)));
      painter.setBrush(Wt::WBrush(Wt::WColor(27, 102, 188)));
      painter.drawRect(0, layout.panelTop, 4,
                       std::max(1, layout.panelBottom - layout.panelTop + 1));
    }

    const auto& channel = recording_->channel(layout.channelIndex);
    painter.setPen(Wt::WPen(Wt::WColor(0, 0, 0)));
    drawText(painter, 6, layout.panelTop + 4, labelPanelWidth_ - 38, 16,
             channel.id.displayName());
    if (marked) {
      painter.setPen(Wt::WPen(Wt::WColor(170, 25, 25)));
      drawText(painter, labelPanelWidth_ - 30, layout.panelTop + 4, 24, 16,
               std::to_string(state_.markNumber(layout.channelIndex) + 1),
               Wt::AlignmentFlag::Right);
    }
    painter.setPen(Wt::WPen(Wt::WColor(0, 0, 0)));
    std::ostringstream high, low;
    high << "max=" << std::setprecision(7) << layout.amplitude.high;
    low << "min=" << std::setprecision(7) << layout.amplitude.low;
    drawText(painter, 6, layout.panelTop + 21, labelPanelWidth_ - 12, 16, high.str());
    drawText(painter, 6, layout.panelTop + 38, labelPanelWidth_ - 12, 16, low.str());
  }
}

void WaveformDisplayWidget::drawTrace(const TraceLayout& layout, Wt::WPainter& painter)
{
  if (!recording_ || !state_.viewRange() || plotRight_ <= plotLeft_ ||
      layout.plotBottom <= layout.plotTop) return;
  const auto view = *state_.viewRange();
  const auto amplitude = ensureUsableRange(layout.amplitude);
  const auto& channel = recording_->channel(layout.channelIndex);

  painter.setBrush(Wt::WBrush(Wt::WColor(0, 0, 192)));
  painter.setPen(Wt::WPen(Wt::WColor(0, 0, 192)));
  painter.drawRect(plotLeft_, layout.panelTop, plotRight_ - plotLeft_,
                   std::max(1, layout.panelBottom - layout.panelTop + 1));
  painter.setPen(Wt::WPen(Wt::WColor(255, 255, 0)));

  auto screenY = [&](double value) -> int {
    if (state_.inverted()) value = -value;
    return geometry::amplitudeY(value, amplitude, layout.plotTop, layout.plotBottom,
                                state_.clipEnabled(), widgetHeight_);
  };

  for (const auto& segment : channel.segments) {
    if (segment.endTime() < view.low || segment.startTime() > view.high) continue;
    const auto& samples = segment.samples();
    const double interval = segment.sampleInterval();
    const double start = segment.startTime();
    const auto indices = segment.sampleIndexRange(view);
    if (!indices) continue;
    const auto [first, last] = *indices;

    int column = -1;
    int previousColumn = -1;
    int previousLastY = 0;
    double low = 0.0, high = 0.0, firstSample = 0.0, lastSample = 0.0;

    auto flush = [&]() {
      if (column < 0) return;
      const int yLow = screenY(low);
      const int yHigh = screenY(high);
      const int yFirst = screenY(firstSample);
      const int yLast = screenY(lastSample);
      if (previousColumn >= 0)
        painter.drawLine(previousColumn, previousLastY, column, yFirst);
      painter.drawLine(column, std::min(yLow, yHigh), column, std::max(yLow, yHigh));
      previousColumn = column;
      previousLastY = yLast;
    };

    for (std::size_t i = first; i <= last; ++i) {
      const double value = samples[i];
      if (!std::isfinite(value)) {
        flush();
        column = -1;
        previousColumn = -1;
        continue;
      }
      const double time = start + static_cast<double>(i) * interval;
      const int x = geometry::timeX(time, view, plotLeft_, plotRight_);
      if (x != column) {
        flush();
        column = x;
        low = high = firstSample = lastSample = value;
      } else {
        low = std::min(low, value);
        high = std::max(high, value);
        lastSample = value;
      }
    }
    flush();
  }
}

void WaveformDisplayWidget::drawRowDividers(Wt::WPainter& painter)
{
  // Every horizontal boundary, including the bottom edge of the final trace,
  // acts as a handle for resizing all the traces to the same new height.
  for (const auto& layout : layouts_) {
    const int y = layout.panelBottom;
    painter.setPen(Wt::WPen(Wt::WColor(142, 160, 183)));
    painter.drawLine(0, y, std::max(0, widgetWidth_ - 1), y);
    painter.setPen(Wt::WPen(Wt::WColor(85, 100, 118)));
    painter.drawLine(std::max(8, labelPanelWidth_ - 33), y - 2,
                     std::max(9, labelPanelWidth_ - 10), y - 2);
    painter.drawLine(std::max(8, labelPanelWidth_ - 33), y,
                     std::max(9, labelPanelWidth_ - 10), y);
  }
}

void WaveformDisplayWidget::drawTimeBounds(Wt::WPainter& painter)
{
  if (!state_.viewRange()) return;
  const auto range = *state_.viewRange();
  const int top = widgetHeight_ - timeBarHeight_;
  const bool milliseconds = (range.high - range.low) < 20.0;
  painter.setPen(Wt::WPen(Wt::WColor(25, 25, 25)));
  drawText(painter, plotLeft_ + 4, top + 12, 140, 16, formatUtc(range.low, false, milliseconds));
  drawText(painter, plotLeft_ + 4, top + 28, 140, 16, formatUtc(range.low, true).substr(0, 10));
  if (plotRight_ - plotLeft_ > 310) {
    drawText(painter, plotRight_ - 144, top + 12, 140, 16,
             formatUtc(range.high, false, milliseconds), Wt::AlignmentFlag::Right);
    drawText(painter, plotRight_ - 144, top + 28, 140, 16,
             formatUtc(range.high, true).substr(0, 10), Wt::AlignmentFlag::Right);
  }
  painter.setPen(Wt::WPen(Wt::WColor(85, 85, 85)));
  painter.drawLine(plotLeft_ + 1, top + 2, plotLeft_ + 1, top + 10);
  painter.drawLine(plotRight_ - 1, top + 2, plotRight_ - 1, top + 10);
}

void WaveformDisplayWidget::drawTimeMark(int x, double time, Wt::WPainter& painter)
{
  if (!state_.viewRange() || x - 52 < lastTimeLabelRight_ + 5) return;
  const int top = widgetHeight_ - timeBarHeight_;
  const bool milliseconds = (state_.viewRange()->high - state_.viewRange()->low) < 20.0;
  painter.setPen(Wt::WPen(Wt::WColor(85, 85, 85)));
  painter.drawLine(x, top + 2, x, top + 9);
  drawText(painter, x - 64, top + 12, 128, 16, formatUtc(time, false, milliseconds),
           Wt::AlignmentFlag::Center);
  drawText(painter, x - 64, top + 28, 128, 16, formatUtc(time, true).substr(0, 10),
           Wt::AlignmentFlag::Center);
  lastTimeLabelRight_ = x + 64;
}

void WaveformDisplayWidget::drawTimePanel(Wt::WPainter& painter)
{
  if (!state_.viewRange() || plotRight_ <= plotLeft_) return;
  const auto range = *state_.viewRange();
  const int top = widgetHeight_ - timeBarHeight_;
  painter.setPen(Wt::WPen(Wt::WColor(195, 195, 195)));
  painter.setBrush(Wt::WBrush(Wt::WColor(250, 250, 250)));
  painter.drawRect(plotLeft_, top, plotRight_ - plotLeft_, timeBarHeight_ - 1);
  painter.setPen(Wt::WPen(Wt::WColor(55, 55, 55)));
  painter.drawLine(plotLeft_, top + 2, plotRight_, top + 2);

  Wt::WFont font;
  font.setFamily(Wt::FontFamily::Monospace);
  font.setSize(Wt::WLength(fontSize_));
  painter.setFont(font);
  drawTimeBounds(painter);

  const double span = range.high - range.low;
  const double step = niceTickStep(span * 155.0 / double(plotRight_ - plotLeft_));
  const double first = std::ceil(range.low / step) * step;
  const int maxTicks = std::max(1, (plotRight_ - plotLeft_) / 40 + 2);
  lastTimeLabelRight_ = plotLeft_ + 128;
  for (int i = 0; i < maxTicks; ++i) {
    const double tick = first + step * i;
    if (tick >= range.high - step * 0.001) break;
    if (tick <= range.low + step * 0.001) continue;
    const int x = static_cast<int>(std::lround(
        plotLeft_ + (tick - range.low) * (plotRight_ - plotLeft_) / span));
    if (x <= plotLeft_ + 112 || x >= plotRight_ - 112) continue;
    drawTimeMark(x, tick, painter);
  }
}

void WaveformDisplayWidget::paintEvent(Wt::WPaintDevice* device)
{
  Wt::WPainter painter(device);
  applyAutoScaling();
  buildLayouts(device);

  Wt::WFont uiFont;
  uiFont.setFamily(Wt::FontFamily::Monospace);
  uiFont.setSize(Wt::WLength(fontSize_));
  painter.setFont(uiFont);

  painter.setBrush(Wt::WBrush(Wt::WColor(255, 255, 255)));
  painter.setPen(Wt::WPen(Wt::WColor(255, 255, 255)));
  painter.drawRect(0, 0, widgetWidth_, widgetHeight_);

  if (!recording_) {
    painter.setPen(Wt::WPen(Wt::WColor(75, 75, 75)));
    drawText(painter, labelPanelWidth_ + 18, 24,
             std::max(0, widgetWidth_ - labelPanelWidth_ - 36), 22,
             "Open a MiniSEED file to display waveforms");
    return;
  }
  if (layouts_.empty()) {
    painter.setPen(Wt::WPen(Wt::WColor(75, 75, 75)));
    drawText(painter, labelPanelWidth_ + 18, 24,
             std::max(0, widgetWidth_ - labelPanelWidth_ - 36), 22,
             "No traces displayed. Use Traces > Original traces/order or File > Select channels.");
    return;
  }

  drawLeftPanel(painter);
  drawTimePanel(painter);
  for (const auto& layout : layouts_) drawTrace(layout, painter);
  drawRowDividers(painter);
}

int WaveformDisplayWidget::screenAt(int x, int y, bool labels) const
{
  if (y < 0 || y >= widgetHeight_ - timeBarHeight_) return -1;
  if (labels ? (x < 0 || x >= labelPanelWidth_) : (x < plotLeft_ || x > plotRight_)) return -1;
  for (const auto& layout : layouts_) {
    const int top = labels ? layout.panelTop : layout.plotTop;
    const int bottom = labels ? layout.panelBottom : layout.plotBottom;
    if (y >= top && y <= bottom) return static_cast<int>(layout.screenIndex);
  }
  return -1;
}

void WaveformDisplayWidget::selectLabel(int screenIndex, bool additive)
{
  if (screenIndex < 0) return;
  const bool changed = additive ? state_.toggleVisibleMark(static_cast<std::size_t>(screenIndex))
                                : state_.selectOnlyVisible(static_cast<std::size_t>(screenIndex));
  if (!changed || !state_.overviewChannel() || !recording_) return;
  const auto channel = *state_.overviewChannel();
  const auto& id = recording_->channel(channel).id;
  statusText_.emit(std::to_string(state_.markedChannels().size()) +
                   " trace(s) marked; overview: " + id.displayName());
  viewChanged_.emit();
  update();
}

double WaveformDisplayWidget::zoomFraction(int x, int y) const
{
  if (mouseZoom_ == MouseZoom::TimeIn || mouseZoom_ == MouseZoom::TimeOut) {
    if (plotRight_ <= plotLeft_) return 0.0;
    return std::clamp(double(x - plotLeft_) / double(plotRight_ - plotLeft_), 0.0, 1.0);
  }
  if (!zoomChannel_) return 0.0;
  const auto it = std::find_if(layouts_.begin(), layouts_.end(),
                               [&](const TraceLayout& layout) { return layout.channelIndex == *zoomChannel_; });
  if (it == layouts_.end() || it->plotBottom <= it->plotTop) return 0.0;
  return std::clamp(double(it->plotBottom - y) / double(it->plotBottom - it->plotTop), 0.0, 1.0);
}

void WaveformDisplayWidget::beginZoom(MouseZoom mode, std::optional<std::size_t> channel, int x, int y)
{
  if (!state_.viewRange() || plotRight_ <= plotLeft_) return;
  const bool time = mode == MouseZoom::TimeIn || mode == MouseZoom::TimeOut;
  if (!time && !channel) return;
  mouseZoom_ = mode;
  zoomChannel_ = channel;
  zoomRange_ = time ? *state_.viewRange() : state_.amplitudeRange(*channel);
  if (!validRange(zoomRange_)) {
    cancelMouseGesture();
    return;
  }
  zoomFirst_ = zoomFraction(x, y);
  decorationStyle().setCursor(Wt::Cursor::Cross);
  const bool out = mode == MouseZoom::TimeOut || mode == MouseZoom::AmplitudeOut;
  statusText_.emit(std::string(time ? "Time" : "Amplitude") +
                   (out ? " zoom out" : " zoom in") +
                   ": move the pointer and click again; Esc cancels.");
  startSelectionOverlay(time, out, x, y);
}

void WaveformDisplayWidget::finishZoom(int x, int y)
{
  const bool time = mouseZoom_ == MouseZoom::TimeIn || mouseZoom_ == MouseZoom::TimeOut;
  const bool out = mouseZoom_ == MouseZoom::TimeOut || mouseZoom_ == MouseZoom::AmplitudeOut;
  const auto channel = zoomChannel_;
  const double end = zoomFraction(x, y);
  double pixels = time ? double(plotRight_ - plotLeft_) : 0.0;
  if (!time && channel) {
    const auto it = std::find_if(layouts_.begin(), layouts_.end(),
                                 [&](const TraceLayout& layout) { return layout.channelIndex == *channel; });
    if (it != layouts_.end()) pixels = it->plotBottom - it->plotTop;
  }
  const auto result = std::abs(end - zoomFirst_) * pixels >= 2.0
      ? mouse::zoom(zoomRange_, zoomFirst_, end, out) : std::nullopt;
  cancelMouseGesture();
  if (!result) {
    statusText_.emit("Zoom cancelled: select an interval at least two pixels wide.");
    return;
  }
  if (time) {
    setViewRange(*result);
  } else if (channel) {
    freezeAmplitude();
    setAmplitudeRange(*channel, *result);
    statusText_.emit("Amplitude zoom applied; Ampl: Fixed.");
    update();
  }
}

void WaveformDisplayWidget::startSelectionOverlay(bool timeZoom, bool zoomOut, int x, int y)
{
  auto* app = Wt::WApplication::instance();
  if (!app || plotRight_ <= plotLeft_) return;

  int top = 0;
  int bottom = std::max(1, widgetHeight_ - timeBarHeight_);
  int firstX = std::clamp(x, plotLeft_, plotRight_);
  int firstY = y;

  if (!timeZoom) {
    if (!zoomChannel_) return;
    const auto it = std::find_if(layouts_.begin(), layouts_.end(),
                                 [&](const TraceLayout& layout) {
                                   return layout.channelIndex == *zoomChannel_;
                                 });
    if (it == layouts_.end()) return;
    top = it->plotTop;
    bottom = std::max(top + 1, it->plotBottom);
    firstY = std::clamp(y, top, bottom);
  }

  std::ostringstream js;
  js << "(function(){var w=" << jsRef() << ";if(!w)return;"
     << "if(w.__itsvSelectionOverlay){w.__itsvSelectionOverlay.remove();}"
     << "var d=document.createElement('div');"
     << "d.style.position='fixed';d.style.pointerEvents='none';"
     << "d.style.zIndex='2147483647';d.style.boxSizing='border-box';"
     << "d.style.border='1px dashed " << (zoomOut ? "rgb(255,164,52)" : "rgb(160,210,160)") << "';"
     << "d.style.background='" << (zoomOut ? "rgba(255,164,52,0.25)" : "rgba(235,255,235,0.32)") << "';"
     << "document.body.appendChild(d);w.__itsvSelectionOverlay=d;"
     << "w.__itsvSelection={time:" << (timeZoom ? "true" : "false")
     << ",firstX:" << firstX << ",firstY:" << firstY
     << ",plotLeft:" << plotLeft_ << ",plotRight:" << plotRight_
     << ",top:" << top << ",bottom:" << bottom << "};"
     << "var r=w.getBoundingClientRect();"
     << "if(w.__itsvSelection.time){"
     << "d.style.left=Math.round(r.left+" << firstX << ")+'px';"
     << "d.style.top=Math.round(r.top+" << top << ")+'px';"
     << "d.style.width='1px';d.style.height='" << std::max(1, bottom - top) << "px';"
     << "}else{"
     << "d.style.left=Math.round(r.left+" << plotLeft_ << ")+'px';"
     << "d.style.top=Math.round(r.top+" << firstY << ")+'px';"
     << "d.style.width='" << std::max(1, plotRight_ - plotLeft_) << "px';d.style.height='1px';"
     << "}})();";
  app->doJavaScript(js.str());
}

void WaveformDisplayWidget::stopSelectionOverlay()
{
  if (auto* app = Wt::WApplication::instance()) {
    app->doJavaScript("(function(){var w=" + jsRef() +
                      ";if(!w)return;if(w.__itsvSelectionOverlay){"
                      "w.__itsvSelectionOverlay.remove();w.__itsvSelectionOverlay=null;}"
                      "w.__itsvSelection=null;})();");
  }
}

void WaveformDisplayWidget::startLabelResizeOverlay(int x)
{
  auto* app = Wt::WApplication::instance();
  if (!app) return;
  const int firstX = std::clamp(x, 100, 400);
  const int height = std::max(1, widgetHeight_ - timeBarHeight_);
  std::ostringstream js;
  js << "(function(){var w=" << jsRef() << ";if(!w)return;"
     << "if(w.__itsvLabelResizeOverlay){w.__itsvLabelResizeOverlay.remove();}"
     << "var d=document.createElement('div');"
     << "d.style.position='fixed';d.style.pointerEvents='none';"
     << "d.style.zIndex='2147483647';d.style.width='2px';"
     << "d.style.background='rgba(30,100,190,0.85)';"
     << "document.body.appendChild(d);w.__itsvLabelResizeOverlay=d;"
     << "w.__itsvLabelResize={min:100,max:400};"
     << "var r=w.getBoundingClientRect();"
     << "d.style.left=Math.round(r.left+" << firstX << ")+'px';"
     << "d.style.top=Math.round(r.top)+'px';"
     << "d.style.height='" << height << "px';})();";
  app->doJavaScript(js.str());
}

void WaveformDisplayWidget::stopLabelResizeOverlay()
{
  if (auto* app = Wt::WApplication::instance()) {
    app->doJavaScript("(function(){var w=" + jsRef() +
                      ";if(!w)return;if(w.__itsvLabelResizeOverlay){"
                      "w.__itsvLabelResizeOverlay.remove();w.__itsvLabelResizeOverlay=null;}"
                      "w.__itsvLabelResize=null;})();");
  }
}

void WaveformDisplayWidget::startRowResizeOverlay(int y, int ordinal)
{
  auto* app = Wt::WApplication::instance();
  if (!app) return;
  std::ostringstream js;
  js << "(function(){var w=" << jsRef() << ";if(!w)return;"
     << "if(w.__itsvRowResizeOverlay)w.__itsvRowResizeOverlay.remove();"
     << "var d=document.createElement('div');"
     << "d.style.cssText='position:fixed;height:2px;background:#36a1e9;"
        "pointer-events:none;z-index:2147483647';"
     << "document.body.appendChild(d);w.__itsvRowResizeOverlay=d;"
     << "w.__itsvRowResize={start:" << y
     << ",ordinal:" << ordinal << ",initial:" << rowHeight_
     << ",min:" << uniform_rows::minimumHeight
     << ",max:" << uniform_rows::maximumHeight << "};"
     << "var r=w.getBoundingClientRect();d.style.left=Math.round(r.left)+'px';"
     << "d.style.width=Math.round(r.width)+'px';"
     << "d.style.top=Math.round(r.top+" << y << ")+'px';})();";
  app->doJavaScript(js.str());
}

void WaveformDisplayWidget::stopRowResizeOverlay()
{
  if (auto* app = Wt::WApplication::instance()) {
    app->doJavaScript("(function(){var w=" + jsRef() +
                      ";if(!w)return;if(w.__itsvRowResizeOverlay){"
                      "w.__itsvRowResizeOverlay.remove();w.__itsvRowResizeOverlay=null;}"
                      "w.__itsvRowResize=null;})();");
  }
}

void WaveformDisplayWidget::showCursorAt(int x, int y)
{
  const int screen = screenAt(x, y, false);
  if (screen < 0 || !state_.viewRange() || plotRight_ <= plotLeft_ || !recording_) return;
  const auto& layout = layouts_[static_cast<std::size_t>(screen)];
  const auto view = *state_.viewRange();
  const double time = view.low + double(x - plotLeft_) * (view.high - view.low) /
                                  double(plotRight_ - plotLeft_);
  const double fraction = double(layout.plotBottom - y) /
                          std::max(1, layout.plotBottom - layout.plotTop);
  double amplitude = layout.amplitude.low + fraction *
                     (layout.amplitude.high - layout.amplitude.low);
  if (state_.inverted()) amplitude = -amplitude;
  const auto& id = recording_->channel(layout.channelIndex).id;
  std::ostringstream text;
  text << id.displayName() << "  A: " << std::fixed << std::setprecision(3)
       << amplitude << "  UTC: " << formatUtc(time, true, true);
  statusText_.emit(text.str());
}

void WaveformDisplayWidget::onMouseDown(const Wt::WMouseEvent& event)
{
  setFocus(true);
  mouseX_ = event.widget().x;
  mouseY_ = event.widget().y;
  const bool left = event.button() == Wt::MouseButton::Left;
  const bool right = event.button() == Wt::MouseButton::Right;
  const int screen = screenAt(mouseX_, mouseY_, false);

  if (left && std::abs(mouseX_ - labelPanelWidth_) <= 6 &&
      mouseY_ >= 0 && mouseY_ < widgetHeight_ - timeBarHeight_ &&
      !event.modifiers().test(Wt::KeyboardModifier::Shift) &&
      !event.modifiers().test(Wt::KeyboardModifier::Control) &&
      !event.modifiers().test(Wt::KeyboardModifier::Alt) &&
      !event.modifiers().test(Wt::KeyboardModifier::Meta)) {
    cancelMouseGesture();
    labelResize_ = true;
    startLabelResizeOverlay(mouseX_);
    statusText_.emit("Drag to resize the left information panel.");
    return;
  }

  if (left && !event.modifiers().test(Wt::KeyboardModifier::Shift) &&
      !event.modifiers().test(Wt::KeyboardModifier::Control) &&
      !event.modifiers().test(Wt::KeyboardModifier::Alt) &&
      !event.modifiers().test(Wt::KeyboardModifier::Meta) &&
      mouseX_ >= 0 && mouseX_ < widgetWidth_) {
    const int ordinal = uniform_rows::dividerAt(
        mouseY_, state_.visibleChannels().size(), rowHeight_);
    if (ordinal > 0 && mouseY_ < widgetHeight_ - timeBarHeight_) {
      cancelMouseGesture();
      rowResize_ = true;
      rowResizeDivider_ = ordinal;
      rowResizeInitialHeight_ = rowHeight_;
      rowResizeStartY_ = mouseY_;
      startRowResizeOverlay(mouseY_, ordinal);
      statusText_.emit("Drag vertically to resize all waveform rows equally.");
      return;
    }
  }

  if (event.button() == Wt::MouseButton::Middle) {
    if (screen < 0 || !state_.viewRange() || plotRight_ <= plotLeft_) return;
    cancelMouseGesture();
    middleDrag_ = true;
    panStartX_ = mouseX_;
    panWidth_ = plotRight_ - plotLeft_;
    panRange_ = *state_.viewRange();
    decorationStyle().setCursor(Wt::Cursor::OpenHand);
    return;
  }
  if (!left && !right) return;

  const int label = screenAt(mouseX_, mouseY_, true);
  if (left && label >= 0) {
    cancelMouseGesture();
    panelSelectionActive_ = true;
    panelSelectionAdditive_ = event.modifiers().test(Wt::KeyboardModifier::Control);
    lastPanel_ = label;
    selectLabel(label, panelSelectionAdditive_);
    return;
  }

  if (mouseZoom_ != MouseZoom::None) {
    if (mouseX_ >= plotLeft_ && mouseX_ <= plotRight_ && mouseY_ >= 0 && mouseY_ < widgetHeight_)
      finishZoom(mouseX_, mouseY_);
    return;
  }

  const bool shift = event.modifiers().test(Wt::KeyboardModifier::Shift);
  const bool control = event.modifiers().test(Wt::KeyboardModifier::Control);
  if (shift && mouseX_ >= plotLeft_ && mouseX_ <= plotRight_ && !layouts_.empty()) {
    beginZoom(left && !control ? MouseZoom::TimeIn : MouseZoom::TimeOut,
              std::nullopt, mouseX_, mouseY_);
    return;
  }
  if (screen < 0 || !state_.viewRange()) return;
  const auto channel = layouts_[static_cast<std::size_t>(screen)].channelIndex;
  if (control) {
    beginZoom(left ? MouseZoom::AmplitudeIn : MouseZoom::AmplitudeOut,
              channel, mouseX_, mouseY_);
    return;
  }
  if (event.modifiers().test(Wt::KeyboardModifier::Alt) ||
      event.modifiers().test(Wt::KeyboardModifier::Meta)) return;

  const double fraction = std::clamp(double(mouseX_ - plotLeft_) /
                                      double(plotRight_ - plotLeft_), 0.0, 1.0);
  const auto view = *state_.viewRange();
  if (const auto range = mouse::shift(view,
      (left ? 1.0 : -1.0) * fraction * (view.high - view.low)))
    setViewRange(*range);
}

void WaveformDisplayWidget::onMouseUp(const Wt::WMouseEvent& event)
{
  mouseX_ = event.widget().x;
  mouseY_ = event.widget().y;
  if (event.button() == Wt::MouseButton::Left && rowResize_) {
    const int next = uniform_rows::heightFromDrag(
        rowResizeInitialHeight_, rowResizeDivider_, mouseY_ - rowResizeStartY_);
    rowResize_ = false;
    rowResizeDivider_ = 0;
    stopRowResizeOverlay();
    setRowHeight(next);
    statusText_.emit("All waveform heights: " + std::to_string(rowHeight_) + " px");
    return;
  }
  if (event.button() == Wt::MouseButton::Left && labelResize_) {
    labelResize_ = false;
    stopLabelResizeOverlay();
    setLabelPanelWidth(mouseX_);
    statusText_.emit("Left information panel width: " + std::to_string(labelPanelWidth_) + " px");
    return;
  }
  if (event.button() == Wt::MouseButton::Middle && middleDrag_) {
    const auto range = mouse::pan(panRange_, mouseX_ - panStartX_, panWidth_);
    middleDrag_ = false;
    decorationStyle().setCursor(Wt::Cursor::Auto);
    if (range) setViewRange(*range);
  }
  if (event.button() == Wt::MouseButton::Left || event.button() == Wt::MouseButton::Right) {
    panelSelectionActive_ = false;
    panelSelectionAdditive_ = false;
    lastPanel_ = -1;
  }
}

void WaveformDisplayWidget::onMouseMove(const Wt::WMouseEvent& event)
{
  mouseX_ = event.widget().x;
  mouseY_ = event.widget().y;
  if (labelResize_ || rowResize_) {
    // Browser-side guide follows the pointer. Repaint only after release.
    return;
  }
  if (middleDrag_) {
    // Wt guarantees mouse-up capture after mouse-down. Do not depend on
    // WMouseEvent::button() being populated for every move event.
    if (const auto range = mouse::pan(panRange_, mouseX_ - panStartX_, panWidth_))
      setViewRange(*range);
    return;
  }
  if (panelSelectionActive_) {
    const int label = screenAt(mouseX_, mouseY_, true);
    if (label >= 0 && label != lastPanel_) selectLabel(label, panelSelectionAdditive_);
    lastPanel_ = label;
    return;
  }
  if (mouseZoom_ != MouseZoom::None) {
    // The browser-side overlay follows the pointer.  Avoid repainting all
    // waveforms for a temporary rectangle; finishZoom() uses the final click.
    return;
  }
  showCursorAt(mouseX_, mouseY_);
}

void WaveformDisplayWidget::onKeyDown(const Wt::WKeyEvent& event)
{
  switch (event.key()) {
    case Wt::Key::Escape:
      cancelMouseGesture();
      statusText_.emit("Mouse operation cancelled.");
      break;
    case Wt::Key::Left: shiftTimeLeft(); break;
    case Wt::Key::Right: shiftTimeRight(); break;
    case Wt::Key::Up: expandAmplitude(); break;
    case Wt::Key::Down: contractAmplitude(); break;
    default: break;
  }
}

} // namespace itsv
