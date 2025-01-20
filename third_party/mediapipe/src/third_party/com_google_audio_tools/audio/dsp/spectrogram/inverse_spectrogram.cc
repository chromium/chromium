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

#include "audio/dsp/number_util.h"
#include "glog/logging.h"
#include "third_party/fft2d/fft.h"

namespace audio_dsp {

using std::complex;

std::vector<double> InverseSpectrogram::GenerateSynthesisWindow(
    const std::vector<double>& analysis_window, int step_length) {
  const int frame_length = analysis_window.size();
  // Perfect reconsturction is not possible if the stft skips samples.
  ABSL_DCHECK_LE(step_length, frame_length);

  std::vector<double> result(analysis_window);
  // The result is just the original window, with a normalization so the sum of
  // each element with it's overlaped elements would be one.
  for (int i = 0; i < step_length; ++i) {
    // Calculate the normalization factor:
    double denom = 0.0;
    for (int offset = i; offset < analysis_window.size();
         offset += step_length) {
      double window_element = analysis_window[offset];
      denom += window_element * window_element;
    }

    // Apply the normalization:
    ABSL_DCHECK_NE(denom, 0.0);  // Can't reconstruct if the weight is zero.
    for (int offset = i; offset < analysis_window.size();
         offset += step_length) {
      result[offset] /= denom;
    }
  }
  return result;
}

bool InverseSpectrogram::InternalInitialize() {
  fft_input_output_.assign(fft_length_, 0.0);
  int half_fft_length = fft_length_ / 2;
  fft_double_working_area_.assign(half_fft_length, 0.0);
  fft_integer_working_area_.assign(2 + static_cast<int>(sqrt(half_fft_length)),
                                   0);
  // Set flag element to ensure that the working areas are initialized
  // on the first call to cdft (redundant given the assign above, but leaving
  // it here as a reminder.
  fft_integer_working_area_[0] = 0;

  overlap_ = frame_size_ - step_length_;
  if (overlap_ < 0) {
    overlap_ = 0;
  }
  working_output_.assign(overlap_, 0.0);

  at_least_one_frame_processed_ = false;
  initialized_ = true;

  return true;
}

bool InverseSpectrogram::Initialize(int fft_length, int step_length) {
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

  if (step_length < 1) {
    LOG(ERROR) << "Step length must be positive.";
    return false;
  }
  step_length_ = step_length;

  with_window_function_ = false;

  return InternalInitialize();
}

bool InverseSpectrogram::Initialize(const std::vector<double>& window,
                                    int step_length) {
  initialized_ = false;
  if (window.size() < 2) {
    LOG(ERROR) << "Window length too short.";
    return false;
  }
  frame_size_ = window.size();
  window_function_ = window;  // Copy window.
  with_window_function_ = true;
  fft_length_ = NextPowerOfTwo(window.size());

  if (step_length < 1) {
    LOG(ERROR) << "Step length must be positive.";
    return false;
  }
  step_length_ = step_length;

  return InternalInitialize();
}

template <class InputSample, class OutputSample>
bool InverseSpectrogram::Process(
    const std::vector<std::vector<complex<InputSample>>>& input,
    std::vector<OutputSample>* output) {
  if (!initialized_) {
    LOG(ERROR) << "Process() called before successful call to Initialize().";
    return false;
  }
  ABSL_CHECK(output);
  output->clear();
  for (const auto& slice : input) {
    // Check that slice is the right size for the complex-to-real inverse FFT.
    ABSL_DCHECK_EQ(slice.size(), fft_length_ / 2 + 1);

    fft_input_output_[0] = std::real(slice[0]);
    // For rdft, put Nyquist-frequency real value into zero-frequency imag slot.
    fft_input_output_[1] = std::real(slice[fft_length_ / 2]);
    for (int j = 1; j < fft_length_ / 2; ++j) {
      const auto& c = slice[j];
      fft_input_output_[j * 2] = std::real(c);
      fft_input_output_[j * 2 + 1] = std::imag(c);
    }
    ProcessCoreFFT();
    int num_output_samples = working_output_.size() - overlap_;
    // Copy the done samples, converting to float if necessary.
    output->insert(output->end(),
                   working_output_.begin(),
                   working_output_.begin() + num_output_samples);
    // And remove them from the working_output_ deque.
    working_output_.erase(working_output_.begin(),
                          working_output_.begin() + num_output_samples);
    at_least_one_frame_processed_ = true;
  }
  return true;
}
// Instantiate it four ways:
template bool InverseSpectrogram::Process(
    const std::vector<std::vector<std::complex<float>>>&, std::vector<float>*);
template bool InverseSpectrogram::Process(
    const std::vector<std::vector<std::complex<double>>>&, std::vector<float>*);
template bool InverseSpectrogram::Process(
    const std::vector<std::vector<std::complex<float>>>&, std::vector<double>*);
template bool InverseSpectrogram::Process(
    const std::vector<std::vector<std::complex<double>>>&,
    std::vector<double>*);

void InverseSpectrogram::ProcessCoreFFT() {
  int half_fft_length = fft_length_ / 2;
  const int kReverseFFT = -1;  // 1 means forward; -1 reverse.
  rdft(fft_length_,
       kReverseFFT,
       &fft_input_output_[0],
       &fft_integer_working_area_[0],
       &fft_double_working_area_[0]);

  ABSL_DCHECK_EQ(overlap_, working_output_.size());

  for (int k = 0; k < frame_size_; ++k) {
    double current_value =
        fft_input_output_[k] / half_fft_length * WindowValue(k);
    if (k < overlap_) {
      working_output_[k] += current_value;
    } else {
      working_output_.push_back(current_value);
    }
  }
}

inline double InverseSpectrogram::WindowValue(int sample_number) const {
  if (!with_window_function_) {
    return 1.0;
  }
  if (sample_number > window_function_.size()) {
    return 0.0;
  }
  return window_function_[sample_number];
}

template <class OutputSample>
bool InverseSpectrogram::Flush(std::vector<OutputSample>* output) {
  output->clear();

  if (!initialized_) {
    LOG(ERROR) << "Flush() called before successful call to Initialize().";
    return false;
  }
  if (at_least_one_frame_processed_) {
    if (output) {
      while (!working_output_.empty()) {
        output->push_back(working_output_.front());
        working_output_.pop_front();
      }
    }
  }
  // Reset to original state so that Process() may be called again.
  Initialize(fft_length_, step_length_);
  return true;
}
template bool InverseSpectrogram::Flush(std::vector<double>*);
template bool InverseSpectrogram::Flush(std::vector<float>*);

}  // namespace audio_dsp
