// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Andrei Akimov
#include "MiniSeedReader.h"
#include "ImportBudget.h"

extern "C" {
#include "libmseed.h"
}

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace itsv {
namespace {

/**
 * @brief RAII cleanup function for libmseed-allocated trace lists.
 */
struct TraceListDeleter {
  /**
   * @brief Free the native trace list through libmseed.
   * @param list Pointer to a libmseed trace list, possibly null.
   */
  void operator()(MSTraceList* list) const noexcept
  {
    if (!list) return;
    MSTraceList* value = list;
    mstl_free(&value, 0);
  }
};

using TraceListPtr = std::unique_ptr<MSTraceList, TraceListDeleter>;

/**
 * @brief Format a libmseed trace identifier in SEED component order.
 * @param id Native trace identifier containing network, station, location, and channel.
 * @return Dot-separated channel identifier.
 */
std::string traceName(const MSTraceID& id)
{
  return std::string(id.network) + "." + id.station + "." + id.location + "." + id.channel;
}

/**
 * @brief Convert a decoded numeric MiniSEED segment to double-precision samples.
 * @param seg Native decoded segment with integer, float, or double sample storage.
 * @return Owned sample vector, or empty for an unsupported sample type.
 */
std::vector<double> copySamples(const MSTraceSeg& seg)
{
  if (seg.numsamples <= 0 || !seg.datasamples) return {};
  const auto count64 = static_cast<std::uint64_t>(seg.numsamples);
  if (count64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    throw std::runtime_error("MiniSEED segment is too large for this platform");
  const std::size_t count = static_cast<std::size_t>(count64);

  std::vector<double> samples(count);
  switch (seg.sampletype) {
    case 'i': {
      const auto* src = static_cast<const int32_t*>(seg.datasamples);
      for (std::size_t i = 0; i < count; ++i) samples[i] = static_cast<double>(src[i]);
      break;
    }
    case 'f': {
      const auto* src = static_cast<const float*>(seg.datasamples);
      for (std::size_t i = 0; i < count; ++i) samples[i] = static_cast<double>(src[i]);
      break;
    }
    case 'd': {
      const auto* src = static_cast<const double*>(seg.datasamples);
      for (std::size_t i = 0; i < count; ++i) samples[i] = src[i];
      break;
    }
    default:
      return {};
  }
  return samples;
}

} // namespace

MiniSeedLoadResult MiniSeedReader::read(const std::string& fileName)
{
  const auto bytes = std::filesystem::file_size(fileName);
  if (bytes > import_limits::maxInputBytes)
    throw std::runtime_error("MiniSEED file exceeds the 256 MiB input limit");

  // The first pass only reads record metadata. It rejects a compressed
  // recording whose decoded sample count would exceed our memory budget
  // *before* libmseed allocates the unpacked sample arrays.
  MSTraceList* metadataRaw = nullptr;
  const int metadataResult = ms_readtracelist(&metadataRaw, fileName.c_str(), 0,
                                              -1.0, -1.0, 0, 1, 0, 0);
  TraceListPtr metadata(metadataRaw);
  if (metadataResult != MS_NOERROR || !metadata)
    throw std::runtime_error(std::string("MiniSEED metadata read failed: ") +
                             ms_errorstr(metadataResult));
  std::uint64_t sampleTotal = 0;
  for (const MSTraceID* id = metadata->traces; id; id = id->next) {
    for (const MSTraceSeg* seg = id->first; seg; seg = seg->next) {
      if (seg->samplecnt <= 0) continue;
      if (!import_limits::canAdd(sampleTotal, static_cast<std::uint64_t>(seg->samplecnt)))
        throw std::runtime_error("MiniSEED contains more than 30 million samples; "
                                 "reduce the time span or channel count before loading");
      sampleTotal += static_cast<std::uint64_t>(seg->samplecnt);
    }
  }
  metadata.reset(); // release metadata before decoded arrays are allocated

  MSTraceList* raw = nullptr;
  const int result = ms_readtracelist(&raw, fileName.c_str(), 0,
                                      -1.0, -1.0, 0, 1, 1, 0);
  TraceListPtr traces(raw);
  if (result != MS_NOERROR || !traces)
    throw std::runtime_error(std::string("MiniSEED read failed: ") + ms_errorstr(result));

  std::vector<Channel> channels;
  std::vector<std::string> warnings;
  sampleTotal = 0; // defend against unexpected header/decode count differences

  for (const MSTraceID* id = traces->traces; id; id = id->next) {
    Channel channel;
    channel.id.network = id->network;
    channel.id.station = id->station;
    channel.id.location = id->location;
    channel.id.channel = id->channel;

    for (const MSTraceSeg* seg = id->first; seg; seg = seg->next) {
      if (!seg->datasamples || seg->numsamples <= 0) continue;
      if (!std::isfinite(seg->samprate) || !(seg->samprate > 0.0)) {
        warnings.push_back("Skipped segment with invalid sample rate in " + traceName(*id));
        continue;
      }
      if (seg->sampletype != 'i' && seg->sampletype != 'f' && seg->sampletype != 'd') {
        std::ostringstream message;
        message << "Skipped unsupported MiniSEED sample type '" << seg->sampletype
                << "' in " << traceName(*id);
        warnings.push_back(message.str());
        continue;
      }

      const double start = static_cast<double>(seg->starttime) / static_cast<double>(HPTMODULUS);
      if (!std::isfinite(start)) {
        warnings.push_back("Skipped segment with invalid start time in " + traceName(*id));
        continue;
      }

      if (!import_limits::canAdd(sampleTotal, static_cast<std::uint64_t>(seg->numsamples)))
        throw std::runtime_error("Decoded MiniSEED exceeds the 30-million-sample limit");
      sampleTotal += static_cast<std::uint64_t>(seg->numsamples);
      auto samples = copySamples(*seg);
      if (samples.empty()) continue;
      channel.segments.emplace_back(start, 1.0 / seg->samprate, std::move(samples));
    }

    if (!channel.segments.empty()) channels.push_back(std::move(channel));
    else warnings.push_back("No numeric waveform samples found for " + traceName(*id));
  }

  if (channels.empty())
    throw std::runtime_error("The MiniSEED file contains no supported numeric waveform data");

  return {Recording(std::move(channels)), std::move(warnings)};
}

} // namespace itsv
