/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/linear_filters/ladder_filter.h"

#include <cfloat>
#include <cmath>
#include <complex>
#include <random>
#include <vector>

#include "audio/dsp/testing_util.h"
#include "audio/linear_filters/biquad_filter.h"
#include "audio/linear_filters/biquad_filter_coefficients.h"
#include "audio/linear_filters/biquad_filter_design.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {
namespace {

using ::absl::StrFormat;
using ::audio_dsp::EigenArrayNear;
using ::Eigen::Array;
using ::Eigen::Array2Xf;
using ::Eigen::ArrayXf;
using ::Eigen::ArrayXXf;
using ::Eigen::Dynamic;
using ::std::complex;
using ::std::vector;

// Get the name of Type as a string.
template <typename Type>
std::string GetTypeName() {
  return util::Demangle(typeid(Type).name());
}

template <typename ScalarType>
struct Tolerance { constexpr static float value = 0; };

template <> struct Tolerance<float> { constexpr static float value = 1e-4; };
template <> struct Tolerance<double> { constexpr static double value = 1e-8; };

#ifndef NDEBUG
// Ensure that maps with inner strides crash.
// ProcessBlock is covered in biquad_filter_test.
TEST(LadderFilterDeathTest, ProcessSampleXCrashesOnInnerStrideOutput) {
  const int kNumFrames = 2;
  const int kNumChannels = 2;
  LadderFilter<ArrayXXf> filter;
  filter.InitFromTransferFunction(kNumChannels, {1, 0, 0}, {1, 0, 0});

  ArrayXXf output(kNumChannels, 2);
  ArrayXXf data(kNumChannels, kNumFrames * 2);
  Eigen::Map<ArrayXXf, 0, Eigen::InnerStride<2>> map(data.data(), kNumChannels,
                                                     1);

  ASSERT_DEATH(filter.ProcessSample(data.col(0), &map), "inner stride");
}

TEST(LadderFilterDeathTest, ProcessSampleXCrashesOnInnerStrideInput) {
  const int kNumFrames = 2;
  const int kNumChannels = 2;
  LadderFilter<ArrayXXf> filter;
  filter.InitFromTransferFunction(kNumChannels, {1, 0, 0}, {1, 0, 0});

  ArrayXf output(kNumChannels);
  ArrayXXf data(kNumChannels, kNumFrames * 2);
  Eigen::Map<ArrayXXf, 0, Eigen::InnerStride<2>> map(data.data(), kNumChannels,
                                                     2);

  ASSERT_DEATH(filter.ProcessSample(map.col(0), &output), "inner stride");
}

TEST(LadderFilterDeathTest, ProcessSampleNCrashesOnInnerStrideOutput) {
  const int kNumFrames = 2;
  const int kNumChannels = 2;
  LadderFilter<Array2Xf> filter;
  filter.InitFromTransferFunction(kNumChannels, {1, 0, 0}, {1, 0, 0});

  Array2Xf output(kNumChannels, 2);
  Array2Xf data(kNumChannels, kNumFrames * 2);
  Eigen::Map<Array2Xf, 0, Eigen::InnerStride<2>> map(data.data(), kNumChannels,
                                                     1);

  ASSERT_DEATH(filter.ProcessSample(data.col(0), &map), "inner stride");
}

TEST(LadderFilterDeathTest, ProcessSampleNCrashesOnInnerStrideInput) {
  const int kNumFrames = 2;
  const int kNumChannels = 2;
  LadderFilter<ArrayXXf> filter;
  filter.InitFromTransferFunction(kNumChannels, {1, 0, 0}, {1, 0, 0});

  ArrayXf output(kNumChannels);
  ArrayXXf data(kNumChannels, kNumFrames * 2);
  Eigen::Map<ArrayXXf, 0, Eigen::InnerStride<2>> map(data.data(), kNumChannels,
                                                     2);

  ASSERT_DEATH(filter.ProcessSample(map.col(0), &output), "inner stride");
}
#endif  // NDEBUG

template <typename T>
class LadderFilterScalarTypedTest : public ::testing::Test {};

typedef ::testing::Types<float, double> ScalarTypes;
TYPED_TEST_SUITE(LadderFilterScalarTypedTest, ScalarTypes);

// Tests to make sure LadderFilter and BiquadFilter produce the same result.
TYPED_TEST(LadderFilterScalarTypedTest, MatchesBiquadFilter) {
  LadderFilter<TypeParam> ladder;
  BiquadFilter<TypeParam> biquad;

  auto coeffs = LowpassBiquadFilterCoefficients(48000, 4000, 0.707);
  ladder.InitFromTransferFunction(1, coeffs.b, coeffs.a);
  biquad.Init(1, coeffs);

  std::mt19937 rng(0 /* seed */);
  std::normal_distribution<TypeParam> dist(0, 1);
  for (int i = 0; i < 512; ++i) {
    TypeParam sample = dist(rng);
    TypeParam ladder_output;
    TypeParam biquad_output;
    ladder.ProcessSample(sample, &ladder_output);
    biquad.ProcessSample(sample, &biquad_output);
    ASSERT_NEAR(ladder_output, biquad_output, Tolerance<TypeParam>::value);
  }
}

TYPED_TEST(LadderFilterScalarTypedTest, MatchesMultipleBiquadFilters) {
  LadderFilter<TypeParam> ladder;
  BiquadFilter<TypeParam> biquad1;
  BiquadFilter<TypeParam> biquad2;

  auto coeffs1 = LowpassBiquadFilterCoefficients(48000, 4000, 0.707);
  auto coeffs2 = HighpassBiquadFilterCoefficients(48000, 200, 0.707);
  BiquadFilterCascadeCoefficients coeffs =
      BiquadFilterCascadeCoefficients({coeffs1, coeffs2});
  vector<double> k;
  vector<double> v;
  coeffs.AsLadderFilterCoefficients(&k, &v);
  ladder.InitFromLadderCoeffs(1, k, v);
  biquad1.Init(1, coeffs1);
  biquad2.Init(1, coeffs2);

  std::mt19937 rng(0 /* seed */);
  std::normal_distribution<TypeParam> dist(0, 1);
  for (int i = 0; i < 512; ++i) {
    TypeParam sample = dist(rng);
    TypeParam ladder_output;
    TypeParam biquad_output;
    ladder.ProcessSample(sample, &ladder_output);
    biquad1.ProcessSample(sample, &biquad_output);
    biquad2.ProcessSample(biquad_output, &biquad_output);
    ASSERT_NEAR(ladder_output, biquad_output, Tolerance<TypeParam>::value);
  }
}

// A sanity check to make sure that InitFromLadderCoeffs doesn't break.
TYPED_TEST(LadderFilterScalarTypedTest, LadderInit) {
  LadderFilter<TypeParam> ladder;

  ladder.InitFromLadderCoeffs(1, {0.3, 0.3, -0.2}, {0.1, 2.3, -5.2, 1.0});

  std::mt19937 rng(0 /* seed */);
  std::normal_distribution<TypeParam> dist(0, 1);
  for (int i = 0; i < 3; ++i) {
    TypeParam sample = dist(rng);
    TypeParam ladder_output;
    ladder.ProcessSample(sample, &ladder_output);
  }
}

TEST(LadderFilterScalarTypedTest, LadderIdentityFilter) {
  LadderFilter<double> ladder;

  std::vector<double> coeffs_b = {1.0, 0.0, 0.0};
  std::vector<double> coeffs_a = {1.0, 0.0, 0.0};
  std::vector<double> coeffs_k;
  std::vector<double> coeffs_v;
  MakeLadderCoefficientsFromTransferFunction(coeffs_b, coeffs_a,
                                             &coeffs_k, &coeffs_v);
  EXPECT_THAT(coeffs_k, testing::ContainerEq(std::vector<double>{0, 0}));
  ladder.InitFromLadderCoeffs(1, coeffs_k, coeffs_v);

  std::mt19937 rng(0 /* seed */);
  std::normal_distribution<double> dist(0, 1);
  for (int i = 0; i < 300; ++i) {
    double sample = dist(rng);
    double ladder_output;
    ladder.ProcessSample(sample, &ladder_output);
    EXPECT_NEAR(sample, ladder_output, 1e-5);
  }
}

template <typename T>
class LadderFilterMultichannelTypedTest : public ::testing::Test {};

typedef ::testing::Types<
    // With scalar SampleType.
    float,
    double,
    complex<float>,
    complex<double>,
    // With SampleType = Eigen::Array* with dynamic number of channels.
    Eigen::ArrayXf,
    Eigen::ArrayXd,
    Eigen::ArrayXcf,
    Eigen::ArrayXcd,
    // With SampleType = Eigen::Vector* with dynamic number of channels.
    Eigen::VectorXf,
    Eigen::VectorXcf,
    // With SampleType with fixed number of channels.
    Eigen::Array3f,
    Eigen::Array3cf,
    Eigen::Vector3f,
    Eigen::Vector3cf
    >
    SampleTypes;
TYPED_TEST_SUITE(LadderFilterMultichannelTypedTest, SampleTypes);

// Test LadderFilter<SampleType> against BiquadFilter<SampleType> for different
// template args.
TYPED_TEST(LadderFilterMultichannelTypedTest, LadderFilter) {
  using SampleType = TypeParam;
  SCOPED_TRACE(
      testing::Message() << "SampleType: " << GetTypeName<SampleType>());

  constexpr int kNumSamples = 20;
  using BiquadFilterType = BiquadFilter<SampleType>;
  using LadderFilterType = LadderFilter<SampleType>;
  const int kNumChannelsAtCompileTime =
      LadderFilterType::kNumChannelsAtCompileTime;
  using ScalarType = typename LadderFilterType::ScalarType;
  using BlockOfSamples =
      typename Eigen::Array<ScalarType, kNumChannelsAtCompileTime, Dynamic>;
  BiquadFilterType biquad;
  LadderFilterType ladder;

  for (int num_channels : {1, 4, 7}) {
    if (kNumChannelsAtCompileTime != Dynamic &&
        kNumChannelsAtCompileTime != num_channels) {
      continue;  // Skip if SampleType is incompatible with num_channels.
    }
    SCOPED_TRACE("num_channels: " + testing::PrintToString(num_channels));

    auto coeffs = LowpassBiquadFilterCoefficients(48000, 4000, 0.707);
    ladder.InitFromTransferFunction(num_channels, coeffs.b, coeffs.a);
    biquad.Init(num_channels, coeffs);

    BlockOfSamples input = BlockOfSamples::Random(num_channels, kNumSamples);
    BlockOfSamples ladder_output;
    ladder.ProcessBlock(input, &ladder_output);
    BlockOfSamples biquad_output;
    biquad.ProcessBlock(input, &biquad_output);

    EXPECT_THAT(ladder_output, EigenArrayNear(biquad_output, 1e-5));
  }
}

template <typename T>
class LadderFilterMultichannelScalarTypedTest : public ::testing::Test {};

typedef ::testing::Types<float, double> ScalarSampleTypes;
TYPED_TEST_SUITE(LadderFilterMultichannelScalarTypedTest, ScalarSampleTypes);

// We will smooth between the coefficients of two eighth order filters
// with very different looking transfer functions. This should be a pretty
// good stress test.
//
// This test should not give different results for different numbers of
// channels or complex scalar types. All the terrifying math is happening in the
// coefficient smoothing.
TYPED_TEST(LadderFilterMultichannelScalarTypedTest,
           LadderFilterCoefficientStressTest) {
  using SampleType = TypeParam;
  SCOPED_TRACE(
      testing::Message() << "SampleType: " << GetTypeName<SampleType>());

  using LadderFilterType = LadderFilter<SampleType>;
  const int kNumChannelsAtCompileTime =
      LadderFilterType::kNumChannelsAtCompileTime;
  using ScalarType = typename LadderFilterType::ScalarType;
  using BlockOfSamples =
      typename Eigen::Array<ScalarType, kNumChannelsAtCompileTime, Dynamic>;
  LadderFilterType ladder;

  vector<double> bandpass_k;
  vector<double> bandpass_v;
  ButterworthFilterDesign(4).BandpassCoefficients(48000, 3000, 4000)
      .AsLadderFilterCoefficients(&bandpass_k, &bandpass_v);
  vector<double> bandstop_k;
  vector<double> bandstop_v;
  // This filter is pretty numerically dicey. We're putting lots of poles near
  // s = 0 to achieve the very low 12Hz cutoff. If we can smooth to
  // and from this without issues, everything else should be pretty safe.
  //
  // Note that for lower cutoffs, higher order, or 32-bit precision, the
  // the coefficients can still leave the range [-1, 1]. For these reason, we
  // enforce a stability check in the smoothing code.
  ButterworthFilterDesign(4).BandstopCoefficients(48000, 12, 2000)
      .AsLadderFilterCoefficients(&bandstop_k, &bandstop_v);

  constexpr int num_channels = 1;

  if (kNumChannelsAtCompileTime != Dynamic &&
      kNumChannelsAtCompileTime != num_channels) {
    return;  // Skip if SampleType is incompatible with num_channels.
  }
  ladder.InitFromLadderCoeffs(num_channels, bandstop_k, bandstop_v);

  bool filter_switch = true;
  // These numbers are mostly random, but an attempt was made to give varied
  // block sizes to each of the two filter coefficient sets.
  int sample_count = 0;
  for (int num_samples : {10, 300, 4, 1203, 8000, 13, 13000,
                          20, 433, 1234, 10000, 100}) {
    SCOPED_TRACE(
        StrFormat("Filter has %d channels and has processed %d "
                  "samples.",
                  num_channels, sample_count));
    BlockOfSamples input = BlockOfSamples::Random(num_channels, num_samples);
    BlockOfSamples ladder_output;

    filter_switch = !filter_switch;
    if (filter_switch) {
      ladder.ChangeLadderCoeffs(bandpass_k, bandpass_v);
    } else {
      ladder.ChangeLadderCoeffs(bandstop_k, bandstop_v);
    }
    ladder.ProcessBlock(input, &ladder_output);
    // The samples don't turn to nan at any point. This can happen if the
    // smoothing filter overshoots causing the reflection coefficients to
    // get smaller than -1, which in turn cause the scattering coefficients
    // to go nan during the sqrt computation.
    ASSERT_FALSE(isnan(std::abs(ladder_output.sum())));
    sample_count += num_samples;
  }
}

template <bool kChangeCoefficients>
void BM_LadderFilterScalarFloat(benchmark::State& state) {
  constexpr int kSamplePerBlock = 1000;
  srand(0 /* seed */);
  ArrayXf input = ArrayXf::Random(kSamplePerBlock);
  ArrayXf output(kSamplePerBlock);
  LadderFilter<float> filter;
  // Equivalent to a single biquad stage.
  vector<double> k = {-0.2, 0.9};
  vector<double> v = {0.5, -0.2, 0.1};
  filter.InitFromLadderCoeffs(1, k, v);

  while (state.KeepRunning()) {
    if (kChangeCoefficients) {
      // There is no check internally to prevent smoothing when the
      // coefficients don't *actually* change.
      filter.ChangeLadderCoeffs(k, v);
    }
    filter.ProcessBlock(input, &output);
    benchmark::DoNotOptimize(output);
  }
  state.SetItemsProcessed(kSamplePerBlock * state.iterations());
}
// Old-style template benchmarking needed for open sourcing. External
// google/benchmark repo doesn't have functionality from cl/118676616 enabling
// BENCHMARK(TemplatedFunction<2>) syntax.

// No coefficient smoothing.
BENCHMARK_TEMPLATE(BM_LadderFilterScalarFloat, false);
// Also test with coefficient smoothing.
BENCHMARK_TEMPLATE(BM_LadderFilterScalarFloat, true);

template <bool kChangeCoefficients>
void BM_LadderFilterArrayXf(benchmark::State& state) {
  const int num_channels = state.range(0);
  constexpr int kSamplePerBlock = 1000;
  srand(0 /* seed */);
  ArrayXXf input = ArrayXXf::Random(num_channels, kSamplePerBlock);
  ArrayXXf output(num_channels, kSamplePerBlock);
  LadderFilter<ArrayXf> filter;
  vector<double> k = {-0.2, 0.9};
  vector<double> v = {0.5, -0.2, 0.1};
  filter.InitFromLadderCoeffs(num_channels, k, v);

  while (state.KeepRunning()) {
    if (kChangeCoefficients) {
      // There is no check internally to prevent smoothing when the
      // coefficients don't *actually* change.
      filter.ChangeLadderCoeffs(k, v);
    }
    filter.ProcessBlock(input, &output);
    benchmark::DoNotOptimize(output);
  }
  state.SetItemsProcessed(kSamplePerBlock * state.iterations());
}

// No coefficient smoothing.
BENCHMARK_TEMPLATE(BM_LadderFilterArrayXf, false)->DenseRange(1, 10);
// Also test with smoothing.
BENCHMARK_TEMPLATE(BM_LadderFilterArrayXf, true)->DenseRange(1, 10);

template <int kNumChannels, bool kChangeCoefficients>
void BM_LadderFilterArrayNf(benchmark::State& state) {
  constexpr int kSamplePerBlock = 1000;
  srand(0 /* seed */);
  using ArrayNf = Eigen::Array<float, kNumChannels, 1>;
  using ArrayNXf = Eigen::Array<float, kNumChannels, Dynamic>;
  ArrayNXf input = ArrayNXf::Random(kNumChannels, kSamplePerBlock);
  ArrayNXf output(kNumChannels, kSamplePerBlock);
  LadderFilter<ArrayNf> filter;
  vector<double> k = {-0.2, 0.9};
  vector<double> v = {0.5, -0.2, 0.1};
  filter.InitFromLadderCoeffs(kNumChannels, k, v);

  while (state.KeepRunning()) {
    if (kChangeCoefficients) {
      // There is no check internally to prevent smoothing when the
      // coefficients don't *actually* change.
      filter.ChangeLadderCoeffs(k, v);
    }
    filter.ProcessBlock(input, &output);
    benchmark::DoNotOptimize(output);
  }
  state.SetItemsProcessed(kSamplePerBlock * state.iterations());
}

// No coefficient smoothing.
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 1, false);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 2, false);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 3, false);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 4, false);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 5, false);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 6, false);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 7, false);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 8, false);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 9, false);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 10, false);
// Also test with coefficient smoothing.
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 1, true);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 2, true);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 3, true);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 4, true);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 5, true);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 6, true);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 7, true);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 8, true);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 9, true);
BENCHMARK_TEMPLATE2(BM_LadderFilterArrayNf, 10, true);
}  // namespace
}  // namespace linear_filters

