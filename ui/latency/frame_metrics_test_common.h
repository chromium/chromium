// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_FRAME_METRICS_TEST_COMMON_H_
#define UI_LATENCY_FRAME_METRICS_TEST_COMMON_H_

#include "ui/latency/fixed_point.h"
#include "ui/latency/histograms.h"
#include "ui/latency/stream_analyzer.h"
#include "ui/latency/windowed_analyzer.h"

#include <array>

// Some convenience macros for checking expected error.
#define EXPECT_ABS_LT(a, b) EXPECT_LT(std::abs(a), std::abs(b))
#define EXPECT_ABS_LE(a, b) EXPECT_LE(std::abs(a), std::abs(b))
#define EXPECT_NEAR_SQRT_APPROX(expected, actual) \
  EXPECT_NEAR(expected, actual, MaxErrorSQRTApprox(expected, actual))

namespace ui {
namespace frame_metrics {

// A simple client to verify it is actually used.
class TestStreamAnalyzerClient : public StreamAnalyzerClient {
 public:
  double TransformResult(double result) const override;
  static constexpr double result_scale = 2.0;
};

using TestWindowedAnalyzerClient = TestStreamAnalyzerClient;

// The WindowedAnalyzer expects the caller to give it some precomputed values,
// even though they are redundant. Precompute them with a helper function to
// remove boilerplate.
// A specialized version of this for StreamAnalyzer that doesn't pre compute
// the weighted values is defined in the implementation file.
template <typename AnalyzerType>
void AddSamplesHelper(AnalyzerType* analyzer,
                      uint64_t value,
                      uint64_t weight,
                      size_t iterations) {
  DCHECK_LE(value, std::numeric_limits<uint32_t>::max());
  DCHECK_LE(weight, std::numeric_limits<uint32_t>::max());
  uint64_t weighted_value = weight * value;
  uint64_t weighted_root = weight * std::sqrt(value << kFixedPointRootShift);
  Accumulator96b weighted_square(value, weight);
  for (size_t i = 0; i < iterations; i++) {
    analyzer->AddSample(value, weight, weighted_value, weighted_root,
                        weighted_square);
  }
}

// A specialization of the templatized AddSamplesHelper above for
// the WindowedAnalyzer, which doesn't need to have it's weighted values
// pre computed.
template <>
void AddSamplesHelper(StreamAnalyzer* analyzer,
                      uint64_t value,
                      uint64_t weight,
                      size_t iterations);

// Moves the |shared_client|'s window forward in time by 1 microsecond and
// adds all of the elements in |values| multipled by kFixedPointMultiplier.
template <typename AnalyzerType>
void AddPatternHelper(SharedWindowedAnalyzerClient* shared_client,
                      AnalyzerType* analyzer,
                      const std::vector<uint32_t>& values,
                      const uint32_t weight) {
  for (auto i : values) {
    shared_client->window_begin += base::TimeDelta::FromMicroseconds(1);
    shared_client->window_end += base::TimeDelta::FromMicroseconds(1);
    AddSamplesHelper(analyzer, i * kFixedPointMultiplier, weight, 1);
  }
}

// Mean and RMS can be exact for most values, however SMR loses a bit of
// precision internally when accumulating the roots. Make sure the SMR
// precision is at least within .5 (i.e. rounded to the nearest integer
// properly), or 8 decimal places if that is less precise.
// When used with kFixedPointMultiplier, this gives us a total precision of
// between ~5 and ~13 decimal places.
// The precision should be even better when the sample's |weight| > 1 since
// the implementation should only do any rounding after scaling by weight.
inline double MaxErrorSQRTApprox(double expected_value, double value) {
  return std::max(0.5, std::max(expected_value, value) * 1e-3);
}

// This class initializes the ratio boundaries on construction in a way that
// is easier to follow than the procedural code in the RatioHistogram
// implementation.
class TestRatioBoundaries {
 public:
  TestRatioBoundaries();
  uint64_t operator[](size_t i) const { return boundaries[i]; }
  size_t size() const { return boundaries.size(); }

 public:
  // uint64_t since the last boundary needs 33 bits.
  std::array<uint64_t, 112> boundaries;
};

// An explicit list of VSync boundaries to verify the procedurally generated
// ones in the implementation.
static constexpr std::array<uint32_t, 99> kTestVSyncBoundries = {
    {// C0: [0,1) (1 bucket).
     0,
     // C1: Powers of two from 1 to 2048 us @ 50% precision (12 buckets)
     1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048,
     // C2: Every 8 Hz from 256 Hz to 128 Hz @ 3-6% precision (16 buckets)
     3906, 4032, 4167, 4310, 4464, 4630, 4808, 5000, 5208, 5435, 5682, 5952,
     6250, 6579, 6944, 7353,
     // C3: Every 4 Hz from 128 Hz to 64 Hz @ 3-6% precision (16 buckets)
     7813, 8065, 8333, 8621, 8929, 9259, 9615, 10000, 10417, 10870, 11364,
     11905, 12500, 13158, 13889, 14706,
     // C4: Every 2 Hz from 64 Hz to 32 Hz @ 3-6% precision (16 buckets)
     15625, 16129, 16667, 17241, 17857, 18519, 19231, 20000, 20833, 21739,
     22727, 23810, 25000, 26316, 27778, 29412,
     // C5: Every 1 Hz from 32 Hz to 1 Hz @ 3-33% precision (31 buckets)
     31250, 32258, 33333, 34483, 35714, 37037, 38462, 40000, 41667, 43478,
     45455, 47619, 50000, 52632, 55556, 58824, 62500, 66667, 71429, 76923,
     83333, 90909, 100000, 111111, 125000, 142857, 166667, 200000, 250000,
     333333, 500000,
     // C6: Powers of two from 1s to 32s @ 50% precision (6 buckets)
     1000000, 2000000, 4000000, 8000000, 16000000, 32000000,
     // C7: Extra value to simplify estimate in Percentiles().
     64000000}};

// A histogram that can be used for dependency injection in tests.
class TestHistogram : public Histogram {
 public:
  struct ValueWeightPair {
    uint32_t value;
    uint32_t weight;
  };

  TestHistogram();
  ~TestHistogram() override;

  // Histogram interface.
  void AddSample(uint32_t value, uint32_t weight) override;
  PercentileResults ComputePercentiles() const override;
  void Reset() override {}

  // Test interface.
  std::vector<ValueWeightPair> GetAndResetAllAddedSamples();
  void SetResults(PercentileResults results);

 private:
  PercentileResults results_;
  std::vector<ValueWeightPair> added_samples_;
};

}  // namespace frame_metrics
}  // namespace ui

#endif  // UI_LATENCY_FRAME_METRICS_TEST_COMMON_H_
