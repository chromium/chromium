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

#include "audio/dsp/spectrogram/inverse_spectrogram.h"

#include <stdlib.h>

#include <complex>
#include <vector>

#include "audio/dsp/spectrogram/spectrogram.h"
#include "audio/dsp/spectrogram/test_file_utils.h"
#include "audio/dsp/window_functions.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

ABSL_FLAG(std::string, waveform_dump_file, "",
          "File to dump the reconstructed waveform to.");

ABSL_FLAG(
    std::string, windowed_waveform_dump_file, "",
    "File to dump the reconstructed waveform with hann synthesis window to.");

namespace audio_dsp {

using std::complex;
using testing::DoubleNear;
using testing::Pointwise;
using testing::SizeIs;

const char kInputFilename[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment_spectrogram.csv";

// For the double-size-window reconstruction test:
const char kInputFilename2[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment_spectrogram_1024_fft.csv";

const char kExpectedFilename[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment_reconstructed.wav";

const char kExpectedPerfectReconstructionFileName[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment.wav";

const char kInputSpectrogramWithWindowFileName[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment_spectrogram_hann.csv";

const char kExpectedWithWindowFilename[] =
    "audio/dsp/spectrogram/"
    "testdata/short_test_segment_reconstructed_hann.wav";

void FakeSpectrogram(int slices,
                     int values_per_slice,
                     std::vector<std::vector<complex<double> > >* output) {
  std::vector<complex<double> > slice;
  slice.resize(values_per_slice, 0.0);
  output->clear();
  output->resize(slices, slice);
}

template <typename T1, typename T2>
void AssertVectorsNear(std::vector<T1> v1, std::vector<T2> v2,
                       double tolerance) {
  ASSERT_EQ(v1.size(), v2.size());
  for (int i = 0; i < v1.size(); ++i) {
    ASSERT_NEAR(v1[i], v2[i], tolerance)  << ": where i=" << i;
  }
}

TEST(InverseSpectrogramTest, NothingInNothingOut) {
  InverseSpectrogram inverse_sgram;
  inverse_sgram.Initialize(512, 200);
  std::vector<std::vector<complex<double> > > input;
  EXPECT_EQ(0, input.size());
  std::vector<double> output;
  EXPECT_TRUE(inverse_sgram.Process(input, &output));
  EXPECT_EQ(0, output.size());
  EXPECT_TRUE(inverse_sgram.Flush(&output));
  EXPECT_EQ(0, output.size());
}

TEST(InverseSpectrogramTest, ReturnsFalseWhenNotInitialized) {
  InverseSpectrogram inverse_sgram;
  std::vector<std::vector<complex<double> > > input;
  std::vector<double> output;
  EXPECT_FALSE(inverse_sgram.Process(input, &output));
  EXPECT_FALSE(inverse_sgram.Flush(&output));
}

TEST(InverseSpectrogramTest, InitializeDoesntLikeTinyWindow) {
  InverseSpectrogram inverse_sgram;
  EXPECT_FALSE(inverse_sgram.Initialize(1, 100));
}

TEST(InverseSpectrogramTest, InitializeDoesntLikeZeroStep) {
  InverseSpectrogram inverse_sgram;
  EXPECT_FALSE(inverse_sgram.Initialize(128, 0));
}

TEST(InverseSpectrogramTest, InitializeDoesntNonPowerOf2FFTLength) {
  InverseSpectrogram inverse_sgram;
  EXPECT_FALSE(inverse_sgram.Initialize(100, 50));
}

TEST(InverseSpectrogramTest, InitializeDoesLikeMinimumParameters) {
  InverseSpectrogram inverse_sgram;
  EXPECT_TRUE(inverse_sgram.Initialize(2, 1));
}

TEST(InverseSpectrogramTest, OneFrameInOneFrameOut) {
  InverseSpectrogram inverse_sgram;
  const int kFFTLength = 512;
  const int kStepLength = 100;
  EXPECT_TRUE(inverse_sgram.Initialize(kFFTLength, kStepLength));
  std::vector<std::vector<complex<double> > > input;
  FakeSpectrogram(1, 257, &input);
  EXPECT_EQ(1, input.size());
  std::vector<double> output;
  EXPECT_TRUE(inverse_sgram.Process(input, &output));
  EXPECT_EQ(output.size(), output.capacity());
  EXPECT_EQ(kStepLength, output.size());
  EXPECT_TRUE(inverse_sgram.Flush(&output));
  EXPECT_EQ(kFFTLength - kStepLength, output.size());
}

TEST(InverseSpectrogramTest, CanCallProcessAgainAfterFlush) {
  InverseSpectrogram inverse_sgram;
  const int kFFTLength = 512;
  const int kStepLength = 100;
  EXPECT_TRUE(inverse_sgram.Initialize(kFFTLength, kStepLength));
  std::vector<std::vector<complex<double> > > input;
  FakeSpectrogram(1, 257, &input);
  EXPECT_EQ(1, input.size());
  std::vector<double> output;
  EXPECT_TRUE(inverse_sgram.Process(input, &output));
  EXPECT_EQ(kStepLength, output.size());
  EXPECT_TRUE(inverse_sgram.Flush(&output));
  EXPECT_EQ(kFFTLength - kStepLength, output.size());
  EXPECT_TRUE(inverse_sgram.Process(input, &output));
  EXPECT_EQ(kStepLength, output.size());
}

TEST(InverseSpectrogramTest, SecondCallToFlushReurnsNothing) {
  InverseSpectrogram inverse_sgram;
  const int kFFTLength = 512;
  const int kStepLength = 100;
  inverse_sgram.Initialize(kFFTLength, kStepLength);
  std::vector<std::vector<complex<double> > > input;
  FakeSpectrogram(1, 257, &input);
  std::vector<double> output;
  EXPECT_TRUE(inverse_sgram.Process(input, &output));
  EXPECT_EQ(kStepLength, output.size());
  EXPECT_TRUE(inverse_sgram.Flush(&output));
  EXPECT_EQ(kFFTLength - kStepLength, output.size());
  EXPECT_TRUE(inverse_sgram.Flush(&output));
  EXPECT_EQ(0, output.size());
}

TEST(InverseSpectrogramTest, OverlapTreatedCorrectly) {
  InverseSpectrogram inverse_sgram;
  const int kFFTLength = 512;
  const int kStepLength = 10;
  inverse_sgram.Initialize(kFFTLength, kStepLength);
  std::vector<std::vector<complex<double> > > input;
  FakeSpectrogram(2, 257, &input);
  EXPECT_EQ(2, input.size());
  std::vector<double> output;
  inverse_sgram.Process(input, &output);
  EXPECT_EQ(2 * kStepLength, output.size());
  EXPECT_TRUE(inverse_sgram.Flush(&output));
  EXPECT_EQ(kFFTLength - kStepLength, output.size());
}

TEST(InverseSpectrogramTest, MultipleCallsToProcessWork) {
  InverseSpectrogram inverse_sgram;
  const int kFFTLength = 512;
  const int kStepLength = 10;
  inverse_sgram.Initialize(kFFTLength, kStepLength);
  std::vector<std::vector<complex<double> > > input;
  FakeSpectrogram(1, 257, &input);
  std::vector<double> output;
  inverse_sgram.Process(input, &output);
  EXPECT_EQ(kStepLength, output.size());
  inverse_sgram.Process(input, &output);
  EXPECT_EQ(kStepLength, output.size());
  EXPECT_TRUE(inverse_sgram.Flush(&output));
  EXPECT_EQ(kFFTLength - kStepLength, output.size());
}

TEST(InverseSpectrogramTest, NoOverlapTreatedCorrectly) {
  InverseSpectrogram inverse_sgram;
  const int kFFTLength = 512;
  const int kStepLength = 520;
  inverse_sgram.Initialize(kFFTLength, kStepLength);
  std::vector<std::vector<complex<double> > > input;
  FakeSpectrogram(2, 257, &input);
  EXPECT_EQ(2, input.size());
  std::vector<double> output;
  EXPECT_TRUE(inverse_sgram.Process(input, &output));
  EXPECT_EQ(2 * kFFTLength, output.size());
  EXPECT_TRUE(inverse_sgram.Flush(&output));
  EXPECT_EQ(0, output.size());
}

TEST(InverseSpectrogramTest, FlushReInitializes) {
  std::vector<std::vector<complex<double> > > double_input;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kInputFilename), &double_input);
  std::vector<double> first_output;
  std::vector<double> second_output;
  InverseSpectrogram inverse_sgram;
  inverse_sgram.Initialize(512, 256);
  inverse_sgram.Process(double_input, &first_output);
  std::vector<float> unused_tail;
  inverse_sgram.Flush(&unused_tail);
  inverse_sgram.Process(double_input, &second_output);
  for (int i = 0; i < first_output.size(); ++i) {
    ASSERT_EQ(first_output[i], second_output[i]);
  }
}

TEST(InverseSpectrogramTest, ReInitializationWithoutFlushWorks) {
  std::vector<std::vector<complex<double> > > double_input;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kInputFilename), &double_input);
  std::vector<double> first_output;
  std::vector<double> second_output;
  InverseSpectrogram inverse_sgram;
  inverse_sgram.Initialize(512, 256);
  inverse_sgram.Process(double_input, &first_output);
  inverse_sgram.Initialize(512, 256);
  inverse_sgram.Process(double_input, &second_output);
  for (int i = 0; i < first_output.size(); ++i) {
    ASSERT_EQ(first_output[i], second_output[i]);
  }
}

TEST(InverseSpectrogramTest, OutputAgreesAcrossArgTypes) {
  std::vector<std::vector<complex<double> > > double_input;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kInputFilename), &double_input);
  std::vector<double> double_output;
  InverseSpectrogram inverse_sgram;
  inverse_sgram.Initialize(512, 256);
  inverse_sgram.Process(double_input, &double_output);
  std::vector<float> double_tail;
  inverse_sgram.Flush(&double_tail);
  // Re-run with double->float, float->double, float->float and compare.
  // Also both types of Flush output, to verify all the template combinations
  // are instantiated and working.
  std::vector<std::vector<complex<float>>> float_input(double_input.size());
  // Copy data to other supported input arg type.
  for (int i = 0; i < double_input.size(); ++i) {
    float_input[i].assign(double_input[i].begin(), double_input[i].end());
  }
  // And make a place for output of other supported arg type.
  std::vector<float> float_output;
  inverse_sgram.Initialize(512, 256);
  inverse_sgram.Process(double_input, &float_output);
  std::vector<float> float_tail;
  inverse_sgram.Flush(&float_tail);
  AssertVectorsNear(double_output, float_output, 1e-6);
  AssertVectorsNear(double_tail, float_tail, 1e-6);
  inverse_sgram.Initialize(512, 256);
  inverse_sgram.Process(float_input, &float_output);
  inverse_sgram.Flush(&double_tail);  // Re-using double_tail.
  AssertVectorsNear(double_output, float_output, 1e-6);
  AssertVectorsNear(double_tail, float_tail, 1e-6);
  inverse_sgram.Initialize(512, 256);
  inverse_sgram.Process(float_input, &double_output);
  AssertVectorsNear(double_output, float_output, 1e-6);
}

