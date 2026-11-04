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

// Utilities for reading test data files (WAV and CSV) in spectrogram tests.

#ifndef AUDIO_DSP_SPECTROGRAM_TEST_FILE_UTILS_H_
#define AUDIO_DSP_SPECTROGRAM_TEST_FILE_UTILS_H_

#include <cstdint>
#include <complex>
#include <string>
#include <vector>

#include "audio/dsp/portable/read_wav_file.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace audio_dsp {

// Reads a wav format file into a vector of floating-point values with range
// -1.0 to 1.0.
template <typename T>
bool ReadWaveFileToVector(const std::string& file_name, std::vector<T>* data) {
  size_t num_samples;
  int num_channels;
  int sample_rate;
  int16_t* samples = Read16BitWavFile(file_name.c_str(), &num_samples,
                                       &num_channels, &sample_rate);
  if (samples == nullptr || num_channels != 1) {
    free(samples);
    return false;
  }
  data->resize(num_samples);
  for (size_t i = 0; i < num_samples; ++i) {
    (*data)[i] = static_cast<T>(samples[i]) / static_cast<T>(32768);
  }
  free(samples);
  return true;
}

// Reads a binary file containing 64-bit floating point values in the
// form [real_1, imag_1, real_2, imag_2, ...] into a rectangular array
// of complex values where row_length is the length of each inner vector.
bool ReadRawDoubleFileToComplexVector(
    const std::string& file_name, int row_length,
    std::vector<std::vector<std::complex<double> > >* data);

// Reads a CSV file of numbers in the format 1.1+2.2i,1.1,2.2i,3.3j into data.
void ReadCSVFileToComplexVectorOrDie(
    const std::string& file_name,
    std::vector<std::vector<std::complex<double> > >* data);

// Reads a 2D array of floats from an ASCII text file, where each line is a row
// of the array, and elements are separated by commas.
void ReadCSVFileToArrayOrDie(const std::string& filename,
                             std::vector<std::vector<float> >* array);

// Write a binary file containing 64-bit floating-point values for
// reading by, for example, MATLAB.
bool WriteDoubleVectorToFile(const std::string& file_name,
                             absl::Span<const double> data);

// Write a binary file containing 32-bit floating-point values for
// reading by, for example, MATLAB.
bool WriteFloatVectorToFile(const std::string& file_name,
                            absl::Span<const float> data);

// Write a binary file containing 64-bit floating-point values for
// reading by, for example, MATLAB.
bool WriteDoubleArrayToFile(const std::string& file_name, int size,
                            const double* data);

// Write a binary file containing 32-bit floating-point values for
// reading by, for example, MATLAB.
bool WriteFloatArrayToFile(const std::string& file_name, int size,
                           const float* data);

// Write a binary file in the format read by
// ReadRawDoubleFileToComplexVector above.
bool WriteComplexVectorToRawDoubleFile(
    const std::string& file_name,
    absl::Span<const std::vector<std::complex<double>>> data);

// Generate a sine wave with the provided parameters, and populate
// data with the samples.
void SineWave(int sample_rate,
              float frequency,
              float duration_seconds,
              std::vector<double>* data);

// Join two path components.
std::string JoinPath(const std::string& a, const std::string& b);

// Get the test source directory (Bazel's TEST_SRCDIR + TEST_WORKSPACE).
std::string TestSrcDir();

}  // namespace audio_dsp

#endif  // AUDIO_DSP_SPECTROGRAM_TEST_FILE_UTILS_H_
