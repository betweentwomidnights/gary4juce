// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Ported from sa3-iplug2-demo's Shared/SA3LaneModel.h, so both plugins draw and send automation
// lanes the same way. The blend differs: here it's a slider between the demo's two presets.

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

// The blend runs from one song (0) to distinct (1). One song spends the first half of the steps
// with every lane at its curve's average, so the take settles on one arrangement before the curves
// apply, and then keeps the sections gently apart. Distinct shares no steps and pulls the sections
// hard apart over the first half, letting the joins smooth after. In between, fewer steps are
// shared, so the curves take hold sooner, and the half that pulls the sections apart starts
// earlier and pulls harder.
struct SA3LaneBlendSteps
{
  int steps = 0;
  int shared = 0;    // steps [0, shared) play every lane at its average
  int apartTo = 0;   // steps [shared, apartTo) pull the sections apart
  float isolation = 0.f;
};

inline int LaneBlendStops(int steps)
{
  return (std::max(1, steps) + 1) / 2;
}

inline SA3LaneBlendSteps LaneBlendSteps(float blend, int steps)
{
  steps = std::max(1, steps);
  const int half = LaneBlendStops(steps);
  const float b = std::clamp(blend, 0.f, 1.f);
  const int shared = static_cast<int>(std::lround(static_cast<float>(half) * (1.f - b)));
  return {steps, shared, std::min(steps, shared + half), 1.f + 3.f * b};
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

inline SA3LaneBlendSettings LaneBlendSettings(float blend, const std::vector<float>& sigmas)
{
  const int steps = static_cast<int>(sigmas.size());
  const SA3LaneBlendSteps plan = LaneBlendSteps(blend, steps);
  SA3LaneBlendSettings settings;
  settings.isolation = plan.isolation;
  if (plan.shared >= steps)
    settings.averageUntil = 0.5f * *std::min_element(sigmas.begin(), sigmas.end());
  else if (plan.shared > 0)
    settings.averageUntil = SA3SigmaBefore(sigmas, plan.shared);
  settings.isolationUntil = plan.apartTo >= steps ? 0.f : SA3SigmaBefore(sigmas, plan.apartTo);
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
