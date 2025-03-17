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

#include "audio/dsp/kiss_fft_util.h"

#include <math.h>

#include <algorithm>
#include <cmath>
#include <complex>

#include "audio/dsp/signal_vector_util.h"

#include "glog/logging.h"

namespace audio_dsp {

using std::complex;

void Upsample(const std::vector<float>& input, int upsampling_factor,
              std::vector<float>* result) {
  ABSL_DCHECK_GT(upsampling_factor, 1);
  const int result_size = static_cast<int>(input.size()) * upsampling_factor;

  // When size is even, the original spectrum is whole-sample Hermitian
  // symmetric about the Nyquist sample, e.g., [A, B, C, D, C*, B*], and the DC
  // and Nyquist samples A and D are real. The upsampled spectrum is
  // [A, B, C, D/2, 0, 0, ..., 0, D/2, C*, B*].
  //
  // Upsampling is similar when the size is odd, except for the treatment of the
  // last sample. An odd-sized spectrum is half-sample Hermitian symmetric about
  // the Nyquist frequency, e.g., [A, B, C, D, D*, C*, B*], and the upsampled
  // spectrum is [A, B, C, D, 0, 0, ..., 0, D*, C*, B*].
  std::vector<complex<float>> spectrum;
  RealFFTTransformer(static_cast<int>(input.size()), false)
      .ForwardTransform(input, &spectrum);

  if (input.size() % 2 == 0) {
    spectrum.back() /= 2;
  }

  RealFFTTransformer upsampled_transformer(result_size, false);
  // Fill high frequencies with zeros.
  spectrum.resize(upsampled_transformer.GetTransformedSize(), 0.0f);
  upsampled_transformer.InverseTransform(spectrum, result);

  // Divide by input.size() because Kiss FFT scales up when going forward.
  MultiplyVectorByScalar(1.0 / input.size(), result);
}

void FftHardLowpass(const std::vector<float>& input, float high_band_edge,
                    float sample_rate, std::vector<float>* output) {
  FftHardBandpass(input, 0, high_band_edge, sample_rate, output);
}

void HardLowpassSpectrum(const std::vector<complex<float>>& input_spectrum,
                         float high_band_edge, float bin_width,
                         std::vector<complex<float>>* output_spectrum) {
  HardBandpassSpectrum(input_spectrum, 0, high_band_edge, bin_width,
                       output_spectrum);
}

void FftHardHighpass(const std::vector<float>& input, float low_band_edge,
                     float sample_rate, std::vector<float>* output) {
  FftHardBandpass(input, low_band_edge, sample_rate / 2, sample_rate, output);
}

void HardHighpassSpectrum(const std::vector<complex<float>>& input_spectrum,
                          float low_band_edge, float bin_width,
                          std::vector<complex<float>>* output_spectrum) {
  const float high_band_edge = (input_spectrum.size() - 0.5) * bin_width;
  HardBandpassSpectrum(input_spectrum, low_band_edge, high_band_edge, bin_width,
                       output_spectrum);
}

void FftHardBandpass(const std::vector<float>& input, float low_band_edge,
                     float high_band_edge, float sample_rate,
                     std::vector<float>* output) {
  ABSL_CHECK(!input.empty());
  ABSL_CHECK_GE(low_band_edge, 0.0f);
  ABSL_CHECK_LE(low_band_edge, high_band_edge);
  // Check upper limit with a tolerance of 0.5 / input.size() above Nyquist to
  // permit numerical imprecision.
  ABSL_CHECK_LT(high_band_edge, sample_rate * (0.5f + 0.5f / input.size()));
  ABSL_CHECK(output != nullptr);
  const int num_samples = static_cast<int>(input.size());
  RealFFTTransformer transformer(num_samples, true);
  std::vector<complex<float>> spectrum;
  transformer.ForwardTransform(input, &spectrum);
  const float bin_width = sample_rate / static_cast<float>(num_samples);
  HardBandpassSpectrum(spectrum, low_band_edge, high_band_edge, bin_width,
                       &spectrum);
  transformer.InverseTransform(spectrum, output);
}

void HardBandpassSpectrum(const std::vector<complex<float>>& input_spectrum,
                          float low_band_edge, float high_band_edge,
                          float bin_width,
                          std::vector<complex<float>>* output_spectrum) {
  ABSL_CHECK(!input_spectrum.empty());
  const int spectrum_size = static_cast<int>(input_spectrum.size());
  ABSL_CHECK(output_spectrum != nullptr);
  ABSL_CHECK_GE(low_band_edge, 0.0f);
  ABSL_CHECK_LE(low_band_edge, high_band_edge);
  ABSL_CHECK_LE(high_band_edge, bin_width * spectrum_size);

  output_spectrum->resize(input_spectrum.size());
  // We define the passband as the closed interval
  // low_band_edge <= f <= high_band_edge; the passband includes the band edges
  // if they happen to fall exactly on a bin.

  // The DFT spectrum samples the frequencies
  // {bin_width * k} for k = 0, ..., spectrum_size.
  // The passband is the set of bin indices k that satisfy
  // low_band_edge / bin_width <= k <= high_band_edge / bin_width,
  // and since k is integer,
  // ceil(low_band_edge / bin_width) <= k <= floor(high_band_edge / bin_width).
  // Therefore, the first index in the passband is
  // ceil(low_band_edge / bin_width), and the first index in the upper
  // stopband is 1 + floor(high_band_edge / bin_width).
  int passband_start = std::ceil(low_band_edge / bin_width);
  int start_index = std::min<int>(passband_start, spectrum_size);
  std::fill(output_spectrum->begin(), output_spectrum->begin() + start_index,
            complex<float>(0.0f));
  int stopband_start = 1 + std::floor(high_band_edge / bin_width);
  int stop_index = std::min<int>(stopband_start, spectrum_size);

  // If this function is not being used for in-place filtering, make sure the
  // passband gets copied over to the output spectrum. We define the passband as
  // the closed interval low_band_edge <= f <= high_band_edge; the passband
  // includes the band edges if they happen to fall exactly on a bin.
  if (&input_spectrum != output_spectrum) {
    std::copy(input_spectrum.begin() + start_index,
              input_spectrum.begin() + stop_index,
              output_spectrum->begin() + start_index);
  }
  // Fill the upper stopband with zeros.
  std::fill(output_spectrum->begin() + stop_index, output_spectrum->end(),
            complex<float>(0.0f));
}

void FftPeriodicResample(const std::vector<float>& input,
                         float input_sample_rate, float output_sample_rate,
                         std::vector<float>* output) {
  ABSL_CHECK_GT(input_sample_rate, 0);
  ABSL_CHECK_GT(output_sample_rate, 0);
  ABSL_CHECK(output);
  const int input_size = static_cast<int>(input.size());
  const int output_size =
      std::round(input_size * (output_sample_rate / input_sample_rate));
  if (output_size == input_size) {
    *output = input;
    return;
  }
  std::vector<complex<float>> spectrum;
  RealFFTTransformer(input_size, false).ForwardTransform(input, &spectrum);

  if (input_size % 2 == 0 && output_size > input.size()) {
    spectrum.back() /= 2;
  }
  RealFFTTransformer output_transformer(output_size, false);
  spectrum.resize(output_transformer.GetTransformedSize(), 0.0);
  output_transformer.InverseTransform(spectrum, output);
  for (float& sample : *output) {
    sample /= input_size;
  }
}

void PeriodicGaussianConvolution(const std::vector<float>& input, float sigma,
                                 std::vector<float>* result) {
  ABSL_CHECK(result);
  RealFFTTransformer transformer(static_cast<int>(input.size()), false);
  std::vector<complex<float>> spectrum;
  transformer.ForwardTransform(input, &spectrum);

  // Multiply spectrum[k] pointwise by
  // exp[-(2 pi^2 sigma^2 / input.size()^2) k^2] / input.size(). The division by
  // input.size() is the normalization factor for the FFT.
  const float factor = 2 * Square(M_PI * sigma / input.size());
  float weight = 1.0f / input.size();
  float ratio = std::exp(-factor);
  const float ratio_ratio = std::exp(-2 * factor);
  // The weights are computed recursively to avoid evaluations of exp. At the
  // beginning of the kth iteration, ratio_k = ratio_0^(1 + 2k) and
  // weight_k = ratio_0^(k^2) * weight_0 = exp(-factor k^2) / input.size()
  // where subscript denotes the iteration number k = 0, ..., input.size() - 1.
  for (complex<float>& x : spectrum) {
    x *= weight;
    weight *= ratio;
    ratio *= ratio_ratio;
  }

  transformer.InverseTransform(spectrum, result);
}

void LinearConvolution(const float* input_a, int size_a, const float* input_b,
                       int size_b, float* output) {
  const int fft_size =
      audio_dsp::RealFFTTransformer::GetNextFastSize(size_a + size_b - 1);
  std::vector<float> x(fft_size, 0.0);
  std::vector<float> y(fft_size, 0.0);
  std::copy(input_a, input_a + size_a, x.begin());
  std::copy(input_b, input_b + size_b, y.begin());

  std::vector<complex<float>> x_transformed;
  std::vector<complex<float>> y_transformed;

  audio_dsp::RealFFTTransformer transformer(fft_size, true);
  transformer.ForwardTransform(x, &x_transformed);
  transformer.ForwardTransform(y, &y_transformed);

  std::vector<float> convolution;
  std::vector<complex<float>> convolution_transformed;
  convolution_transformed.reserve(x_transformed.size());
  for (int i = 0; i < x_transformed.size(); ++i) {
    convolution_transformed.push_back(x_transformed[i] * y_transformed[i]);
  }
  transformer.InverseTransform(convolution_transformed, &convolution);
  convolution.resize(size_a + size_b - 1);

  std::copy(convolution.begin(), convolution.begin() + size_a + size_b - 1,
            output);
}

void LinearConvolution(const float* input, int size, int num_times,
                       float* output) {
  ABSL_CHECK_GT(num_times, 0);
  const int output_size = (size - 1) * num_times + 1;
  const int fft_size =
      audio_dsp::RealFFTTransformer::GetNextFastSize(output_size);
  std::vector<float> x(fft_size, 0.0);
  std::copy(input, input + size, x.begin());

  std::vector<complex<float>> x_transformed;

  audio_dsp::RealFFTTransformer transformer(fft_size, true);
  transformer.ForwardTransform(x, &x_transformed);

  std::vector<float> convolution;
  std::vector<complex<float>> convolution_transformed;
  convolution_transformed.reserve(x_transformed.size());
  for (int i = 0; i < x_transformed.size(); ++i) {
    convolution_transformed.push_back(
        std::complex<float>(std::pow(x_transformed[i], num_times)));
  }
  transformer.InverseTransform(convolution_transformed, &convolution);
  convolution.resize(output_size);

  std::copy(convolution.begin(), convolution.begin() + output_size, output);
}
}  // namespace audio_dsp
