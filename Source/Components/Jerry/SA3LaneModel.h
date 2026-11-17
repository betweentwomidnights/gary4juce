// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Ported from sa3-iplug2-demo's Shared/SA3LaneModel.h, so both plugins draw and send automation
// lanes the same way. The blend differs: here it's a slider over how long the take shares one
// arrangement, where the demo toggles between two presets.

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

// The blend: how many of the first steps play every lane at its curve's average, so the take
// settles on one arrangement before the curves apply. Stop 0 (smooth) shares half the steps; each
// stop to the right shares fewer, so the curves take hold sooner and the LoRAs reach further into
// the arrangement, down to none shared at the last (responsive). Every stop keeps the sections
// gently apart (isolation 1) through the last step. The stops were picked by ear at 8 steps: 4, 3,
// 2 and 0 shared; 1 sounded like 2. A fraction of the steps lands at the same noise level at any
// count, since the schedule is a function of step / steps.
inline constexpr int kSA3LaneBlendStops = 4;
inline constexpr float kSA3LaneIsolation = 1.f;

inline int LaneBlendShared(int stop, int steps)
{
  constexpr float shared[kSA3LaneBlendStops] = {0.5f, 0.375f, 0.25f, 0.f};
  steps = std::max(1, steps);
  return static_cast<int>(std::lround(static_cast<float>(steps) * shared[std::clamp(stop, 0, kSA3LaneBlendStops - 1)]));
}

// sa3.cpp's sampler schedule (make_sa3_schedule with the shift's default parameters): the sigma
// each step runs at. A linear ramp from sigmaMax, warped by the distribution shift. sigmaMax is 1
// except for a transform, where it is the transform's strength. Only Full depends on the length.
inline std::vector<float> SA3StepSigmas(int steps, float sigmaMax, const std::string& shift, double seconds)
{
  steps = std::max(1, steps);
  const float frames = static_cast<float>(std::max(1.0, std::ceil(seconds * 44100.0 / 4096.0)));
  const auto warp = [&](float t)
  {
    if (shift == "logsnr")
      return 1.f / (1.f + std::exp(2.f - t * 8.2f));
    if (shift == "flux")
      return 6.93f * t / (1.f + 5.93f * t);
    if (shift == "full")
    {
      const float length = std::clamp(frames, 256.f, 4096.f);
      const float em = std::exp(-(0.5f + 0.65f * (length - 256.f) / 3840.f));
      return 1.f - em / (em + t / (1.f - t));
    }
    return t;
  };
  std::vector<float> sigmas(static_cast<size_t>(steps));
  sigmas[0] = sigmaMax;
  for (int i = 1; i < steps; ++i)
  {
    const float linear = sigmaMax * (1.f - static_cast<float>(i) / static_cast<float>(steps));
    const float warped = warp(linear);
    sigmas[static_cast<size_t>(i)] = std::isfinite(warped) ? warped : linear;
  }
  return sigmas;
}

// A sigma that steps before `step` run above and the rest don't, since sa3.cpp compares a step's
// sigma against each threshold. A transform's schedule can rise after its first step; where no
// sigma separates them, only the earlier steps above every later one count.
inline float SA3SigmaBefore(const std::vector<float>& sigmas, int step)
{
  const auto split = sigmas.begin() + std::clamp(step, 1, static_cast<int>(sigmas.size()) - 1);
  const float lowest = *std::min_element(sigmas.begin(), split);
  const float highest = *std::max_element(split, sigmas.end());
  return lowest > highest ? 0.5f * (lowest + highest) : highest;
}

struct SA3LaneBlendSettings
{
  float isolation = 0.f;
  float isolationUntil = 0.f;   // 0 runs isolation through the last step
  float averageUntil = 0.f;     // 0 averages no steps
};

inline SA3LaneBlendSettings LaneBlendSettings(int stop, const std::vector<float>& sigmas)
{
  const int steps = static_cast<int>(sigmas.size());
  const int shared = LaneBlendShared(stop, steps);
  SA3LaneBlendSettings settings;
  settings.isolation = kSA3LaneIsolation;
  if (shared >= steps)
    settings.averageUntil = 0.5f * *std::min_element(sigmas.begin(), sigmas.end());
  else if (shared > 0)
    settings.averageUntil = SA3SigmaBefore(sigmas, shared);
  return settings;
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

// Each point is sent at least this long after the one before it. sa3.cpp sorts the pairs by time
// and then amount, so two points at one time, a vertical jump, come out lowest first: a drop
// becomes a rise toward 0 followed by a ramp down from the peak to the next point, which plays the
// LoRA for bars after it was drawn to stop. Far shorter than a latent frame (93 ms), so it doesn't
// move the jump.
inline constexpr float kSA3EnvelopeTieSeconds = 0.001f;

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
    float seconds = static_cast<float>(offsetSeconds + point.position * durationSeconds);
    if (!envelope.empty())
      seconds = std::max(seconds, envelope[envelope.size() - 2] + kSA3EnvelopeTieSeconds);
    envelope.push_back(seconds);
    envelope.push_back(std::clamp(point.amount, 0.f, kSA3MaxLaneAmount));
  }
  return envelope;
}

}
