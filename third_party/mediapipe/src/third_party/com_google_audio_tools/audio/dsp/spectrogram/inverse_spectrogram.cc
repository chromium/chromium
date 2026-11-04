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

#include "audio/dsp/spectrogram/inverse_spectrogram.h"

#include <cmath>
#include <complex>
#include <memory>
#include <type_traits>
#include <vector>

#include "audio/dsp/number_util.h"
#include "glog/logging.h"
#include "third_party/eigen3/Eigen/Core"
#include "third_party/fft2d/fft.h"
#include "third_party/pffft/src/pffft.h"



namespace audio_dsp {

namespace internal {
InverseSpectrogramFftImpl<float>::InverseSpectrogramFftImpl(int fft_length) {
  setup = pffft_new_setup(fft_length, PFFFT_REAL);
  ABSL_CHECK(setup) << "pffft_new_setup failed for fft_length " << fft_length;
  pffft_in =
      static_cast<float*>(pffft_aligned_malloc(fft_length * sizeof(float)));
  pffft_out =
      static_cast<float*>(pffft_aligned_malloc(fft_length * sizeof(float)));
  fft_input_output.resize(fft_length);
}

InverseSpectrogramFftImpl<float>::~InverseSpectrogramFftImpl() {
  pffft_destroy_setup(setup);
  pffft_aligned_free(pffft_in);
  pffft_aligned_free(pffft_out);
}

template <typename Slice>
void InverseSpectrogramFftImpl<float>::Process(
    const Slice& slice, int fft_length, int frame_size, int overlap,
    bool with_window_function, const Eigen::ArrayXf& window_function,
    std::vector<float>& working_output) {
  // pffft_transform_ordered with PFFFT_REAL and PFFFT_BACKWARD expects N
  // floats in the format [R0, Rn/2, R1, I1, R2, I2, ..., Rn/2-1, In/2-1].
  // The input slice is in the format [R0, I0, R1, I1, ..., Rn/2, In/2],
  // where I0 and In/2 are 0.
  // We also need to conjugate the input to match the original behavior.
  pffft_in[0] = static_cast<std::complex<float>>(slice[0]).real();
  pffft_in[1] = static_cast<std::complex<float>>(slice[fft_length / 2]).real();
  for (int i = 1; i < fft_length / 2; ++i) {
    std::complex<float> c =
        std::conj(static_cast<std::complex<float>>(slice[i]));
    pffft_in[2 * i] = c.real();
    pffft_in[2 * i + 1] = c.imag();
  }

  pffft_transform_ordered(setup, pffft_in, pffft_out, nullptr, PFFFT_BACKWARD);

  for (int i = 0; i < fft_length; ++i) {
    fft_input_output[i] = pffft_out[i];
  }

  if (with_window_function) {
    fft_input_output.head(frame_size) *= window_function;
    fft_input_output.head(frame_size) *= 0.5f;
  } else {
    const float full_fft_length = static_cast<float>(fft_length);
    fft_input_output.head(frame_size) /= full_fft_length;
  }

  Eigen::Map<Eigen::ArrayXf>(working_output.data(), working_output.size()) +=
      fft_input_output.head(overlap);
  auto new_portion = fft_input_output.segment(overlap, frame_size - overlap);
  working_output.insert(working_output.end(), new_portion.begin(),
                        new_portion.end());
}

InverseSpectrogramFftImpl<double>::InverseSpectrogramFftImpl(int fft_length) {
  fft_input_output.resize(fft_length);
  int half_fft_length = fft_length / 2;
  fft_double_working_area.assign(half_fft_length, 0.0);
  integer_working_area.assign(2 + static_cast<int>(sqrt(half_fft_length)), 0);
  // Set flag element to ensure that the working areas are initialized
  // on the first call to cdft (redundant given the assign above, but leaving
  // it here as a reminder.
  integer_working_area[0] = 0;
}

template <typename Slice>
void InverseSpectrogramFftImpl<double>::Process(
    const Slice& slice, int fft_length, int frame_size, int overlap,
    bool with_window_function, const Eigen::ArrayXd& window_function,
    std::vector<double>& working_output) {
  using InputRealSample =
      typename std::remove_reference_t<decltype(slice[0])>::value_type;
  fft_input_output[0] = std::real(slice[0]);
  fft_input_output[1] = std::real(slice[fft_length / 2]);
  fft_input_output.tail(fft_length - 2) =
      Eigen::Map<const Eigen::Array<InputRealSample, Eigen::Dynamic, 1>>(
          reinterpret_cast<const InputRealSample*>(slice.data() + 1),
          fft_length - 2)
          .template cast<double>();

  constexpr int kReverseFFT = -1;
  rdft(fft_length, kReverseFFT, fft_input_output.data(),
       integer_working_area.data(), fft_double_working_area.data());

  if (with_window_function) {
    fft_input_output.head(frame_size) *= window_function;
  } else {
    const int half_fft_length = fft_length / 2;
    fft_input_output.head(frame_size) /= half_fft_length;
  }

  Eigen::Map<Eigen::ArrayXd>(working_output.data(), working_output.size()) +=
      fft_input_output.head(overlap);
  auto new_portion = fft_input_output.segment(overlap, frame_size - overlap);
  working_output.insert(working_output.end(), new_portion.begin(),
                        new_portion.end());
}

}  // namespace internal

using std::complex;

template <typename Scalar>
std::vector<Scalar> InverseSpectrogramT<Scalar>::GenerateSynthesisWindow(
    const std::vector<Scalar>& analysis_window, int step_length) {
  const int frame_length = analysis_window.size();
  // Perfect reconstruction is not possible if the stft skips samples.
  ABSL_DCHECK_LE(step_length, frame_length);

  // The result is just the original window, with a normalization so the sum of
  // each element with its overlapped elements would be one.
  std::vector<Scalar> result(analysis_window);
  for (int i = 0; i < step_length; ++i) {
    // Calculate the normalization factor:
    Scalar denom = 0.0;
    for (int offset = i; offset < analysis_window.size();
         offset += step_length) {
      Scalar window_element = analysis_window[offset];
      denom += window_element * window_element;
    }

    // Apply the normalization:
    ABSL_DCHECK_NE(denom, static_cast<Scalar>(0.0));
    for (int offset = i; offset < analysis_window.size();
         offset += step_length) {
      result[offset] /= denom;
    }
  }
  return result;
}

template <typename Scalar>
bool InverseSpectrogramT<Scalar>::InternalInitialize() {
  fft_impl_ = std::make_unique<internal::InverseSpectrogramFftImpl<Scalar>>(
      fft_length_);

  overlap_ = frame_size_ - step_length_;
  if (overlap_ < 0) {
    overlap_ = 0;
  }
  working_output_.assign(overlap_, 0.0);
  working_output_.reserve(frame_size_);

  at_least_one_frame_processed_ = false;
  initialized_ = true;

  return true;
}

template <typename Scalar>
bool InverseSpectrogramT<Scalar>::Initialize(int fft_length, int step_length) {
  initialized_ = false;
  if (fft_length < 2) {
    LOG(ERROR) << "FFT length too short.";
    return false;
  }
  if (!IsPowerOfTwoOrZero(fft_length)) {
    LOG(ERROR) << "FFT length not a power of 2.";
    return false;
  }
  fft_length_ = fft_length;
  frame_size_ = fft_length;

  if (std::is_same<Scalar, float>::value && fft_length_ < 32) {
    LOG(ERROR) << "FFT length must be at least 32 for float implementation.";
    initialized_ = false;
    return false;
  }

  if (step_length < 1) {
    LOG(ERROR) << "Step length must be positive.";
    return false;
  }
  step_length_ = step_length;

  with_window_function_ = false;

  return InternalInitialize();
}

template <typename Scalar>
bool InverseSpectrogramT<Scalar>::Initialize(const std::vector<Scalar>& window,
                                             int step_length) {
  initialized_ = false;
  if (window.size() < 2) {
    LOG(ERROR) << "Window length too short.";
    return false;
  }
  frame_size_ = window.size();
  with_window_function_ = true;
  fft_length_ = NextPowerOfTwo(window.size());

  if (std::is_same<Scalar, float>::value && fft_length_ < 32) {
    LOG(ERROR) << "FFT length must be at least 32 for float implementation.";
    initialized_ = false;
    return false;
  }

  const int half_fft_length = fft_length_ / 2;
  // Copy window.
  window_function_ = Eigen::Map<const ArrayX>(window.data(), window.size()) /
                     static_cast<Scalar>(half_fft_length);

  if (step_length < 1) {
    LOG(ERROR) << "Step length must be positive.";
    return false;
  }
  step_length_ = step_length;

  return InternalInitialize();
}

template <typename Container>
struct ContainerTraits {};

// Traits for std::vector<vector<complex<RealScalar>>>.
template <typename RealScalar_>
struct ContainerTraits<std::vector<std::vector<std::complex<RealScalar_>>>> {
  using RealScalar = RealScalar_;