TEST(InverseSpectrogramTest, OutputAgreesWithMatlab) {
  const int kDataVectorLength = 257;
  const int kNumberOfFramesInTestData = 178;
  const int kStep = 256;
  const int kNumberOfOutputSamples = 45824;
  std::vector<std::vector<complex<double> > > input;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kInputFilename), &input);
  EXPECT_EQ(kNumberOfFramesInTestData, input.size());
  EXPECT_EQ(kDataVectorLength, input[0].size());
  std::vector<double> expected_output;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kExpectedFilename),
      &expected_output));
  EXPECT_EQ(kNumberOfOutputSamples, expected_output.size());
  std::vector<double> output;
  InverseSpectrogram inverse_sgram;
  inverse_sgram.Initialize(512, kStep);
  inverse_sgram.Process(input, &output);
  EXPECT_EQ(output.size(), expected_output.size() - kStep);
  if (!absl::GetFlag(FLAGS_waveform_dump_file).empty()) {
    ABSL_CHECK(WriteDoubleVectorToFile(absl::GetFlag(FLAGS_waveform_dump_file),
                                  output));
  }
  std::vector<double> tail;
  inverse_sgram.Flush(&tail);
  output.insert(output.end(), tail.begin(), tail.end());
  AssertVectorsNear(output, expected_output, 1e-4);

  // Verify the case of using matrix as input
  Eigen::MatrixXcf input_matrix(kDataVectorLength, kNumberOfFramesInTestData);
  for (int col = 0; col < kNumberOfFramesInTestData; ++col) {
    for (int row = 0; row < kDataVectorLength; ++row) {
      input_matrix(row, col).real(static_cast<float>(input[col][row].real()));
      input_matrix(row, col).imag(static_cast<float>(input[col][row].imag()));
    }
  }
  std::vector<float> output_matrix;
  inverse_sgram.Process(input_matrix, &output_matrix);
  EXPECT_EQ(output_matrix.size(), output_matrix.capacity());
  EXPECT_EQ(output_matrix.size(), expected_output.size() - kStep);
  std::vector<float> tail_matrix;
  inverse_sgram.Flush(&tail_matrix);
  output_matrix.insert(output_matrix.end(), tail_matrix.begin(),
                       tail_matrix.end());
  AssertVectorsNear(output_matrix, expected_output, 1e-4);
}

