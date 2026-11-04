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

#include "audio/dsp/spectrogram/spectrogram.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include "audio/dsp/number_util.h"
#include "audio/dsp/window_functions.h"
#include "glog/logging.h"
#include "third_party/eigen3/Eigen/Core"
#include "third_party/fft2d/fft.h"
#include "third_party/pffft/src/pffft.h"



namespace audio_dsp {

namespace internal {

SpectrogramFftImpl<float>::SpectrogramFftImpl(int fft_length) {
  setup = pffft_new_setup(fft_length, PFFFT_REAL);
  ABSL_CHECK(setup) << "pffft_new_setup failed for fft_length " << fft_length;
  pffft_in =
      static_cast<float*>(pffft_aligned_malloc(fft_length * sizeof(float)));
  pffft_out =
      static_cast<float*>(pffft_aligned_malloc(fft_length * sizeof(float)));
  fft_input_output.resize(fft_length + 2);
  real_input.assign(fft_length, 0.0f);
}

SpectrogramFftImpl<float>::~SpectrogramFftImpl() {
  pffft_destroy_setup(setup);
  pffft_aligned_free(pffft_in);
  pffft_aligned_free(pffft_out);
}

void SpectrogramFftImpl<float>::Reset() {}

void SpectrogramFftImpl<float>::Process(int fft_length, int window_length,
                                        int output_frequency_channels,
                                        const float* input_queue,
                                        const Eigen::ArrayXf& window) {
  Eigen::Map<Eigen::ArrayXf>(pffft_in, window_length) =
      Eigen::Map<const Eigen::ArrayXf>(input_queue, window_length) * window;
  if (fft_length > window_length) {
    std::fill(pffft_in + window_length, pffft_in + fft_length, 0.0f);
  }

  pffft_transform_ordered(setup, pffft_in, pffft_out, nullptr, PFFFT_FORWARD);

  // pffft_transform_ordered with PFFFT_REAL and PFFFT_FORWARD returns N
  // floats in the format [R0, Rn/2, R1, I1, R2, I2, ..., Rn/2-1, In/2-1]. We
  // need to reformat this into [R0, 0, R1, -I1, R2, -I2, ..., Rn/2, 0] to
  // match the original behavior (which included a conjugation).
  fft_input_output[0] = pffft_out[0];
  fft_input_output[1] = 0.0f;
  for (int i = 1; i < fft_length / 2; ++i) {
    fft_input_output[2 * i] = pffft_out[2 * i];
    fft_input_output[2 * i + 1] = -pffft_out[2 * i + 1];
  }
  fft_input_output[fft_length] = pffft_out[1];
  fft_input_output[fft_length + 1] = 0.0f;
}

SpectrogramFftImpl<double>::SpectrogramFftImpl(int fft_length) {
  fft_input_output.resize(fft_length + 2);
  int half_fft_length = fft_length / 2;
  double_working_area.assign(half_fft_length, 0.0);
  integer_working_area.assign(2 + static_cast<int>(std::sqrt(half_fft_length)),
                              0);
  integer_working_area[0] = 0;
}

void SpectrogramFftImpl<double>::Reset() { integer_working_area[0] = 0; }

void SpectrogramFftImpl<double>::Process(int fft_length, int window_length,
                                         int output_frequency_channels,
                                         const double* input_queue,
                                         const Eigen::ArrayXd& window) {
  fft_input_output.head(window_length) =
      Eigen::Map<const Eigen::ArrayXd>(input_queue, window_length) * window;
  fft_input_output.segment(window_length, fft_length - window_length).setZero();

  // 1 means forward; -1 reverse.
  constexpr int kForwardFFT = 1;
  // This real FFT is a fair amount faster than using cdft here.
  rdft(fft_length, kForwardFFT, fft_input_output.data(),
       integer_working_area.data(), double_working_area.data());

  // Make rdft result look like cdft result;
  // unpack the last real value from the first position's imag slot.
  fft_input_output[fft_length] = fft_input_output[1];
  fft_input_output[fft_length + 1] = 0;
  fft_input_output[1] = 0;
}

}  // namespace internal

using std::complex;

template <typename Scalar>
bool SpectrogramT<Scalar>::ResetSampleBuffer() {
  fft_impl_->Reset();
  input_queue_.clear();
  samples_to_next_step_ = window_length_;
  return true;
}

template <typename Scalar>
bool SpectrogramT<Scalar>::Initialize(int window_length, int step_length,
                                      std::optional<int> fft_length) {
  std::vector<Scalar> window;
  HannWindow().GetPeriodicSamples(window_length, &window);
  return Initialize(window, step_length, fft_length);
}

template <typename Scalar>
bool SpectrogramT<Scalar>::Initialize(const std::vector<Scalar>& window,
                                      int step_length,
                                      std::optional<int> fft_length) {
  window_length_ = window.size();
  window_ = Eigen::Map<const ArrayX>(window.data(), window.size());
  if (window_length_ < 2) {
    LOG(ERROR) << "Window length too short.";
    initialized_ = false;
    return false;
  }

  step_length_ = step_length;
  if (step_length_ < 1) {
    LOG(ERROR) << "Step length must be positive.";
    initialized_ = false;
    return false;
  }

  if (fft_length.has_value() && !IsPowerOfTwoOrZero(fft_length.value())) {
    LOG(ERROR) << "FFT length must be a power of two.";
    initialized_ = false;
    return false;
  }
  fft_length_ = fft_length.value_or(NextPowerOfTwo(window_length_));
  ABSL_CHECK_GE(fft_length_, window_length_);

  if (std::is_same<Scalar, float>::value && fft_length_ < 32) {
    LOG(ERROR) << "FFT length must be at least 32 for float implementation.";
    initialized_ = false;
    return false;
  }

  output_frequency_channels_ = 1 + fft_length_ / 2;

  fft_impl_ =
      std::make_unique<internal::SpectrogramFftImpl<Scalar>>(fft_length_);

  ResetSampleBuffer();
  initialized_ = true;
  return true;
}

template <typename Scalar>
template <class InputSample, class OutputSample>
bool SpectrogramT<Scalar>::ComputeComplexSpectrogram(
    const std::vector<InputSample>& input,
    std::vector<std::vector<std::complex<OutputSample>>>* output) {
  if (!initialized_) {
    LOG(ERROR) << "ComputeComplexSpectrogram() called before successful call "
               << "to Initialize().";
    return false;
  }
  ABSL_CHECK(output);
  output->clear();
  // Calculate how many frames will be produced.
  const int num_frames =
      (input.size() < samples_to_next_step_)
          ? 0
          : 1 + (input.size() - samples_to_next_step_) / step_length_;
  output->reserve(num_frames);
  int input_start = 0;

  while (GetNextWindowOfSamples(input, &input_start)) {
    ABSL_DCHECK_EQ(input_queue_.size(), window_length_);
    fft_impl_->Process(fft_length_, window_length_, output_frequency_channels_,
                       input_queue_.data(), window_);
    const std::complex<Scalar>* out =
        reinterpret_cast<const std::complex<Scalar>*>(
            fft_impl_->fft_input_output.data());
    output->emplace_back(out, out + output_frequency_channels_);
  }

  ABSL_DCHECK_EQ(output->size(), num_frames);
  return true;
}

template <typename Scalar>
template <class InputSample, class OutputSample>
bool SpectrogramT<Scalar>::ComputeSquaredMagnitudeSpectrogram(
    const std::vector<InputSample>& input,
    std::vector<std::vector<OutputSample>>* output) {
  if (!initialized_) {
    LOG(ERROR) << "ComputeSquaredMagnitudeSpectrogram() called before "
               << "successful call to Initialize().";
    return false;
  }
  ABSL_CHECK(output);
  output->clear();
  // Calculate how many frames will be produced.
  const int num_frames =
      (input.size() < samples_to_next_step_)
          ? 0
          : 1 + (input.size() - samples_to_next_step_) / step_length_;
  output->reserve(num_frames);
  int input_start = 0;

  while (GetNextWindowOfSamples(input, &input_start)) {
    ABSL_DCHECK_EQ(input_queue_.size(), window_length_);
    fft_impl_->Process(fft_length_, window_length_, output_frequency_channels_,
                       input_queue_.data(), window_);
    auto& spectrogram_slice = output->emplace_back(output_frequency_channels_);
    for (int i = 0; i < output_frequency_channels_; ++i) {
      const Scalar re = fft_impl_->fft_input_output[2 * i];
      const Scalar im = fft_impl_->fft_input_output[2 * i + 1];
      spectrogram_slice[i] = re * re + im * im;
    }
  }

  ABSL_DCHECK_EQ(output->size(), num_frames);
  return true;
}

// Return true if a full window of samples is prepared; manage the queue.
template <typename Scalar>
template <class InputSample>
bool SpectrogramT<Scalar>::GetNextWindowOfSamples(
    const std::vector<InputSample>& input, int* input_start) {
  auto input_it = input.begin() + *input_start;
  int input_remaining = input.end() - input_it;
  if (samples_to_next_step_ > input_remaining) {
    // Copy in as many samples are left and return false, no full window.
    input_queue_.insert(input_queue_.end(), input_it, input.end());
    *input_start += input_remaining;  // Increases it to input.size().
    samples_to_next_step_ -= input_remaining;
    return false;  // Not enough for a full window.
  } else {
    // Copy just enough into queue to make a new window.
    if (samples_to_next_step_ < window_length_) {
      input_queue_.erase(input_queue_.begin(),
                         input_queue_.begin() + input_queue_.size() -
                             (window_length_ - samples_to_next_step_));
      input_queue_.insert(input_queue_.end(), input_it,
                          input_it + samples_to_next_step_);
    } else {
      input_queue_.assign(input_it + samples_to_next_step_ - window_length_,
                          input_it + samples_to_next_step_);
    }
    *input_start += samples_to_next_step_;
    ABSL_DCHECK_EQ(window_length_, input_queue_.size());
    samples_to_next_step_ = step_length_;  // Be ready for next time.
    return true;  // Yes, input_queue_ now contains exactly a window-full.
  }
}

template class SpectrogramT<float>;
template class SpectrogramT<double>;

#define INSTANTIATE_SPECTROGRAM_METHODS_IO(Scalar, Input, Output)         \
  template bool SpectrogramT<Scalar>::ComputeComplexSpectrogram(          \
      const std::vector<Input>&,                                          \
      std::vector<std::vector<std::complex<Output>>>*);                   \
  template bool SpectrogramT<Scalar>::ComputeSquaredMagnitudeSpectrogram( \
      const std::vector<Input>&, std::vector<std::vector<Output>>*);

#define INSTANTIATE_SPECTROGRAM_METHODS(Scalar)             \
  INSTANTIATE_SPECTROGRAM_METHODS_IO(Scalar, float, float)  \
  INSTANTIATE_SPECTROGRAM_METHODS_IO(Scalar, float, double) \
  INSTANTIATE_SPECTROGRAM_METHODS_IO(Scalar, double, float) \
  INSTANTIATE_SPECTROGRAM_METHODS_IO(Scalar, double, double)

INSTANTIATE_SPECTROGRAM_METHODS(float)
INSTANTIATE_SPECTROGRAM_METHODS(double)

}  // namespace audio_dsp
