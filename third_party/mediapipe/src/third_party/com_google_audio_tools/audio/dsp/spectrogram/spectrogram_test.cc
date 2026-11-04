/*
 * Copyright 2026 Google LLC
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

#include "audio/dsp/spectrogram/spectrogram.h"

#include <stdlib.h>

#include <complex>
#include <vector>

#include "audio/dsp/spectrogram/test_file_utils.h"
#include "audio/dsp/window_functions.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

ABSL_FLAG(std::string, spectrogram_dump_file, "",
          "File to dump the computed spectrogram to. "
          "The data can be read into MATLAB using read_complex_doubles.m");

ABSL_FLAG(std::string, window_dump_file, "",
          "File to dump the windowing function used by the spectrogram.");

namespace audio_dsp {

const char kInputFilename[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment.wav";

const char kExpectedFilename[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment_spectrogram.csv";

const int kDataVectorLength = 257;
const int kNumberOfFramesInTestData = 178;

const char kExpectedNonPowerOfTwoFilename[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment_spectrogram_400_200.csv";
const int kNonPowerOfTwoDataVectorLength = 257;
const int kNumberOfFramesInNonPowerOfTwoTestData = 228;

const char kExpectedHannWindowFilename[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment_spectrogram_hann.csv";

TEST(SpectrogramTest, TooLittleDataYieldsNoFrames) {
  Spectrogram sgram;
  sgram.Initialize(400, 200);
  std::vector<double> input;
  // Generate 44 samples of audio.
  SineWave(44100, 1000.0, 0.001, &input);
  EXPECT_EQ(44, input.size());
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  EXPECT_EQ(0, output.size());
}

TEST(SpectrogramTest, StepSizeSmallerThanWindow) {
  Spectrogram sgram;
  EXPECT_TRUE(sgram.Initialize(400, 200));
  std::vector<double> input;
  // Generate 661 samples of audio.
  SineWave(44100, 1000.0, 0.015, &input);
  EXPECT_EQ(661, input.size());
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  EXPECT_EQ(2, output.size());
}

TEST(SpectrogramTest, StepSizeBiggerThanWindow) {
  Spectrogram sgram;
  EXPECT_TRUE(sgram.Initialize(200, 400));
  std::vector<double> input;
  // Generate 882 samples of audio.
  SineWave(44100, 1000.0, 0.02, &input);
  EXPECT_EQ(882, input.size());
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  EXPECT_EQ(2, output.size());
}

TEST(SpectrogramTest, StepSizeBiggerThanWindow2) {
  Spectrogram sgram;
  EXPECT_TRUE(sgram.Initialize(200, 400));
  std::vector<double> input;
  // Generate more than 600 but fewer than 800 samples of audio.
  SineWave(44100, 1000.0, 0.016, &input);
  EXPECT_GT(input.size(), 600);
  EXPECT_LT(input.size(), 800);
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  EXPECT_EQ(2, output.size());
}

TEST(SpectrogramTest,
     MultipleCallsToComputeComplexSpectrogramMayYieldDifferentNumbersOfFrames) {
  // Repeatedly pass inputs with "extra" samples beyond complete windows
  // and check that the excess points cumulate to eventually cause an
  // extra output frame.
  Spectrogram sgram;
  sgram.Initialize(200, 400);
  std::vector<double> input;
  // Generate 882 samples of audio.
  SineWave(44100, 1000.0, 0.02, &input);
  EXPECT_EQ(882, input.size());
  std::vector<std::vector<std::complex<double>>> output;
  const std::vector<int> expected_output_sizes = {
      2,  // One pass of input leaves 82 samples buffered after two steps of
          // 400.
      2,  // Passing in 882 samples again will now leave 164 samples buffered.
      3,  // Third time gives 246 extra samples, triggering an extra output
          // frame.
  };
  for (int expected_output_size : expected_output_sizes) {
    sgram.ComputeComplexSpectrogram(input, &output);
    EXPECT_EQ(expected_output_size, output.size());
  }
}

TEST(SpectrogramTest, CumulatingExcessInputsForOverlappingFrames) {
  // Input frames that don't fit into whole windows are cumulated even when
  // the windows have overlap (similar to
  // MultipleCallsToComputeComplexSpectrogramMayYieldDifferentNumbersOfFrames
  // but with window size/hop size swapped).
  Spectrogram sgram;
  sgram.Initialize(400, 200);
  std::vector<double> input;
  // Generate 882 samples of audio.
  SineWave(44100, 1000.0, 0.02, &input);
  EXPECT_EQ(882, input.size());
  std::vector<std::vector<std::complex<double>>> output;
  const std::vector<int> expected_output_sizes = {
      3,  // Windows 0..400, 200..600, 400..800 with 82 samples buffered.
      4,  // 1764 frames input; outputs from 600, 800, 1000, 1200..1600.
      5,  // 2646 frames in; outputs from 1400, 1600, 1800, 2000, 2200..2600.
  };
  for (int expected_output_size : expected_output_sizes) {
    sgram.ComputeComplexSpectrogram(input, &output);
    EXPECT_EQ(expected_output_size, output.size());
  }
}

TEST(SpectrogramTest, StepSizeEqualToWindowWorks) {
  Spectrogram sgram;
  sgram.Initialize(200, 200);
  std::vector<double> input;
  // Generate 2205 samples of audio.
  SineWave(44100, 1000.0, 0.05, &input);
  EXPECT_EQ(2205, input.size());
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  EXPECT_EQ(11, output.size());
}

TEST(SpectrogramTest, StepSizeEqualToWindowWorksWithMatchingFftSize) {
  Spectrogram sgram;
  constexpr int kFftSize = 128;
  constexpr int kWindowSize = 128;
  sgram.Initialize(kWindowSize, kWindowSize, kFftSize);
  std::vector<double> input;
  // Generate 2205 samples of audio.
  SineWave(44100, 1000.0, 0.05, &input);
  const int num_input_samples = input.size();
  EXPECT_EQ(num_input_samples, 44100 * 0.05);
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  EXPECT_EQ(num_input_samples / kWindowSize, output.size());
  const int expected_num_frequency_bins = kFftSize / 2 + 1;
  EXPECT_EQ(output[0].size(), expected_num_frequency_bins);
}

TEST(SpectrogramTest, StepSizeEqualToWindowWorksWith2XFftSize) {
  Spectrogram sgram;
  constexpr int kFftSize = 256;
  constexpr int kWindowSize = 128;
  sgram.Initialize(kWindowSize, kWindowSize, kFftSize);
  std::vector<double> input;
  // Generate 2205 samples of audio.
  SineWave(44100, 1000.0, 0.05, &input);
  const int num_input_samples = input.size();
  EXPECT_EQ(num_input_samples, 44100 * 0.05);
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  EXPECT_EQ(num_input_samples / kWindowSize, output.size());
  const int expected_num_frequency_bins = kFftSize / 2 + 1;
  EXPECT_EQ(output[0].size(), expected_num_frequency_bins);
}

template <class ExpectedSample, class ActualSample>
void CompareComplexData(
    const std::vector<std::vector<std::complex<ExpectedSample>>>& expected,
    const std::vector<std::vector<std::complex<ActualSample>>>& actual,
    double tolerance) {
  ASSERT_EQ(actual.size(), expected.size());
  for (int i = 0; i < expected.size(); ++i) {
    ASSERT_EQ(expected[i].size(), actual[i].size());
    for (int j = 0; j < expected[i].size(); ++j) {
      ASSERT_NEAR(std::real(expected[i][j]), std::real(actual[i][j]), tolerance)
          << ": where i=" << i << " and j=" << j << ".";
      ASSERT_NEAR(std::imag(expected[i][j]), std::imag(actual[i][j]), tolerance)
          << ": where i=" << i << " and j=" << j << ".";
    }
  }
}

template <class Sample>
double GetMaximumAbsolute(const std::vector<std::vector<Sample>>& spectrogram) {
  double max_absolute = 0.0;
  for (int i = 0; i < spectrogram.size(); ++i) {
    for (int j = 0; j < spectrogram[i].size(); ++j) {
      double absolute_value = std::abs(spectrogram[i][j]);
      if (absolute_value > max_absolute) {
        max_absolute = absolute_value;
      }
    }
  }
  return max_absolute;
}

template <class ExpectedSample, class ActualSample>
void CompareMagnitudeData(
    const std::vector<std::vector<std::complex<ExpectedSample>>>&
        expected_complex_output,
    const std::vector<std::vector<ActualSample>>& actual_squared_magnitude,
    double tolerance) {
  ASSERT_EQ(actual_squared_magnitude.size(), expected_complex_output.size());
  for (int i = 0; i < expected_complex_output.size(); ++i) {
    ASSERT_EQ(expected_complex_output[i].size(),
              actual_squared_magnitude[i].size());
    for (int j = 0; j < expected_complex_output[i].size(); ++j) {
      ASSERT_NEAR(std::norm(expected_complex_output[i][j]),
                  actual_squared_magnitude[i][j], tolerance)
          << ": where i=" << i << " and j=" << j << ".";
    }
  }
}

TEST(SpectrogramTest, ReInitializationWorks) {
  Spectrogram sgram;
  sgram.Initialize(512, 256);
  std::vector<double> input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &input));
  std::vector<std::vector<std::complex<double>>> first_output;
  std::vector<std::vector<std::complex<double>>> second_output;
  sgram.Initialize(512, 256);
  sgram.ComputeComplexSpectrogram(input, &first_output);
  // Re-Initialize it.
  sgram.Initialize(512, 256);
  sgram.ComputeComplexSpectrogram(input, &second_output);
  // Verify identical outputs.
  ASSERT_EQ(first_output.size(), second_output.size());
  int slice_size = first_output[0].size();
  for (int i = 0; i < first_output.size(); ++i) {
    ASSERT_EQ(slice_size, first_output[i].size());
    ASSERT_EQ(slice_size, second_output[i].size());
    for (int j = 0; j < slice_size; ++j) {
      ASSERT_EQ(first_output[i][j], second_output[i][j]);
    }
  }
}

TEST(SpectrogramTest, ResetWorks) {
  Spectrogram sgram;
  sgram.Initialize(512, 256);
  std::vector<double> input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &input));
  std::vector<std::vector<std::complex<double>>> first_output;
  std::vector<std::vector<std::complex<double>>> second_output;
  sgram.Initialize(512, 256);
  sgram.ComputeComplexSpectrogram(input, &first_output);
  // Reset it.
  sgram.ResetSampleBuffer();
  sgram.ComputeComplexSpectrogram(input, &second_output);
  // Verify identical outputs.
  ASSERT_EQ(first_output.size(), second_output.size());
  int slice_size = first_output[0].size();
  for (int i = 0; i < first_output.size(); ++i) {
    ASSERT_EQ(slice_size, first_output[i].size());
    ASSERT_EQ(slice_size, second_output[i].size());
    for (int j = 0; j < slice_size; ++j) {
      ASSERT_EQ(first_output[i][j], second_output[i][j]);
    }
  }
  // And, just to check, if you do this without the Reset, it fails:
  sgram.ComputeComplexSpectrogram(input, &second_output);
  // Non-identical outputs.
  ASSERT_NE(first_output.size(), second_output.size());
}

TEST(SpectrogramTest, ComputedComplexDataAgreeWithMatlab) {
  const int kInputDataLength = 45870;
  Spectrogram sgram;
  sgram.Initialize(512, 256);
  std::vector<double> input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &input));
  EXPECT_EQ(kInputDataLength, input.size());
  std::vector<std::vector<std::complex<double>>> expected_output;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kExpectedFilename), &expected_output);
  EXPECT_EQ(kNumberOfFramesInTestData, expected_output.size());
  EXPECT_EQ(kDataVectorLength, expected_output[0].size());
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  if (!absl::GetFlag(FLAGS_spectrogram_dump_file).empty()) {
    ABSL_CHECK(WriteComplexVectorToRawDoubleFile(
        absl::GetFlag(FLAGS_spectrogram_dump_file), output));
  }
  if (!absl::GetFlag(FLAGS_window_dump_file).empty()) {
    const auto& window = sgram.GetWindow();
    ABSL_CHECK(WriteDoubleVectorToFile(
        absl::GetFlag(FLAGS_window_dump_file), window));
  }
  CompareComplexData(expected_output, output, 1e-6);
}

TEST(SpectrogramTest, ComputedFloatComplexDataAgreeWithMatlab) {
  const int kInputDataLength = 45870;
  Spectrogram sgram;
  sgram.Initialize(512, 256);
  std::vector<double> double_input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &double_input));
  std::vector<float> input;
  input.assign(double_input.begin(), double_input.end());
  EXPECT_EQ(kInputDataLength, input.size());
  std::vector<std::vector<std::complex<double>>> expected_output;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kExpectedFilename), &expected_output);
  EXPECT_EQ(kNumberOfFramesInTestData, expected_output.size());
  EXPECT_EQ(kDataVectorLength, expected_output[0].size());
  std::vector<std::vector<std::complex<float>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  CompareComplexData(expected_output, output, 1e-4);
}

TEST(SpectrogramTest, ComputedSquaredMagnitudeDataAgreeWithMatlab) {
  const int kInputDataLength = 45870;
  Spectrogram sgram;
  sgram.Initialize(512, 256);
  std::vector<double> input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &input));
  EXPECT_EQ(kInputDataLength, input.size());
  std::vector<std::vector<std::complex<double>>> expected_output;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kExpectedFilename), &expected_output);
  EXPECT_EQ(kNumberOfFramesInTestData, expected_output.size());
  EXPECT_EQ(kDataVectorLength, expected_output[0].size());
  std::vector<std::vector<double>> output;
  sgram.ComputeSquaredMagnitudeSpectrogram(input, &output);
  CompareMagnitudeData(expected_output, output, 1e-6);
}

TEST(SpectrogramTest, ComputedFloatSquaredMagnitudeDataAgreeWithMatlab) {
  const int kInputDataLength = 45870;
  Spectrogram sgram;
  sgram.Initialize(512, 256);
  std::vector<double> double_input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &double_input));
  EXPECT_EQ(kInputDataLength, double_input.size());
  std::vector<float> input;
  input.assign(double_input.begin(), double_input.end());
  std::vector<std::vector<std::complex<double>>> expected_output;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kExpectedFilename), &expected_output);
  EXPECT_EQ(kNumberOfFramesInTestData, expected_output.size());
  EXPECT_EQ(kDataVectorLength, expected_output[0].size());
  std::vector<std::vector<float>> output;
  sgram.ComputeSquaredMagnitudeSpectrogram(input, &output);
  double max_absolute = GetMaximumAbsolute(output);
  EXPECT_GT(max_absolute, 2300.0);  // Verify that we have some big numbers.
  // Squaring increases dynamic range; max square is about 2300,
  // so 2e-4 is about 7 decimal digits; not bad for a float.
  CompareMagnitudeData(expected_output, output, 2e-4);
}

TEST(SpectrogramTest, ComputedNonPowerOfTwoComplexDataAgreeWithMatlab) {
  const int kInputDataLength = 45870;
  Spectrogram sgram;
  sgram.Initialize(400, 200);
  std::vector<double> input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &input));
  EXPECT_EQ(kInputDataLength, input.size());
  std::vector<std::vector<std::complex<double>>> expected_output;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kExpectedNonPowerOfTwoFilename),
      &expected_output);
  EXPECT_EQ(kNumberOfFramesInNonPowerOfTwoTestData, expected_output.size());
  EXPECT_EQ(kNonPowerOfTwoDataVectorLength, expected_output[0].size());
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  CompareComplexData(expected_output, output, 1e-6);
}

TEST(SpectrogramTest, TemplatedComputeWorksForComplexOutput) {
  // Recapitulates ComputedComplexDataAgreeWithMatlab with ComputeSpectrogram
  // instead of ComputeComplexSpectrogram.
  const int kInputDataLength = 45870;
  Spectrogram sgram;
  sgram.Initialize(512, 256);
  std::vector<double> input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &input));
  EXPECT_EQ(kInputDataLength, input.size());
  std::vector<std::vector<std::complex<double>>> expected_output;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kExpectedFilename), &expected_output);
  EXPECT_EQ(kNumberOfFramesInTestData, expected_output.size());
  EXPECT_EQ(kDataVectorLength, expected_output[0].size());
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeSpectrogram(input, &output);
  CompareComplexData(expected_output, output, 1e-6);
}

TEST(SpectrogramTest, TemplatedComputeWorksForSquaredMagnitudeOutput) {
  // Recapitulates ComputedSquaredMagnitudeDataAgreeWithMatlab with
  // ComputeSpectrogram instead of ComputeSquaredMagnitudeSpectrogram.
  const int kInputDataLength = 45870;
  Spectrogram sgram;
  sgram.Initialize(512, 256);
  std::vector<double> input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &input));
  EXPECT_EQ(kInputDataLength, input.size());
  std::vector<std::vector<std::complex<double>>> expected_output;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kExpectedFilename), &expected_output);
  EXPECT_EQ(kNumberOfFramesInTestData, expected_output.size());
  EXPECT_EQ(kDataVectorLength, expected_output[0].size());
  std::vector<std::vector<double>> output;
  sgram.ComputeSpectrogram(input, &output);
  CompareMagnitudeData(expected_output, output, 1e-6);
}

TEST(SpectrogramTest, InitializeWithAWindowFunction) {
  const int kInputDataLength = 45870;

  std::vector<double> hann_window;
  audio_dsp::HannWindow().GetPeriodicSamples(512, &hann_window);

  Spectrogram sgram;
  sgram.Initialize(hann_window, 256);
  std::vector<double> input;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kInputFilename), &input));
  EXPECT_EQ(kInputDataLength, input.size());
  std::vector<std::vector<std::complex<double>>> expected_output;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kExpectedHannWindowFilename),
      &expected_output);
  EXPECT_EQ(kNumberOfFramesInTestData, expected_output.size());
  EXPECT_EQ(kDataVectorLength, expected_output[0].size());
  std::vector<std::vector<std::complex<double>>> output;
  sgram.ComputeComplexSpectrogram(input, &output);
  CompareComplexData(expected_output, output, 1e-6);
}

}  // namespace audio_dsp