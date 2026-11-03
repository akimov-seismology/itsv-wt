// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
#include "AmplitudeMenuModes.h"
#include "MouseNavigation.h"
#include "PlotGeometry.h"
#include "PreviewSelectionGeometry.h"
#include "ImportBudget.h"
#include "Recording.h"
#include "ViewState.h"
#include "UniformRows.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;

void check(bool condition, const std::string& message)
{
  ++checks;
  if (!condition) throw std::runtime_error(message);
}

void rangeIs(const std::optional<itsv::Range>& actual, double low, double high,
             const std::string& message, double tolerance = 1e-9)
{
  check(actual.has_value(), message + " (missing range)");
  check(std::abs(actual->low - low) <= tolerance &&
        std::abs(actual->high - high) <= tolerance, message);
}

void navigationTests()
{
  using namespace itsv;
  using namespace itsv::mouse;
  const Range current{1000.0, 1120.0};
  rangeIs(zoom(current, 0.25, 0.75, false), 1030.0, 1090.0, "time zoom in");
  rangeIs(zoom(current, 0.25, 0.75, true), 940.0, 1180.0, "time zoom out");
  rangeIs(shift(current, 30.0), 1030.0, 1150.0, "time shift");
  rangeIs(pan(current, 100.0, 400.0), 970.0, 1090.0, "time pan");
  rangeIs(centerInData({100.0, 300.0}, {-1000.0, 1000.0}, -900.0),
          -1000.0, -800.0, "pre-1970 overview clamp");
  check(!zoom(current, 0.5, 0.5, false), "reject zero-width zoom");
  check(!pan(current, 1.0, 0.0), "reject invalid pan width");
}


void previewSelectionTests()
{
  using namespace itsv;
  const Range recording{1000.0, 2000.0};
  const Range selected{1250.0, 1750.0};
  const auto band = previewTimeBand(recording, selected, 100.0, 800.0);
  check(band.has_value(), "overview interval exists");
  check(std::abs(band->left - 300.0) < 1e-9 &&
        std::abs(band->right - 700.0) < 1e-9,
        "overview interval aligns with visible time bounds");
  const auto rect = previewSelectionRect(*band, 100.0, 5.0, 800.0, 100.0);
  check(rect.has_value(), "overview rectangle exists");
  check(rect->x == 300.0 && rect->width == 400.0,
        "rectangle left/right align with time markers");
  check(rect->y >= 9.0 && rect->y + rect->height <= 101.0,
        "rectangle top/bottom are inset from plot edges");
  check(rect->height > 0.0, "rectangle has visible horizontal edges");

  const auto clipped = previewTimeBand(recording, {900.0, 1500.0}, 100.0, 800.0);
  check(clipped && clipped->left == 100.0 && clipped->right == 500.0,
        "visible interval clipped at start of recording");
  const auto full = previewTimeBand(recording, recording, 100.0, 800.0);
  const auto fullRect = full ? previewSelectionRect(*full, 100.0, 5.0, 800.0, 100.0) : std::nullopt;
  check(fullRect && fullRect->x == 102.0 && fullRect->width == 796.0,
        "full-width interval outline stays inside blue plot");
  const auto narrow = previewTimeBand(recording, {1500.0, 1500.001}, 100.0, 800.0);
  const auto narrowRect = narrow ? previewSelectionRect(*narrow, 100.0, 5.0, 800.0, 100.0) : std::nullopt;
  check(narrowRect && narrowRect->width >= 3.0,
        "very narrow interval remains a box, not one line");
  check(!previewTimeBand(recording, {2001.0, 2100.0}, 100.0, 800.0),
        "non-overlapping view has no misleading selection");
  check(!previewTimeBand(recording, selected, 100.0, 0.0),
        "invalid preview width produces no rectangle");
  check(!previewSelectionRect(*band, 100.0, 5.0, 800.0, 3.0),
        "too-short preview cannot draw a clipped rectangle");
}

void recordingTests()
{
  using namespace itsv;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> samples(700);
  for (std::size_t i = 0; i < samples.size(); ++i) samples[i] = static_cast<double>(i) - 350.0;
  samples[100] = nan;
  samples[600] = 5000.0;

  Segment segment(-10.0, 0.5, samples);
  check(std::abs(segment.endTime() - 339.5) < 1e-12, "segment end time");
  rangeIs(segment.fullRange(), -350.0, 5000.0, "full extrema ignores NaN");
  rangeIs(segment.rangeInTime({-10.0, -5.0}), -350.0, -340.0, "partial extrema");
  rangeIs(segment.rangeInTime({290.0, 291.0}), 251.0, 5000.0, "block extrema includes spike");
  check(!segment.rangeInTime({1000.0, 1010.0}), "outside segment has no extrema");

  Channel first{{"AZ", "AAA", "00", "BHZ"}, {segment}};
  Channel second{{"BK", "BBB", "", "HHN"}, {Segment(500.0, 1.0, {1.25, 2.5, 3.75})}};
  Recording recording({first, second});
  check(recording.channelCount() == 2, "recording channel count");
  check(recording.channel(0).id.station == "AAA" && recording.channel(1).id.station == "BBB",
        "recording preserves source order");
  rangeIs(recording.timeRange(), -10.0, 502.0, "recording accepts pre-1970 time");
  check(recording.channel(1).id.seedName() == "BK.BBB..HHN", "SEED name formatting");
}