TEST(InverseSpectrogramTest, LargerFFTSizeReconstructsSimilar) {
  const int kFFTSize = 1024;
  const int kDataVectorLength = 1 + kFFTSize / 2;
  const int kNumberOfFramesInTestData = 178;
  const int kStep = 256;
  const int kNumberOfOutputSamples = 45824;
  std::vector<std::vector<complex<double> > > input;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kInputFilename2), &input);
  EXPECT_EQ(kNumberOfFramesInTestData, input.size());
  EXPECT_EQ(kDataVectorLength, input[0].size());
  std::vector<double> expected_output;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kExpectedFilename),
      &expected_output));
  EXPECT_EQ(kNumberOfOutputSamples, expected_output.size());
  std::vector<double> output;
  InverseSpectrogram inverse_sgram;
  inverse_sgram.Initialize(kFFTSize, kStep);
  inverse_sgram.Process(input, &output);
  // Stay away from the start where the window makes it different,
  // and away from the end by not flushing.
  for (int i = 512; i < output.size(); ++i) {
    ASSERT_NEAR(output[i], expected_output[i], 1e-4) << ": where i=" << i;
  }

  // Verify the case of using matrix as input after flush
  std::vector<double> tail;
  inverse_sgram.Flush(&tail);

  Eigen::MatrixXcf input_matrix(kDataVectorLength, kNumberOfFramesInTestData);
  for (int col = 0; col < kNumberOfFramesInTestData; ++col) {
    for (int row = 0; row < kDataVectorLength; ++row) {
      input_matrix(row, col).real(static_cast<float>(input[col][row].real()));
      input_matrix(row, col).imag(static_cast<float>(input[col][row].imag()));
    }
  }
  std::vector<float> output_matrix;
  inverse_sgram.Process(input_matrix, &output_matrix);
  for (int i = 512; i < output_matrix.size(); ++i) {
    ASSERT_NEAR(output_matrix[i], expected_output[i], 1e-4)
        << ": where i=" << i;
  }
}

