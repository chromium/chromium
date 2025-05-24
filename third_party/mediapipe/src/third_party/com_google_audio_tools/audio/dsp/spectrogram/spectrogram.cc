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

#include <math.h>
#include <optional>

#include "audio/dsp/number_util.h"
#include "audio/dsp/window_functions.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {

using std::complex;

bool Spectrogram::ResetSampleBuffer() {
  // Set flag element to ensure that the working areas are initialized
  // on the first call to rdft.
  fft_integer_working_area_[0] = 0;
  input_queue_.clear();
  samples_to_next_step_ = window_length_;
  return true;
}

bool Spectrogram::Initialize(int window_length, int step_length,
  std::optional<int> fft_length) {
  std::vector<double> window;
  HannWindow().GetPeriodicSamples(window_length, &window);
  return Initialize(window, step_length, fft_length);
}

bool Spectrogram::Initialize(const std::vector<double>& window,
                             int step_length,
                             std::optional<int> fft_length) {
  window_length_ = window.size();
  window_ = window;  // Copy window.
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
  ABSL_CHECK(fft_length_ >= window_length_);
  output_frequency_channels_ = 1 + fft_length_ / 2;

  // Allocate 2 more than what rdft needs, so we can rationalize the layout.
  fft_input_output_.assign(fft_length_ + 2, 0.0);

  int half_fft_length = fft_length_ / 2;
  fft_double_working_area_.assign(half_fft_length, 0.0);
  fft_integer_working_area_.assign(2 + static_cast<int>(sqrt(half_fft_length)),
                                   0);
  ResetSampleBuffer();
  initialized_ = true;
  return true;
}

template <class InputSample, class OutputSample>
bool Spectrogram::ComputeComplexSpectrogram(
    const std::vector<InputSample>& input,
    std::vector<std::vector<complex<OutputSample>>>* output) {
  if (!initialized_) {
    LOG(ERROR) << "ComputeComplexSpectrogram() called before successful call "
               << "to Initialize().";
    return false;
  }
  ABSL_CHECK(output != nullptr);
  output->clear();
  int input_start = 0;
  while (GetNextWindowOfSamples(input, &input_start)) {
    ABSL_DCHECK_EQ(window_length_, static_cast<int>(input_queue_.size()));
    ProcessCoreFFT();  // Processes input_queue_ to fft_input_output_.
    // Add a new slice vector onto the output, to save new result to.
    output->resize(output->size() + 1);
    // Get a reference to the newly added slice to fill in.
    auto& spectrogram_slice = output->back();
    spectrogram_slice.resize(output_frequency_channels_);
    for (int i = 0; i < output_frequency_channels_; ++i) {
      // This will convert double to float if it needs to.
      spectrogram_slice[i] = complex<OutputSample>(
          fft_input_output_[2 * i],
          fft_input_output_[2 * i + 1]);
    }
  }
  return true;
}

// Instantiate the templated functions four ways.

// This is problematic because of the disconnect between the instantiation of
// template in the client code and here; it might be preferable to have this
// code somehow included in the header used by client code, but that would end
// up dragging in most of the code in this file. This setup works (for now?).

template
bool Spectrogram::ComputeComplexSpectrogram(
    const vector<float>& input, vector<vector<complex<float>>>*);
template
bool Spectrogram::ComputeComplexSpectrogram(
    const vector<double>& input, vector<vector<complex<float>>>*);
template
bool Spectrogram::ComputeComplexSpectrogram(
    const vector<float>& input, vector<vector<complex<double>>>*);
template
bool Spectrogram::ComputeComplexSpectrogram(
    const vector<double>& input, vector<vector<complex<double>>>*);

template <class InputSample, class OutputSample>
bool Spectrogram::ComputeSquaredMagnitudeSpectrogram(
    const std::vector<InputSample>& input,
    std::vector<std::vector<OutputSample>>* output) {
  if (!initialized_) {
    LOG(ERROR) << "ComputeSquaredMagnitudeSpectrogram() called before "
               << "successful call to Initialize().";
    return false;
  }
  ABSL_CHECK(output);
  output->clear();
  int input_start = 0;
  while (GetNextWindowOfSamples(input, &input_start)) {
    ABSL_DCHECK_EQ(window_length_, static_cast<int>(input_queue_.size()));
    ProcessCoreFFT();  // Processes input_queue_ to fft_input_output_.
    // Add a new slice vector onto the output, to save new result to.
    output->resize(output->size() + 1);
    // Get a reference to the newly added slice to fill in.
    auto& spectrogram_slice = output->back();
    spectrogram_slice.resize(output_frequency_channels_);
    for (int i = 0; i < output_frequency_channels_; ++i) {
      // Similar to the Complex case, except storing the norm.
      // But the norm function is known to be a performance killer,
      // so do it this way with explicit real and imagninary temps.
      const double re = fft_input_output_[2 * i];
      const double im = fft_input_output_[2 * i + 1];
      // Which finally converts double to float if it needs to.
      spectrogram_slice[i] = re * re + im * im;
    }
  }
  return true;
}