void viewStateTests()
{
  using namespace itsv;
  ViewState state;
  state.resetForRecording(4, {-100.0, 100.0});
  check(state.visibleChannels() == std::vector<std::size_t>({0,1,2,3}), "all channels initially visible");
  check(state.overviewChannel() && *state.overviewChannel() == 0, "initial overview channel");

  check(state.selectOnlyVisible(2), "exclusive label selection succeeds");
  check(state.markedChannels() == std::vector<std::size_t>({2}), "exclusive label selection");
  check(state.toggleVisibleMark(0), "additive selection succeeds");
  check(state.toggleVisibleMark(3), "second additive selection succeeds");
  check(state.markedChannels() == std::vector<std::size_t>({2,0,3}), "marks preserve click order");
  check(state.markNumber(0) == 1 && state.markNumber(3) == 2, "mark numbers follow click order");

  check(state.keepMarked(), "keep marked succeeds");
  check(state.visibleChannels() == std::vector<std::size_t>({2,0,3}), "keep marked uses mark order");
  check(state.markedChannels().empty(), "keep marked clears marks");
  state.restoreVisible();
  check(state.visibleChannels() == std::vector<std::size_t>({0,1,2,3}), "restore returns base order");

  state.selectOnlyVisible(1);
  check(state.deleteMarked(), "delete marked succeeds");
  check(state.visibleChannels() == std::vector<std::size_t>({0,2,3}), "delete preserves survivor order");
  state.restoreVisible();

  state.setBaseChannels({3,1,3}, 4);
  check(state.baseChannels() == std::vector<std::size_t>({3,1}), "base selection removes duplicate indices");
  check(state.visibleChannels() == std::vector<std::size_t>({3,1}), "base selection sets visible order");
  state.markAllVisible();
  check(state.markedChannels() == std::vector<std::size_t>({3,1}), "mark all follows visible order");
  state.clearMarks();
  check(state.markedChannels().empty(), "clear marks");

  check(state.setViewRange({-50.0, -25.0}), "negative UTC view is accepted");
  rangeIs(state.viewRange(), -50.0, -25.0, "negative UTC view retained");
  state.fitToData();
  rangeIs(state.viewRange(), -100.0, 100.0, "fit restores data range");

  state.setAmplitudeRange(1, {-2.5, 7.5});
  state.setAmplitudeMode(AmplitudeMode::Fixed);
  state.toggleInverted();
  const auto inverted = state.amplitudeRange(1);
  check(std::abs(inverted.low + 7.5) < 1e-12 && std::abs(inverted.high - 2.5) < 1e-12,
        "fixed amplitude range follows inversion");
  check(state.clipEnabled(), "clipping defaults on");
  check(!state.toggleClip(), "clip toggle off");
  check(state.toggleClip(), "clip toggle on");

  bool rejected = false;
  try { state.setBaseChannels({4}, 4); }
  catch (const std::out_of_range&) { rejected = true; }
  check(rejected, "invalid channel selection rejected");
}
void edgeSafetyTests()
{
  using namespace itsv;
  const double max = std::numeric_limits<double>::max();
  check(validRange(widenedRange(max)), "widening maximum finite double must stay finite");
  check(validRange(widenedRange(-max)), "widening minimum finite double must stay finite");
  check(validRange(widenedRange(0.0)), "widening zero");
  check(!validRange({-max, max}), "reject unrepresentable span");
  check(import_limits::canAdd(1000000, 29000000), "decoded budget boundary");
  check(!import_limits::canAdd(1000000, 29000001), "reject decoded budget overflow");
  check(!import_limits::canAdd(UINT64_MAX, 1), "avoid decoded budget unsigned overflow");

  const auto y = geometry::amplitudeY(1.0e300, {-1e-6, 1e-6}, 10, 100,
                                      false, 900);
  check(y >= -4096 && y <= 4996, "unclipped amplitude stays within safe viewport extent");
  check(geometry::amplitudeY(1.0e300, {-1e-6, 1e-6}, 10, 100, true, 900) == 10,
        "clipped high outlier touches top");
  check(geometry::amplitudeY(-1.0e300, {-1e-6, 1e-6}, 10, 100, true, 900) == 100,
        "clipped low outlier touches bottom");
  check(geometry::amplitudeY(0.0, {-1e-6, 1e-6}, 10, 100, true, 900) == 55,
        "center amplitude maps to center panel");
  check(geometry::timeX(max, {0.0, 1.0}, 150, 1200) == 1200,
        "time coordinate saturates before integer conversion");
  check(geometry::timeX(-max, {0.0, 1.0}, 150, 1200) == 150,
        "time coordinate saturates at left boundary");
}