TEST(InverseSpectrogramTest, GenerateSynthesisWindow) {
  // Test that the normalized inverse window has unit gain at each window phase.
  const int kStep = 160;
  const int kFrameLength = 256;
  std::vector<double> hann_window;
  audio_dsp::HannWindow().GetPeriodicSamples(kFrameLength, &hann_window);
  std::vector<double> inverse_window =
      InverseSpectrogram::GenerateSynthesisWindow(hann_window, kStep);

  EXPECT_THAT(inverse_window, SizeIs(kFrameLength));

  std::vector<double> product(hann_window);
  for (int i = 0; i < kFrameLength; ++i) {
    product[i] *= inverse_window[i];
  }
  // Expect unit gain at each phase of the window.
  for (int phase = 0; phase < kStep; ++phase) {
    double phases_gain = 0;
    for (int overlaped = phase; overlaped < kFrameLength; overlaped += kStep) {
      phases_gain += product[overlaped];
    }
    EXPECT_THAT(phases_gain, DoubleNear(1.0, 1e-5));
  }
}

TEST(InverseSpectrogramTest, GenerateSynthesisWindowSpecialCase) {
  // Test NormalizeInverseWindow in special overlap = 3/4 case. Cases in which
  // frame_length is an integer multiple of 4 * frame_step are special because
  // they allow exact reproduction of the waveform with a squared Hann window
  // (Hann window in both forward and reverse transforms). In the case where
  // frame_length = 4 * frame_step, that combination produces a constant gain
  // of 1.5, and so the corrected window will be the Hann window / 1.5.
  const int kStep = 64;
  const int kFrameLength = 256;
  std::vector<double> hann_window;
  audio_dsp::HannWindow().GetPeriodicSamples(kFrameLength, &hann_window);
  std::vector<double> actual_inverse =
      InverseSpectrogram::GenerateSynthesisWindow(hann_window, kStep);

  std::vector<double> expected_inverse(hann_window);
  for (double& val : expected_inverse) {
    val /= 1.5;
  }

  EXPECT_THAT(actual_inverse, Pointwise(DoubleNear(1e-5), expected_inverse));
}

