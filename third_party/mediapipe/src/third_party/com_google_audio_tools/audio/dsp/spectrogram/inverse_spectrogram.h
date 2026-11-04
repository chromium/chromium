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

// A simple inverse spectrogram which can be used in conjunction with
// the class Spectrogram.

// Initialize() should be called before calls to other functions.
// Once Initialize() has been called and returned true, Process() can
// be called repeatedly with sequential input data (ie. the first
// element of the next input vector directly follows the last element
// of the previous input vector). The output vector is populated with
// as many samples as it is possible to reconstruct from the input
// spectrogram slices. This class is thread-unsafe, and should only be
// called from one thread at a time.

#ifndef AUDIO_DSP_SPECTROGRAM_INVERSE_SPECTROGRAM_H_
#define AUDIO_DSP_SPECTROGRAM_INVERSE_SPECTROGRAM_H_

#include <complex>
#include <memory>
#include <vector>

#include "third_party/eigen3/Eigen/Core"
#include "third_party/pffft/src/pffft.h"

namespace audio_dsp {

namespace internal {

// FFT implementations for InverseSpectrogram.
template <typename Scalar>
struct InverseSpectrogramFftImpl;

template <>
struct InverseSpectrogramFftImpl<float> {
  explicit InverseSpectrogramFftImpl(int fft_length);
  ~InverseSpectrogramFftImpl();

  // Template to support different input complex types.
  template <typename Slice>
  void Process(const Slice& slice, int fft_length, int frame_size, int overlap,
               bool with_window_function, const Eigen::ArrayXf& window_function,
               std::vector<float>& working_output);

  PFFFT_Setup* setup;
  Eigen::ArrayXf fft_input_output;
  // Aligned buffers for pffft.
  float* pffft_in;
  float* pffft_out;
};

template <>
struct InverseSpectrogramFftImpl<double> {
  explicit InverseSpectrogramFftImpl(int fft_length);

  template <typename Slice>
  void Process(const Slice& slice, int fft_length, int frame_size, int overlap,
               bool with_window_function, const Eigen::ArrayXd& window_function,
               std::vector<double>& working_output);

  Eigen::ArrayXd fft_input_output;
  std::vector<int> integer_working_area;
  std::vector<double> fft_double_working_area;
};

}  // namespace internal

template <typename Scalar = double>
class InverseSpectrogramT {
 public:
  // Computes a window that can be used in an InverseSpectrogram.
  // Constructs a window that is equal to the forward window with a further
  // pointwise amplitude correction.
  // Args:
  //  - analysis_window: should be the same window used in the forward transform
  //  (Spectrogram).
  //  - step_length: should be the same step length used for the forward
  //  transform (Spectrogram).
  // Returns a window suitable for reconstruction original waveform, assuming it
  // was supplied to this class's Initialize method.
  // Should be similar to TensorFlow's tf.signal.inverse_stft_window_fn.
  static std::vector<Scalar> GenerateSynthesisWindow(
      const std::vector<Scalar>& analysis_window, int step_length);

  InverseSpectrogramT()
      : initialized_(false), at_least_one_frame_processed_(false) {}
  ~InverseSpectrogramT() = default;

  // Initializes the class to expect input spectrogram data generated
  // with a given FFT length and step length (both in samples).
  // Returns true on success, after which calls to Process() are
  // possible.
  // Note: InverseSpectrogramT<float> requires a transform size of at least 32
  // (a limitation due to pffft). InverseSpectrogramT<double> supports any
  // power-of-two size.
  bool Initialize(int fft_length, int step_length);

  // Initialize with an explicit synthesis window function instead of a
  // rectangular window. The FFT length is inferred as the next
  // power of 2 of the window's length - i.e. the input spectrograms should be
  // the output of a FFT with that FFT-length and step-length (both in samples).
  // For perfect reconstruction the synthesis window should be the normalized
  // analysis window which was used to create the spectrogram. It can be created
  // with the GenerateSynthesisWindow static method above.
  // Note: InverseSpectrogramT<float> requires a transform size of at least 32
  // (a limitation due to pffft). InverseSpectrogramT<double> supports any
  // power-of-two size.
  bool Initialize(const std::vector<Scalar>& window, int step_length);

  // Processes an arbitrary number of spectrogram frames from input,
  // placing the results in output. Process() may be called repeatedly
  // with new input frames each time. output will be cleared on each
  // call to Process() before being populated with new data.
  // The template is instantiated for the 4 combinations of float and
  // double input and output samples.  It always goes from complex spectrum
  // input to real waveform output.
  //
  // InputContainer must be one of
  //   - vector<vector<complex<float>>>,
  //   - vector<vector<complex<double>>>,
  //   - Eigen::MatrixXcf.
  // OutputSample must be either float or double.
  template <class InputContainer, class OutputSample>
  bool Process(const InputContainer& input, std::vector<OutputSample>* output);

  // After the end of a sequence of frames has been processed by
  // Process(), any remaining audio data buffered internally by the
  // class may be obtained by calling Flush(). After a call to
  // Flush(), the internal buffers are reset and Process() may be
  // called again as if Initialize() had just been called.
  // With NULL arg, just re-initializes.
  template <class OutputSample>
  bool Flush(std::vector<OutputSample>* output);

 private:
  // Initializes the class assuming that the fft_length_, frame_size_ and
  // step_length_ have been initialized and are valid values.
  bool InternalInitialize();

  int fft_length_;
  int frame_size_;   // In samples.
  int step_length_;  // In samples.
  int overlap_;      // In samples.
  bool initialized_;
  bool at_least_one_frame_processed_;

  bool with_window_function_;
  // Used as a synthesis window (after the inverse-FFT). For weighted overlap
  // add (WOLA).
  typedef Eigen::Array<Scalar, Eigen::Dynamic, 1> ArrayX;
  ArrayX window_function_;
  // A queue to do waveform overlap-add into.
  std::vector<Scalar> working_output_;

  std::unique_ptr<internal::InverseSpectrogramFftImpl<Scalar>> fft_impl_;

  InverseSpectrogramT(const InverseSpectrogramT&) = delete;
  InverseSpectrogramT& operator=(const InverseSpectrogramT&) = delete;
};

// For backward compatibility and convenience.
using InverseSpectrogram = InverseSpectrogramT<double>;
using InverseSpectrogramFloat = InverseSpectrogramT<float>;

}  // namespace audio_dsp

#endif  // AUDIO_DSP_SPECTROGRAM_INVERSE_SPECTROGRAM_H_