template
bool Spectrogram::ComputeSquaredMagnitudeSpectrogram(
    const vector<float>& input, vector<vector<float>>*);
template
bool Spectrogram::ComputeSquaredMagnitudeSpectrogram(
    const vector<double>& input, vector<vector<float>>*);
template
bool Spectrogram::ComputeSquaredMagnitudeSpectrogram(
    const vector<float>& input, vector<vector<double>>*);
template
bool Spectrogram::ComputeSquaredMagnitudeSpectrogram(
    const vector<double>& input, vector<vector<double>>*);

// Add wrappers to provide a single ComputeSpectrogram function that selects
// the Complex or SquaredMagnitude versions based on the output pointer type.
template <class InputSample, class OutputSample>
bool Spectrogram::ComputeSpectrogram(
    const std::vector<InputSample>& input,
    std::vector<std::vector<std::complex<OutputSample>>>* output) {
  return Spectrogram::ComputeComplexSpectrogram(input, output);
}

template <class InputSample, class OutputSample>
bool Spectrogram::ComputeSpectrogram(
    const std::vector<InputSample>& input,
    std::vector<std::vector<OutputSample>>* output) {
  return Spectrogram::ComputeSquaredMagnitudeSpectrogram(input, output);
}

template
bool Spectrogram::ComputeSpectrogram(
    const vector<float>& input, vector<vector<complex<float>>>*);
template
bool Spectrogram::ComputeSpectrogram(
    const vector<double>& input, vector<vector<complex<float>>>*);
template
bool Spectrogram::ComputeSpectrogram(
    const vector<float>& input, vector<vector<complex<double>>>*);
template
bool Spectrogram::ComputeSpectrogram(
    const vector<double>& input, vector<vector<complex<double>>>*);

template
bool Spectrogram::ComputeSpectrogram(
    const vector<float>& input, vector<vector<float>>*);
template
bool Spectrogram::ComputeSpectrogram(
    const vector<double>& input, vector<vector<float>>*);
template
bool Spectrogram::ComputeSpectrogram(
    const vector<float>& input, vector<vector<double>>*);
template
bool Spectrogram::ComputeSpectrogram(
    const vector<double>& input, vector<vector<double>>*);

// Return true if a full window of samples is prepared; manage the queue.
template <class InputSample>
bool Spectrogram::GetNextWindowOfSamples(const vector<InputSample>& input,
                                         int* input_start) {
  auto input_it = input.begin() + *input_start;
  int input_remaining = input.end() - input_it;
  if (samples_to_next_step_ > input_remaining) {
    // Copy in as many samples are left and return false, no full window.
    input_queue_.insert(input_queue_.end(), input_it, input.end());
    *input_start += input_remaining;  // Increases it to input.size().
    samples_to_next_step_ -= input_remaining;
    return false;  // Not enough for a full window.
  } else {
    // Copy just enough into queue to make a new window, then trim the
    // front off the queue to make it window-sized.
    input_queue_.insert(input_queue_.end(),
                        input_it,
                        input_it + samples_to_next_step_);
    *input_start += samples_to_next_step_;
    input_queue_.erase(input_queue_.begin(),
                       input_queue_.begin() +
                       input_queue_.size() - window_length_);
    ABSL_DCHECK_EQ(window_length_, static_cast<int>(input_queue_.size()));
    samples_to_next_step_ = step_length_;  // Be ready for next time.
    return true;  // Yes, input_queue_ now contains exactly a window-full.
  }
}

void Spectrogram::ProcessCoreFFT() {
  for (int j = 0; j < window_length_; ++j) {
    fft_input_output_[j] = input_queue_[j] * window_[j];
  }
  // Zero-pad the rest of the input buffer.
  for (int j = window_length_; j < fft_length_; ++j) {
    fft_input_output_[j] = 0.0;
  }
  const int kForwardFFT = 1;  // 1 means forward; -1 reverse.
  // This real FFT is a fair amount faster than using cdft here.
  rdft(fft_length_,
       kForwardFFT,
       &fft_input_output_[0],
       &fft_integer_working_area_[0],
       &fft_double_working_area_[0]);
  // Make rdft result look like cdft result;
  // unpack the last real value from the first position's imag slot.
  fft_input_output_[fft_length_] = fft_input_output_[1];
  fft_input_output_[fft_length_ + 1] = 0;
  fft_input_output_[1] = 0;
}

}  // namespace audio_dsp