/*
Run on lpac2 (32 X 2600 MHz CPUs); 2017-03-18T00:23:02.130960951-07:00
CPU: Intel Sandybridge with HyperThreading (16 cores)
Benchmark                           Time(ns)        CPU(ns)     Iterations
--------------------------------------------------------------------------
// Without Smoothing
BM_LadderFilterScalarFloat<false>      10798          10785         648910
// With Smoothing
BM_LadderFilterScalarFloat<true>       23546          23514         296014
// Without Smoothing
BM_LadderFilterArrayXf<false>/1        45927          45864         148129
BM_LadderFilterArrayXf<false>/2        58594          58509         100000
BM_LadderFilterArrayXf<false>/3        66637          66548         100000
BM_LadderFilterArrayXf<false>/4        68580          68484         100000
BM_LadderFilterArrayXf<false>/5        89917          89790          78060
BM_LadderFilterArrayXf<false>/6        96007          95865          73105
BM_LadderFilterArrayXf<false>/7       107780         107645          65079
BM_LadderFilterArrayXf<false>/8       100637         100487          69824
BM_LadderFilterArrayXf<false>/9       121341         121155          57750
BM_LadderFilterArrayXf<false>/10      125594         125412          55950
// With Smoothing
BM_LadderFilterArrayXf<true>/1         59495          59414         100000
BM_LadderFilterArrayXf<true>/2         71644          71538          97357
BM_LadderFilterArrayXf<true>/3         80983          80855          86763
BM_LadderFilterArrayXf<true>/4         82802          82683          85074
BM_LadderFilterArrayXf<true>/5        105808         105660          66139
BM_LadderFilterArrayXf<true>/6        111400         111244          62859
BM_LadderFilterArrayXf<true>/7        123522         123346          56632
BM_LadderFilterArrayXf<true>/8        116607         116421          59819
BM_LadderFilterArrayXf<true>/9        137115         136906          50875
BM_LadderFilterArrayXf<true>/10       140538         140302          49863
// Without Smoothing
BM_LadderFilterArrayNf<1, false>       11434          11418         614886
BM_LadderFilterArrayNf<2, false>       17536          17512         398564
BM_LadderFilterArrayNf<3, false>       23380          23348         301532
BM_LadderFilterArrayNf<4, false>       23843          23812         293993
BM_LadderFilterArrayNf<5, false>       31686          31638         221157
BM_LadderFilterArrayNf<6, false>       36910          36849         189688
BM_LadderFilterArrayNf<7, false>       44204          44134         158793
BM_LadderFilterArrayNf<8, false>       47177          47100         148575
BM_LadderFilterArrayNf<9, false>       66160          66055         100000
BM_LadderFilterArrayNf<10, false>      66245          66142         100000
// With Smoothing
BM_LadderFilterArrayNf<1, true>        24169          24138         291159
BM_LadderFilterArrayNf<2, true>        30826          30773         226611
BM_LadderFilterArrayNf<3, true>        36160          36102         194426
BM_LadderFilterArrayNf<4, true>        36711          36651         189847
BM_LadderFilterArrayNf<5, true>        44581          44512         158154
BM_LadderFilterArrayNf<6, true>        49466          49386         141126
BM_LadderFilterArrayNf<7, true>        56954          56887         100000
BM_LadderFilterArrayNf<8, true>        61420          61335         100000
BM_LadderFilterArrayNf<9, true>        79584          79457          88092
BM_LadderFilterArrayNf<10, true>       80726          80587          86785
*/
