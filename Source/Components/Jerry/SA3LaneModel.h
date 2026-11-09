// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Ported unchanged from sa3-iplug2-demo's Shared/SA3LaneModel.h, so both plugins draw and send
// automation lanes the same way.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace gary
{

inline constexpr size_t kSA3MaxLanes = 8;
inline constexpr size_t kSA3MaxLanePoints = 256;
inline constexpr float kSA3MaxLaneAmount = 2.f;

// A point on a lane: position runs 0..1 across whatever is being rendered, so a curve follows the
// range when it changes. Amount is the LoRA's strength at that moment, the same 0..2 as its slider.
struct SA3LanePoint
{
  float position = 0.f;
  float amount = 0.f;
};

struct SA3Lane
{
  std::string name;
  std::string path;
  std::string prompt;
  std::vector<SA3LanePoint> points;
};

enum class SA3LaneBlend
{
  OneSong,
  Distinct
};

// What dragged points land on. The values are saved with projects and sessions.
enum class SA3LaneSnap
{
  Off = 0,
  Quarter = 1,
  Half = 2,
  Bar = 3
};

inline SA3LaneSnap LaneSnapFromIndex(int index)
{
  return index >= 1 && index <= 3 ? static_cast<SA3LaneSnap>(index) : SA3LaneSnap::Off;
}

inline double LaneSnapSeconds(SA3LaneSnap snap, double bpm, double beatsPerBar)
{
  if (bpm <= 0.0)
    return 0.0;
  const double beat = 60.0 / bpm;
  switch (snap)
  {
    case SA3LaneSnap::Quarter: return beat;
    case SA3LaneSnap::Half: return beat * 2.0;
    case SA3LaneSnap::Bar: return beat * std::max(1.0, beatsPerBar);
    case SA3LaneSnap::Off: break;
  }
  return 0.0;
}

struct SA3LaneBlendSettings
{
  float isolation = 1.f;
  float isolationUntil = 0.f;
  float averageUntil = 0.9f;
};

inline SA3LaneBlendSettings LaneBlendSettings(SA3LaneBlend blend)
{
  if (blend == SA3LaneBlend::Distinct)
    return {4.f, 0.9f, 0.f};
  return {1.f, 0.f, 0.9f};
}

inline void SortLanePoints(std::vector<SA3LanePoint>& points)
{
  std::stable_sort(points.begin(), points.end(),
                   [](const SA3LanePoint& a, const SA3LanePoint& b) { return a.position < b.position; });
}

// Linear between points and held flat past either end, as the sampler reads them. Points are sorted.
inline float LaneAmountAt(const std::vector<SA3LanePoint>& points, float position)
{
  if (points.empty())
    return 0.f;
  if (position <= points.front().position)
    return points.front().amount;
  for (size_t i = 1; i < points.size(); ++i)
  {
    if (position <= points[i].position)
    {
      const float span = points[i].position - points[i - 1].position;
      const float u = span > 0.f ? (position - points[i - 1].position) / span : 1.f;
      return points[i - 1].amount + u * (points[i].amount - points[i - 1].amount);
    }
  }
  return points.back().amount;
}

// Thins a curve to at most maxPoints, keeping its shape: freehand strokes can exceed what one lane
// may carry. Douglas-Peucker on the amount error, loosening the tolerance until it fits.
inline std::vector<SA3LanePoint> SimplifyLanePoints(const std::vector<SA3LanePoint>& points,
                                                    size_t maxPoints = kSA3MaxLanePoints)
{
  if (points.size() <= maxPoints || maxPoints < 2)
    return points;
  std::vector<char> keep;
  for (float tolerance = 0.002f;; tolerance *= 1.5f)
  {
    keep.assign(points.size(), 0);
    keep.front() = keep.back() = 1;
    std::vector<std::pair<size_t, size_t>> spans{{0, points.size() - 1}};
    while (!spans.empty())
    {
      const auto [first, last] = spans.back();
      spans.pop_back();
      float worst = 0.f;
      size_t worstIndex = first;
      const float span = points[last].position - points[first].position;
      for (size_t i = first + 1; i < last; ++i)
      {
        const float u = span > 0.f ? (points[i].position - points[first].position) / span : 0.f;
        const float line = points[first].amount + u * (points[last].amount - points[first].amount);
        const float error = std::abs(points[i].amount - line);
        if (error > worst)
        {
          worst = error;
          worstIndex = i;
        }
      }
      if (worst > tolerance)
      {
        keep[worstIndex] = 1;
        spans.push_back({first, worstIndex});
        spans.push_back({worstIndex, last});
      }
    }
    if (static_cast<size_t>(std::count(keep.begin(), keep.end(), char(1))) <= maxPoints)
      break;
  }
  std::vector<SA3LanePoint> simplified;
  for (size_t i = 0; i < points.size(); ++i)
    if (keep[i])
      simplified.push_back(points[i]);
  return simplified;
}

// The (seconds, amount) pairs libsa3 reads. offsetSeconds places position 0 on the output timeline,
// which Continue needs because its time 0 is the start of the source.
inline std::vector<float> LaneEnvelopeSeconds(const std::vector<SA3LanePoint>& points,
                                              double offsetSeconds, double durationSeconds)
{
  const std::vector<SA3LanePoint> simplified = SimplifyLanePoints(points);
  std::vector<float> envelope;
  envelope.reserve(simplified.size() * 2);
  for (const SA3LanePoint& point : simplified)
  {
    envelope.push_back(static_cast<float>(offsetSeconds + point.position * durationSeconds));
    envelope.push_back(std::clamp(point.amount, 0.f, kSA3MaxLaneAmount));
  }
  return envelope;
}

}