void amplitudeMenuTests()
{
  using namespace itsv;
  ViewState state;
  check(state.amplitudeMode() == AmplitudeMode::Auto, "initial amplitude mode is Auto");
  for (const auto active : amplitudeMenuModes) {
    state.setAmplitudeMode(active);
    check(state.amplitudeMode() == active, "amplitude mode selection stored");
    int checkedCount = 0;
    for (const auto option : amplitudeMenuModes)
      checkedCount += amplitudeMenuChecked(option, state.amplitudeMode()) ? 1 : 0;
    check(checkedCount == 1, "exactly one amplitude mode has a check mark");
  }
  state.setAmplitudeMode(AmplitudeMode::Fixed);
  state.resetForRecording(2, {-100.0, 100.0});
  check(state.amplitudeMode() == AmplitudeMode::Auto,
        "loading another recording restores Auto menu selection");
}

void uniformRowTests()
{
  using namespace itsv::uniform_rows;
  check(clampHeight(1) == minimumHeight, "shared row height has readable minimum");
  check(clampHeight(10000) == maximumHeight, "shared row height is bounded");
  check(minimumHeight == 56 && maximumHeight == 1600,
        "waveform rows have expanded 56-1600 pixel range");
  check(clampHeight(700) == 700, "700-pixel waveform row allowed");
  check(fitHeight(0, 600, 64) == defaultHeight, "no rows keeps neutral height");
  check(fitHeight(1, 600, 64) == 536, "one waveform fills available area");
  check(fitHeight(3, 600, 64) == 178, "three waveforms share available height");
  check(fitHeight(10, 600, 64) == minimumHeight,
        "many waveforms use a minimum-height scrolling viewport");
  check(fitHeight(12, 720, 64) == minimumHeight,
        "height floor applies when there is insufficient room");
  check(fitHeight(0, 0, 64) == defaultHeight, "no waveforms before browser measurement");
  check(fitHeight(2, 64, 64) == minimumHeight, "tiny viewport never collapses rows");
  check(fitHeight(1, 10000, 64) == maximumHeight, "very tall viewport respects paint limit");
  check(fitHeight(static_cast<std::size_t>(-1), 10000, 64) == minimumHeight,
        "huge channel counts cannot overflow auto-fit division");
  check(fitHeight(4, 500, 68) == 108, "font-dependent time ruler is reserved");
  check(clampHeight(1500) == 1500, "1500-pixel waveform row allowed");
  check(contentHeight(1, 110, 64) == 174, "one trace plus time ruler");
  check(contentHeight(12, 110, 64) == 1384, "native scroll content height grows linearly");
  check(contentHeight(0, 110, 64) == 64, "no waveforms leaves a bounded ruler");
  check(contentHeight(static_cast<std::size_t>(-1), 110, 64) ==
        std::numeric_limits<int>::max(), "large channel count cannot overflow height");
  check(dividerAt(109, 4, 110) == 1, "first row bottom divider");
  check(dividerAt(219, 4, 110) == 2, "second row divider");
  check(dividerAt(439, 4, 110) == 4, "last row divider works");
  check(dividerAt(109, 0, 110) == 0, "no divider without rows");
  check(dividerAt(90, 4, 110) == 0, "normal click does not trigger row resize");
  check(heightFromDrag(110, 1, 30) == 140, "first divider tracks pointer");
  check(heightFromDrag(110, 3, 90) == 140, "third divider also changes every row to 140");
  check(heightFromDrag(110, 3, -9999) == minimumHeight, "row shrink clamps");
  check(heightFromDrag(110, 3, 9999) == maximumHeight, "row grow clamps");
  check(heightFromDrag(110, 1, 450) == 560,
        "mouse drag supports a 560-pixel waveform row");
  check(heightFromDrag(1500, 1, 300) == maximumHeight,
        "large row grows safely up to maximum");
  check(heightFromDrag(110, 1, std::numeric_limits<int>::max()) == maximumHeight,
        "extreme positive drag cannot overflow height conversion");
  check(heightFromDrag(110, 1, std::numeric_limits<int>::min()) == minimumHeight,
        "extreme negative drag cannot overflow height conversion");
  for (int viewport = 120; viewport <= 4000; viewport += 37) {
    for (int count = 1; count <= 80; ++count) {
      const int height = fitHeight(static_cast<std::size_t>(count), viewport, 64);
      check(height >= minimumHeight && height <= maximumHeight,
            "fitted waveform height always stays within readable bounds");
      const int remaining = viewport - 64;
      if (remaining >= count * minimumHeight && remaining <= count * maximumHeight)
        check(count * height <= remaining && remaining - count * height < count,
              "auto-fit uses all but fractional remainder pixels");
    }
  }
  std::mt19937 gen(73820);
  for (int i = 0; i < 3000; ++i) {
    const int count = 1 + static_cast<int>(gen() % 30);
    const int height = minimumHeight + static_cast<int>(gen() %
        (maximumHeight - minimumHeight + 1));
    const int ordinal = 1 + static_cast<int>(gen() % count);
    const int y = ordinal * height - 1;
    check(dividerAt(y, count, height) == ordinal,
          "uniform row boundary hit testing agrees with row geometry");
    const int delta = static_cast<int>(gen() % 121) - 60;
    const int next = heightFromDrag(height, ordinal, delta);
    check(next >= minimumHeight && next <= maximumHeight,
          "every row receives one shared clamped height");
    const int content = contentHeight(count, next, 64);
    check(content == count * next + 64,
          "all trace rows have exactly the same height after resizing");
  }
}

