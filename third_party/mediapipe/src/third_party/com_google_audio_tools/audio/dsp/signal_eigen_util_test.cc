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

#include "audio/dsp/signal_eigen_util.h"

#include <cmath>
#include <complex>

#include "audio/dsp/testing_util.h"
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "benchmark/benchmark.h"
#include "third_party/eigen3/Eigen/Core"

namespace audio_dsp {
namespace {

using ::Eigen::Array2f;
using ::Eigen::ArrayXf;
using ::Eigen::ArrayXd;
using ::Eigen::ArrayXcf;
using ::testing::IsEmpty;

TEST(SignalEigenUtilTest, TwoSampleSmooth) {
  Array2f data = {1, 0};
  ThreeTapSmoother<Array2f> smoother(data.size());
  smoother.SmoothCircular(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear<float>({0.8, 0.2}, 1e-5));
}

// Tests for circular smoothing cases.
TEST(SignalEigenUtilTest, CircularSmoothImpulseResponse) {
  ArrayXf data(4);
  data << 1, 0, 0, 0;
  ThreeTapSmoother<ArrayXf> smoother(data.size());
  smoother.SmoothCircular(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear<float>({0.8, 0.1, 0, 0.1}, 1e-5));
}

TEST(SignalEigenUtilTest, CircularSmoothTwoImpulses) {
  ArrayXf data(5);
  data << 1, 0, 0, 0, 1;
  ThreeTapSmoother<ArrayXf> smoother(data.size());
  smoother.SmoothCircular(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear<float>({0.9, 0.1, 0, 0.1, 0.9}, 1e-5));
}

TEST(SignalEigenUtilTest, CircularSmoothThreeImpulses) {
  ArrayXf data(5);
  data << 1, 0, 1, 0, 1;
  ThreeTapSmoother<ArrayXf> smoother(data.size());
  smoother.SmoothCircular(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear<float>({0.9, 0.2, 0.8, 0.2, 0.9}, 1e-5));
}

// Same test as above with doubles.
TEST(SignalEigenUtilTest, CircularSmoothThreeImpulsesDouble) {
  ArrayXd data(5);
  data << 1, 0, 1, 0, 1;
  ThreeTapSmoother<ArrayXd> smoother(data.size());
  smoother.SmoothCircular(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear<double>({0.9, 0.2, 0.8, 0.2, 0.9}, 1e-5));
}

// Same test as above with complex floats.
TEST(SignalEigenUtilTest, CircularSmoothThreeImpulsesComplex) {
  ArrayXcf data(5);
  data << std::complex<float>(1.0f, 1.0f), std::complex<float>(0.0f),
      std::complex<float>(1.0f, 1.0f), std::complex<float>(0.0f),
      std::complex<float>(1.0f, 1.0f);
  ArrayXcf expected(5);
  expected << std::complex<float>(0.9f, 0.9f), std::complex<float>(0.2f, 0.2f),
      std::complex<float>(0.8f, 0.8f), std::complex<float>(0.2f, 0.2f),
      std::complex<float>(0.9f, 0.9f);
  ThreeTapSmoother<ArrayXcf> smoother(data.size());
  smoother.SmoothCircular(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear(expected, 1e-5));
}

TEST(SignalEigenUtilTest, CircularSmoothAlreadySmooth) {
  ArrayXf data = ArrayXf::Ones(200);
  ThreeTapSmoother<ArrayXf> smoother(data.size());
  smoother.SmoothCircular(0.1, &data);
  EXPECT_THAT(data, EigenEach(testing::FloatNear(1.0, 1e-5)));
}

// Tests the reflected edges cases.
TEST(SignalEigenUtilTest, ReflectedSmoothImpulseResponse) {
  ArrayXf data(4);
  data << 1, 0, 0, 0;
  ThreeTapSmoother<ArrayXf> smoother(data.size());
  smoother.SmoothSymmetric(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear<float>({0.8, 0.1, 0, 0.0}, 1e-5));
}

TEST(SignalEigenUtilTest, ReflectedSmoothTwoImpulses) {
  ArrayXf data(5);
  data << 1, 0, 0, 0, 1;
  ThreeTapSmoother<ArrayXf> smoother(data.size());
  smoother.SmoothSymmetric(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear<float>({0.8, 0.1, 0, 0.1, 0.8}, 1e-5));
}

// Same test as above with complex floats.
TEST(SignalEigenUtilTest, ReflectedSmoothThreeImpulsesComplex) {
  ArrayXcf data(5);
  data << std::complex<float>(1.0f, 1.0f), std::complex<float>(0.0f),
      std::complex<float>(1.0f, 1.0f), std::complex<float>(0.0f),
      std::complex<float>(1.0f, 1.0f);
  ArrayXcf expected(5);
  expected << std::complex<float>(0.8f, 0.8f), std::complex<float>(0.2f, 0.2f),
      std::complex<float>(0.8f, 0.8f), std::complex<float>(0.2f, 0.2f),
      std::complex<float>(0.8f, 0.8f);
  ThreeTapSmoother<ArrayXcf> smoother(data.size());
  smoother.SmoothSymmetric(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear(expected, 1e-5));
}

TEST(SignalEigenUtilTest, ReflectedSmoothAlreadySmooth) {
  ArrayXf data = ArrayXf::Ones(200);
  ThreeTapSmoother<ArrayXf> smoother(data.size());
  smoother.SmoothSymmetric(0.1, &data);
  EXPECT_THAT(data, EigenEach(testing::FloatNear(1.0, 1e-5)));
}

// Tests the zero padding cases.
TEST(SignalEigenUtilTest, ZeropadSmooth) {
  ArrayXf data(5);
  data << 1, 0.5, 0, 0.5, 1;
  ThreeTapSmoother<ArrayXf> smoother(data.size());
  smoother.SmoothZeroPadding(0.1, &data);
  EXPECT_THAT(data, EigenArrayNear<float>({0.85, 0.5, 0.1, 0.5, 0.85}, 1e-5));
}

// Benchmark results:
// Run on lpac16 (32 X 2600 MHz CPUs); 2016-12-25T20:27:49.811570322-08:00
// CPU: Intel Sandybridge (16 cores) dL1:32KB dL2:256KB dL3:20MB
// Benchmark           Time(ps)    CPU(ps) Iterations
// --------------------------------------------------
// BM_CircularSmooth     115844     115558   60386526

// Benchmark for the smoother.
void BM_CircularSmooth(benchmark::State& state) {
  ArrayXf arr = ArrayXf::Random(180);
  ThreeTapSmoother<ArrayXf> smoother(arr.size());
  while (state.KeepRunning()) {
    smoother.SmoothCircular(0.1, &arr);
    benchmark::DoNotOptimize(arr);
  }
}
BENCHMARK(BM_CircularSmooth);

}  // namespace
}  // namespace audio_dsp