TEST(InverseSpectrogramTest, CorrectOutputWithWindow) {
  // Test the output value for an inverse spectrogram that uses a specific
  // synthesis window, which is not the default rectangle window.
  const int kDataVectorLength = 257;
  const int kNumberOfFramesInTestData = 178;
  const int kStep = 256;
  const int kFrameLength = 512;
  const int kNumberOfOutputSamples = 45824;

  std::vector<std::vector<complex<double>>> input;
  ReadCSVFileToComplexVectorOrDie(
      JoinPath(TestSrcDir(), kInputSpectrogramWithWindowFileName),
      &input);
  EXPECT_THAT(input, SizeIs(kNumberOfFramesInTestData));
  EXPECT_THAT(input[0], SizeIs(kDataVectorLength));

  // Initialize the InverseSpectrogram with a window.
  std::vector<double> hann_window;
  audio_dsp::HannWindow().GetPeriodicSamples(kFrameLength, &hann_window);
  std::vector<double> inverse_window =
      InverseSpectrogram::GenerateSynthesisWindow(hann_window, kStep);
  InverseSpectrogram inverse_sgram;
  inverse_sgram.Initialize(inverse_window, kStep);
  std::vector<double> output;

  inverse_sgram.Process(input, &output);

  std::vector<double> expected_output;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(), kExpectedWithWindowFilename),
      &expected_output));
  EXPECT_THAT(expected_output, SizeIs(kNumberOfOutputSamples));

  // Last frame could not be reconstructed without more samples or an explicit
  // flush.
  EXPECT_THAT(output, SizeIs(expected_output.size() - kStep));

  std::vector<double> tail;
  inverse_sgram.Flush(&tail);
  output.insert(output.end(), tail.begin(), tail.end());
  EXPECT_THAT(output, SizeIs(expected_output.size()));
  EXPECT_THAT(output, Pointwise(DoubleNear(1e-4), expected_output));
}

TEST(InverseSpectrogramTest, PerfectReconstruction) {
  // Test that actually test that a round trip of waveform -> spectrogram ->
  // waveform, with the Spectrogram and InverseSpectrogram classes, outputs the
  // original waveform.
  // Note we use sizes that aren't powers of two - so it is an actual "new"
  // scenario.
  const int kStep = 200;
  const int kFrameLength = 400;
  std::vector<double> hann_window;
  audio_dsp::HannWindow().GetPeriodicSamples(kFrameLength, &hann_window);
  Spectrogram spectrogram_calculator;
  spectrogram_calculator.Initialize(hann_window, kStep);

  std::vector<double> signal;
  ABSL_CHECK(ReadWaveFileToVector(
      JoinPath(TestSrcDir(),
                     kExpectedPerfectReconstructionFileName),
      &signal));
  std::vector<std::vector<complex<double>>> spectrogram;
  spectrogram_calculator.ComputeComplexSpectrogram(signal, &spectrogram);

  std::vector<double> inverse_window =
      InverseSpectrogram::GenerateSynthesisWindow(hann_window, kStep);
  InverseSpectrogram inverse_spectrogram_calculator;
  inverse_spectrogram_calculator.Initialize(inverse_window, kStep);
  std::vector<double> reconstructed;
  inverse_spectrogram_calculator.Process(spectrogram, &reconstructed);
  // Truncate signal to the size of inverse stft. and ignore the frame_length
  // samples at either edge.
  signal.resize(reconstructed.size() - kFrameLength);
  signal.erase(signal.begin(), signal.begin() + kFrameLength);
  reconstructed.erase(reconstructed.begin(),
                      reconstructed.begin() + kFrameLength);
  reconstructed.resize(signal.size());

  EXPECT_THAT(reconstructed, Pointwise(DoubleNear(1e-5), signal));
}

}  // namespace audio_dsp
