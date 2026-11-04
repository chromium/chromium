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

// Class for generating spectrogram slices from a waveform.
// http://goto/waveform-erase Used in conjunction with the class
// InverseSpectrogram.  Initialize() should be called before calls to
// other functions.  Once Initialize() has been called and returned
// true, The Compute*() functions can be called repeatedly with
// sequential input data (ie. the first element of the next input
// vector directly follows the last element of the previous input
// vector). Whenever enough audio samples are buffered to produce a
// new frame, it will be placed in output. Output is cleared on each
// call to Compute*(). This class is thread-unsafe, and should only be
// called from one thread at a time.
// With the default parameters, the output of this class should be very
// close to the results of the following MATLAB code:
// overlap_samples = window_length_samples - step_samples;
// window = hann(window_length_samples, 'periodic');
// S = abs(spectrogram(audio, window, overlap_samples)).^2;

#ifndef AUDIO_DSP_SPECTROGRAM_SPECTROGRAM_H_
#define AUDIO_DSP_SPECTROGRAM_SPECTROGRAM_H_

#include <complex>
#include <memory>
#include <optional>
#include <vector>

#include "third_party/eigen3/Eigen/Core"
#include "third_party/pffft/src/pffft.h"



namespace audio_dsp {

namespace internal {

// FFT implementations for Spectrogram.
template <typename Scalar>
struct SpectrogramFftImpl;

template <>
struct SpectrogramFftImpl<float> {
  explicit SpectrogramFftImpl(int fft_length);
  ~SpectrogramFftImpl();
  void Reset();
  void Process(int fft_length, int window_length, int output_frequency_channels,
               const float* input_queue, const Eigen::ArrayXf& window);

  PFFFT_Setup* setup;
  Eigen::ArrayXf fft_input_output;
  std::vector<float> real_input;
  // Aligned buffers for pffft.
  float* pffft_in;
  float* pffft_out;
};

template <>
struct SpectrogramFftImpl<double> {
  explicit SpectrogramFftImpl(int fft_length);
  ~SpectrogramFftImpl() = default;
  void Reset();
  void Process(int fft_length, int window_length, int output_frequency_channels,
               const double* input_queue, const Eigen::ArrayXd& window);

  Eigen::ArrayXd fft_input_output;
  std::vector<int> integer_working_area;
  std::vector<double> double_working_area;
};

}  // namespace internal

template <typename Scalar = double>
class SpectrogramT {
 public:
  SpectrogramT() : initialized_(false) {}
  ~SpectrogramT() = default;

  // Initializes the class with a given window length and step length
  // (both in samples). Internally a Hann window is used as the window
  // function. Returns true on success, after which calls to Process()
  // are possible. window_length must be greater than 1 and step
  // length must be greater than 0. fft_length defines the fft length which must
  // be greater than window_length and a power of 2.
  // Note: SpectrogramT<float> requires a transform size of at least 32
  // (a limitation due to pffft). SpectrogramT<double> supports any
  // power-of-two size.
  bool Initialize(int window_length, int step_length,
                  std::optional<int> fft_length = std::nullopt);

  // Initialize with an explicit window instead of a length.
  bool Initialize(const std::vector<Scalar>& window, int step_length,
                  std::optional<int> fft_length = std::nullopt);

  // Re-initializes/resets the internal sample buffer to the state before any
  // samples have been passed to the Compute methods.
  bool ResetSampleBuffer();

  // Processes an arbitrary amount of audio data (contained in input)
  // to yield complex spectrogram frames. After a successful call to
  // Initialize(), Process() may be called repeatedly with new input data
  // each time.  The audio input is buffered internally, and the output
  // vector is populated with as many temporally-ordered spectral slices
  // as it is possible to generate from the input.  The output is cleared
  // on each call before the new frames (if any) are added.
  //
  // The template parameters can be float or double.
  template <class InputSample, class OutputSample>
  bool ComputeComplexSpectrogram(
      const std::vector<InputSample>& input,
      std::vector<std::vector<std::complex<OutputSample>>>* output);

  // This function works as the one above, but returns the power
  // (the L2 norm, or the squared magnitude) of each complex value.
  template <class InputSample, class OutputSample>
  bool ComputeSquaredMagnitudeSpectrogram(
      const std::vector<InputSample>& input,
      std::vector<std::vector<OutputSample>>* output);

  // Allow templating of a single function name that emits complex values or
  // magnitude depending on the type of the output pointer.  This allows
  // the caller to support both complex and real output types with templated
  // code.
  template <class InputSample, class OutputSample>
  bool ComputeSpectrogram(
      const std::vector<InputSample>& input,
      std::vector<std::vector<std::complex<OutputSample>>>* output) {
    return ComputeComplexSpectrogram(input, output);
  }
  template <class InputSample, class OutputSample>
  bool ComputeSpectrogram(const std::vector<InputSample>& input,
                          std::vector<std::vector<OutputSample>>* output) {
    return ComputeSquaredMagnitudeSpectrogram(input, output);
  }

  // Return reference to the window function used internally.
  typedef Eigen::Array<Scalar, Eigen::Dynamic, 1> ArrayX;
  const ArrayX& GetWindow() const { return window_; }

  // Return the number of frequency channels in the spectrogram.
  int output_frequency_channels() const { return output_frequency_channels_; }

 private:
  template <class InputSample>
  bool GetNextWindowOfSamples(const std::vector<InputSample>& input,
                              int* input_start);

  int fft_length_;
  int output_frequency_channels_;
  int window_length_;
  int step_length_;
  bool initialized_;
  int samples_to_next_step_;

  ArrayX window_;
  std::vector<Scalar> input_queue_;

  // Working data areas for the FFT routines.
  std::unique_ptr<internal::SpectrogramFftImpl<Scalar>> fft_impl_;

  SpectrogramT(const SpectrogramT&) = delete;
  SpectrogramT& operator=(const SpectrogramT&) = delete;
};

// For backward compatibility and convenience.
using Spectrogram = SpectrogramT<double>;
using SpectrogramFloat = SpectrogramT<float>;

}  // namespace audio_dsp

#endif  // AUDIO_DSP_SPECTROGRAM_SPECTROGRAM_H_