  static const auto& AsIterable(
      const std::vector<std::vector<std::complex<RealScalar>>>& input) {
    return input;
  }
};

// Traits for Eigen::MatrixXcf and Eigen::MatrixXcd.
template <typename RealScalar_>
struct ContainerTraits<
    Eigen::Matrix<std::complex<RealScalar_>, Eigen::Dynamic, Eigen::Dynamic>> {
  using RealScalar = RealScalar_;

  static auto AsIterable(
      const Eigen::Matrix<std::complex<RealScalar_>, Eigen::Dynamic,
                          Eigen::Dynamic>& input) -> decltype(input.colwise()) {
    return input.colwise();
  }
};

template <typename Scalar>
template <class InputContainer, class OutputSample>
bool InverseSpectrogramT<Scalar>::Process(const InputContainer& input,
                                          std::vector<OutputSample>* output) {
  if (!initialized_) {
    LOG(ERROR) << "Process() called before successful call to Initialize().";
    return false;
  }
  ABSL_CHECK(output);
  output->clear();
  // Note that input_iterable.end() - input_iterable.begin() != input.size()
  // in general, e.g. for Eigen::MatrixXcf. Also, const auto& is safe here even
  // for Eigen::MatrixXcf because the lifetime of Eigen::MatrixXcf::colwise() is
  // extended by C++ lifetime extension.
  const auto& input_iterable =
      ContainerTraits<InputContainer>::AsIterable(input);
  const int output_size = (input_iterable.end() - input_iterable.begin()) *
                          (frame_size_ - overlap_);
  output->reserve(output_size);

  for (const auto& slice : input_iterable) {
    // Check that slice is the right size for the complex-to-real inverse FFT.
    ABSL_DCHECK_EQ(slice.size(), fft_length_ / 2 + 1);

    fft_impl_->Process(slice, fft_length_, frame_size_, overlap_,
                       with_window_function_, window_function_,
                       working_output_);

    int num_output_samples = working_output_.size() - overlap_;
    // Copy the done samples, converting to float if necessary.
    output->insert(output->end(), working_output_.begin(),
                   working_output_.begin() + num_output_samples);
    // And remove them from working_output_.
    working_output_.erase(working_output_.begin(),
                          working_output_.begin() + num_output_samples);
    at_least_one_frame_processed_ = true;
  }
  return true;
}

template <typename Scalar>
template <class OutputSample>
bool InverseSpectrogramT<Scalar>::Flush(std::vector<OutputSample>* output) {
  if (output) {
    if (at_least_one_frame_processed_) {
      output->assign(working_output_.begin(), working_output_.end());
    } else {
      output->clear();
    }
  }
  if (!initialized_) {
    LOG(ERROR) << "Flush() called before successful call to Initialize().";
    return false;
  }
  // Reset to original state so that Process() may be called again.
  Initialize(fft_length_, step_length_);
  return true;
}

template class InverseSpectrogramT<float>;
template class InverseSpectrogramT<double>;

#define INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_INTERNAL(Scalar, Slice) \
  template void internal::InverseSpectrogramFftImpl<Scalar>::Process(   \
      const Slice&, int, int, int, bool,                                \
      const Eigen::Array<Scalar, Eigen::Dynamic, 1>&, std::vector<Scalar>&);

#define INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_IO(Scalar, Input, Output) \
  template bool InverseSpectrogramT<Scalar>::Process(                     \
      const std::vector<std::vector<std::complex<Input>>>&,               \
      std::vector<Output>*);

#define INSTANTIATE_INVERSE_SPECTROGRAM_METHODS(Scalar)                       \
  INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_IO(Scalar, float, float)            \
  INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_IO(Scalar, float, double)           \
  INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_IO(Scalar, double, float)           \
  INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_IO(Scalar, double, double)          \
  template bool InverseSpectrogramT<Scalar>::Process(const Eigen::MatrixXcf&, \
                                                     std::vector<float>*);    \
  template bool InverseSpectrogramT<Scalar>::Process(const Eigen::MatrixXcf&, \
                                                     std::vector<double>*);   \
  template bool InverseSpectrogramT<Scalar>::Process(const Eigen::MatrixXcd&, \
                                                     std::vector<float>*);    \
  template bool InverseSpectrogramT<Scalar>::Process(const Eigen::MatrixXcd&, \
                                                     std::vector<double>*);   \
  INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_INTERNAL(                           \
      Scalar, std::vector<std::complex<float>>)                               \
  INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_INTERNAL(                           \
      Scalar, std::vector<std::complex<double>>)                              \
  INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_INTERNAL(                           \
      Scalar, Eigen::MatrixXcf::ConstColXpr)                                  \
  INSTANTIATE_INVERSE_SPECTROGRAM_METHODS_INTERNAL(                           \
      Scalar, Eigen::MatrixXcd::ConstColXpr)                                  \
  template bool InverseSpectrogramT<Scalar>::Flush(std::vector<double>*);     \
  template bool InverseSpectrogramT<Scalar>::Flush(std::vector<float>*);

INSTANTIATE_INVERSE_SPECTROGRAM_METHODS(float)
INSTANTIATE_INVERSE_SPECTROGRAM_METHODS(double)

}  // namespace audio_dsp