void envelopeTests()
{
  using namespace itsv;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::mt19937 generator(781512);
  std::uniform_real_distribution<double> value(-250.0, 280.0);
  std::uniform_real_distribution<double> clock(-50.0, 30.0);
  for (int trial = 0; trial < 1500; ++trial) {
    const int count = 32 + static_cast<int>(generator() % 4000);
    const int columns = 5 + static_cast<int>(generator() % 190);
    const double start = clock(generator);
    const double interval = 0.02 + 0.001 * static_cast<double>(generator() % 100);
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      const double v = value(generator);
      samples.push_back(i % 97 == 0 ? nan : v);
    }
    Segment segment(start, interval, samples);
    Channel channel{{"AZ", "TEST", "00", "BHZ"}, {segment}};
    const Range bounds{start - 0.33 * interval,
                       start + static_cast<double>(count - 1) * interval + 0.31 * interval};
    const auto actual = channel.overviewEnvelope(bounds, columns);
    check(static_cast<int>(actual.size()) == columns, "correct overview bin count");
    std::vector<EnvelopeBin> expected(static_cast<std::size_t>(columns));
    for (int i = 0; i < count; ++i) {
      if (!std::isfinite(samples[static_cast<std::size_t>(i)])) continue;
      const long double time = static_cast<long double>(start) +
          static_cast<long double>(i) * interval;
      const long double f = (time - bounds.low) /
          (static_cast<long double>(bounds.high) - bounds.low);
      const std::size_t x = f <= 0 ? 0 : f >= 1 ? static_cast<std::size_t>(columns - 1) :
          std::min(static_cast<std::size_t>(columns - 1),
                   static_cast<std::size_t>(f * columns));
      auto& bin = expected[x];
      const double v = samples[static_cast<std::size_t>(i)];
      if (!bin.present) bin = {v, v, true};
      else { bin.low = std::min(bin.low, v); bin.high = std::max(bin.high, v); }
    }
    bool equal = actual.size() == expected.size();
    for (std::size_t i = 0; equal && i < actual.size(); ++i) {
      equal = actual[i].present == expected[i].present &&
          (!actual[i].present || (actual[i].low == expected[i].low &&
                                  actual[i].high == expected[i].high));
    }
    check(equal, "accelerated envelope matches per-sample reference");
  }
  Channel gap{{"AZ", "GAP", "", "BHZ"},
              {Segment(0, 1, {1, 2, 3}), Segment(100, 1, {-2, -3, -4})}};
  const auto bins = gap.overviewEnvelope({0, 102}, 103);
  check(!bins[50].present && bins[0].present && bins[102].present,
        "separated segments keep blank columns");
  check(gap.overviewEnvelope({0, 102}, 0).empty(), "reject zero-width envelope");
  check(gap.overviewEnvelope({0, 102}, 12001).empty(), "reject oversize envelope");
  // A large DC offset must retain both min and max in the envelope.
  Channel dc{{"AZ", "DC", "", "BHZ"},
             {Segment(0.0, 0.001, std::vector<double>(100000, 2.0e9))}};
  const auto dcBins = dc.overviewEnvelope({0.0, 99.999}, 200);
  check(dcBins[20].present && dcBins[20].low == 2.0e9 && dcBins[20].high == 2.0e9,
        "large DC baseline is preserved");
}

}

int main()
{
  try {
    navigationTests();
    previewSelectionTests();
    recordingTests();
    viewStateTests();
    amplitudeMenuTests();
    edgeSafetyTests();
    uniformRowTests();
    envelopeTests();
    std::cout << "PASS: " << checks << " checks\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
