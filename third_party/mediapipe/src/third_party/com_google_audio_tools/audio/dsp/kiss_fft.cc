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

// (c) Google Inc. All Rights Reserved.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "audio/dsp/kiss_fft.h"

#include <algorithm>
#include <complex>
#include <memory>

#include "glog/logging.h"
#include "third_party/kissfft/kiss_fft_float.h"

namespace audio_dsp {

using ::kissfft_float::kiss_fft_alloc;
using ::kissfft_float::kiss_fft_cfg;
using ::kissfft_float::kiss_fft_cpx;
using ::kissfft_float::kiss_fft_next_fast_size;
using ::kissfft_float::kiss_fftr_alloc;
using ::kissfft_float::kiss_fftr_cfg;
using ::kissfft_float::kiss_fftri;
using ::std::complex;

class ComplexFFTTransformerImpl {
 public:
  virtual ~ComplexFFTTransformerImpl() {}
  virtual int GetSize() const = 0;
  virtual void ForwardTransform(const complex<float>* input,
                           complex<float>* output) = 0;
  virtual void InverseTransform(const complex<float>* input,
                           complex<float>* output) = 0;
};

class RealFFTTransformerImpl {
 public:
  virtual ~RealFFTTransformerImpl() {}
  virtual int GetSize() const = 0;
  virtual void ForwardTransform(const float* input, complex<float>* output) = 0;
  virtual void InverseTransform(const complex<float>* input, float* output) = 0;
};

namespace {

// Determine whether a given FFT size factors such that Kiss FFT would be fast.
bool IsFastKissFFTSize(int size) {
  while ((size % 2) == 0) {
    size /= 2;
  }
  while ((size % 3) == 0) {
    size /= 3;
  }
  while ((size % 5) == 0) {
    size /= 5;
  }
  // After dividing out factors of 2, 3, and 5, Kiss FFT will handle the
  // remaining factor by a generic O(N^2) codelet. Therefore, this remaining
  // factor must be small for Kiss FFT to be fast.
  //
  // kMaxFastFactor was optimized by comparing benchmark results for
  // KissComplexFFTTransformerImpl vs. BluesteinComplexFFTTransformerImpl over
  // a sweep of prime sizes.
  constexpr int kMaxFastFactor = 25;
  return size <= kMaxFastFactor;
}

// Multiply array of length size by multiplier.
template <typename ValueType>
void MultiplyArrayByScalar(float multiplier, int size, ValueType* array) {
  for (int i = 0; i < size; ++i) {
    array[i] *= multiplier;
  }
}

// Given a spectrum of nonnegative frequencies, fill the negative frequencies
// by complex-conjugate reflection.
void ReflectSpectrum(int size, std::vector<complex<float>>* spectrum) {
  ABSL_CHECK_EQ(size / 2 + 1, spectrum->size());
  int i = static_cast<int>(spectrum->size()) - (size % 2 == 0 ? 2 : 1);
  for (; i > 0; --i) {
    spectrum->push_back(std::conj((*spectrum)[i]));
  }
}

// Use Kiss FFT to perform complex-to-complex FFTs.
class KissComplexFFTTransformerImpl: public ComplexFFTTransformerImpl {
 public:
  explicit KissComplexFFTTransformerImpl(int size)
      : size_(size),
        forward_config_(
            ABSL_DIE_IF_NULL(kiss_fft_alloc(size, 0, nullptr, nullptr))),
        inverse_config_(
            ABSL_DIE_IF_NULL(kiss_fft_alloc(size, 1, nullptr, nullptr))) {}

  ~KissComplexFFTTransformerImpl() override {
    kiss_fft_free(forward_config_);
    kiss_fft_free(inverse_config_);
  }

  int GetSize() const override {
    return size_;
  }

  void ForwardTransform(const complex<float>* input,
                        complex<float>* output) override {
    kiss_fft(forward_config_,
             reinterpret_cast<const kiss_fft_cpx*>(input),
             reinterpret_cast<kiss_fft_cpx*>(output));
  }

  void InverseTransform(const complex<float>* input,
                        complex<float>* output) override {
    kiss_fft(inverse_config_,
             reinterpret_cast<const kiss_fft_cpx*>(input),
             reinterpret_cast<kiss_fft_cpx*>(output));
  }

 private:
  const int size_;
  kiss_fft_cfg forward_config_;
  kiss_fft_cfg inverse_config_;
};

// Use Kiss FFT to perform real-to-complex FFTs.
// NOTE: Kiss FFT is limited to even sized real-to-complex FFTs.
class KissRealFFTTransformerImpl: public RealFFTTransformerImpl {
 public:
  explicit KissRealFFTTransformerImpl(int size)
      : size_(size),
        forward_config_(
            ABSL_DIE_IF_NULL(kiss_fftr_alloc(size, 0, nullptr, nullptr))),
        inverse_config_(
            ABSL_DIE_IF_NULL(kiss_fftr_alloc(size, 1, nullptr, nullptr))) {}

  ~KissRealFFTTransformerImpl() override {
    kiss_fftr_free(forward_config_);
    kiss_fftr_free(inverse_config_);
  }

  int GetSize() const override {
    return size_;
  }

  void ForwardTransform(const float* input, complex<float>* output) override {
    kiss_fftr(forward_config_,
              reinterpret_cast<const kiss_fft_scalar*>(input),
              reinterpret_cast<kiss_fft_cpx*>(output));
  }

  void InverseTransform(const complex<float>* input, float* output) override {
    kiss_fftri(inverse_config_,
              reinterpret_cast<const kiss_fft_cpx*>(input),
              reinterpret_cast<kiss_fft_scalar*>(output));
  }

 private:
  const int size_;
  kiss_fftr_cfg forward_config_;
  kiss_fftr_cfg inverse_config_;
};

// A wrapper to use a ComplexFFTTransformerImpl to perform real-to-complex FFTs,
// by throwing away the upper half of the spectrum on the forward transform or
// Hermitian mirroring the spectrum before the inverse transform.
class ComplexBackendRealFFTTransformerImpl: public RealFFTTransformerImpl {
 public:
  explicit ComplexBackendRealFFTTransformerImpl(
      ComplexFFTTransformerImpl* backend)
      : backend_(ABSL_DIE_IF_NULL(backend)) {}

  int GetSize() const override {
    return backend_->GetSize();
  }

  void ForwardTransform(const float* input, complex<float>* output) override {
    const int size = GetSize();
    std::vector<complex<float>> complex_input(size);
    std::copy(input, input + size, complex_input.begin());
    std::vector<complex<float>> buffer(size);
    backend_->ForwardTransform(complex_input.data(), buffer.data());
    // Copy only the nonnegative frequencies to output.
    std::copy(buffer.begin(), buffer.begin() + (size / 2 + 1), output);
  }

  void InverseTransform(const complex<float>* input, float* output) override {
    const int size = GetSize();
    std::vector<complex<float>> reflected_input(input, input + (size / 2 + 1));
    // Extend input spectrum to length size_ by complex conjugate symmetry.
    ReflectSpectrum(size, &reflected_input);
    std::vector<complex<float>> complex_output(size);
    backend_->InverseTransform(reflected_input.data(), complex_output.data());
    for (int i = 0; i < size; ++i) {
      output[i] = complex_output[i].real();
    }
  }

 private:
  std::unique_ptr<ComplexFFTTransformerImpl> backend_;
};

// Use Bluestein's FFT algorithm to express an arbitrary-size FFT as a
// convolution which can be computed with FFTs of some fast size.
// [http://en.wikipedia.org/wiki/Chirp_Z-transform#Bluestein.27s_algorithm]
// The FFT
//   X[k] = sum_n x[n] exp(-i 2 pi k n / N),
// where the sum is over n = 0, 1, ..., N - 1, is rewritten using the identity
// nk = (-(k - n)^2 + n^2 + k^2) / 2 as the convolution
//   X[k] = b[k]* sum_n a[n] b[k - n]
// in which
//   a[n] = x[n] exp(-i pi n^2 / N),
//   b[n] = exp(i pi n^2 / N).
// The convolution can be performed using FFTs of any size greater than or equal
// to (2N - 1). This FFT-based convolution is computed with Kiss FFT using a
// fast composite size.
class BluesteinComplexFFTTransformerImpl: public ComplexFFTTransformerImpl {
 public:
  // Constructor. A fast FFT size is determined and the transform of b[n] and
  // twiddle_factors_ = b[n]* / fft_size are precomputed.
  explicit BluesteinComplexFFTTransformerImpl(int size)
      : size_(size),
        backend_(kiss_fft_next_fast_size(2 * size - 1)) {
    const int fft_size = backend_.GetSize();
    std::vector<complex<float>> b(fft_size, 0.0);
    twiddle_factors_.resize(size);

    // Recursively generate the complex chirp exp(i pi n^2 / N).
    complex<double> rotator = std::polar(1.0, M_PI / size);
    complex<double> rotator_step = rotator * rotator;
    complex<double> phasor = 1.0;
    b[0] = 1.0;
    twiddle_factors_[0] = 1.0 / fft_size;
    for (int i = 1; i < size; ++i) {
      phasor *= rotator;
      rotator *= rotator_step;
      b[i] = static_cast<complex<float>>(phasor);
      b[fft_size - i] = b[i];
      twiddle_factors_[i] = std::conj(b[i]) / static_cast<float>(fft_size);
    }

    b_spectrum_.resize(fft_size);
    backend_.ForwardTransform(b.data(), b_spectrum_.data());
  }

  int GetSize() const override {
    return size_;
  }

  void ForwardTransform(const complex<float>* input,
                        complex<float>* output) override {
    const int fft_size = backend_.GetSize();
    // Compute the sequence a[n] = input[n] exp(-i pi n^2 / N).
    std::vector<complex<float>> a = MultiplyByChirp(input, -1);
    a.resize(fft_size, 0.0);

    // Perform convolution with b[n].
    std::vector<complex<float>> convolution_spectrum(fft_size);
    backend_.ForwardTransform(a.data(), convolution_spectrum.data());
    for (int i = 0; i < fft_size; ++i) {
      convolution_spectrum[i] *= b_spectrum_[i];
    }
    std::vector<complex<float>> convolution(fft_size);
    backend_.InverseTransform(convolution_spectrum.data(), convolution.data());

    // Multiply by b[n]* / fft_size.
    for (int i = 0; i < size_; ++i) {
      output[i] = twiddle_factors_[i] * convolution[i];
    }
  }

  void InverseTransform(const complex<float>* input,
                        complex<float>* output) override {
    const int fft_size = backend_.GetSize();
    std::vector<complex<float>> a = MultiplyByChirp(input, 1);
    a.resize(fft_size, 0.0);

    std::vector<complex<float>> convolution_spectrum(fft_size);
    backend_.ForwardTransform(a.data(), convolution_spectrum.data());
    convolution_spectrum[0] *= std::conj(b_spectrum_[0]);
    for (int i = 1; i < fft_size; ++i) {
      convolution_spectrum[i] *= std::conj(b_spectrum_[fft_size - i]);
    }
    std::vector<complex<float>> convolution(fft_size);
    backend_.InverseTransform(convolution_spectrum.data(), convolution.data());

    for (int i = 0; i < size_; ++i) {
      output[i] = std::conj(twiddle_factors_[i]) * convolution[i];
    }
  }

 private:
  // Multiply input by the complex chirp exp(i sign pi n^2 / N).
  std::vector<complex<float>> MultiplyByChirp(const complex<float>* input,
                                         int sign) const {
    complex<double> rotator = std::polar(1.0, sign * M_PI / size_);
    complex<double> rotator_step = rotator * rotator;
    complex<double> phasor = 1.0;
    std::vector<complex<float>> result(size_);
    result[0] = input[0];
    for (int i = 1; i < size_; ++i) {
      phasor *= rotator;
      rotator *= rotator_step;
      result[i] = input[i] * static_cast<complex<float>>(phasor);
    }
    return result;
  }

  const int size_;
  std::vector<complex<float>> b_spectrum_;
  std::vector<complex<float>> twiddle_factors_;
  KissComplexFFTTransformerImpl backend_;
};

}  // namespace

ComplexFFTTransformer::ComplexFFTTransformer(int size, bool normalization)
    : normalization_(normalization) {
  ABSL_CHECK_GT(size, 0);
  // Kiss FFT is fast for sizes having factors 2, 3, and 5. Otherwise, Kiss FFT
  // degrades to O(N^2) performance. Use Bluestein for non-fast sizes.
  if (!IsFastKissFFTSize(size)) {
    impl_ = new BluesteinComplexFFTTransformerImpl(size);
  } else {
    impl_ = new KissComplexFFTTransformerImpl(size);
  }
}

ComplexFFTTransformer::~ComplexFFTTransformer() {
  delete impl_;
}

int ComplexFFTTransformer::GetSize() const {
  return impl_->GetSize();
}

// static
int ComplexFFTTransformer::GetNextFastSize(int size) {
  return kiss_fft_next_fast_size(size);
}

void ComplexFFTTransformer::ForwardTransform(
    const std::vector<complex<float>>& input,
    std::vector<complex<float>>* output) {
  ABSL_CHECK_EQ(input.size(), GetSize());
  ABSL_CHECK(output);
  output->resize(input.size());
  ForwardTransform(input.data(), output->data());
}

void ComplexFFTTransformer::ForwardTransform(
    const complex<float>* input, complex<float>* output) {
  ABSL_CHECK(input);
  ABSL_CHECK(output);
  impl_->ForwardTransform(input, output);
}

void ComplexFFTTransformer::InverseTransform(
    const std::vector<complex<float>>& input,
    std::vector<complex<float>>* output) {
  ABSL_CHECK_EQ(input.size(), GetSize());
  ABSL_CHECK(output);
  output->resize(input.size());
  InverseTransform(input.data(), output->data());
}

void ComplexFFTTransformer::InverseTransform(
    const complex<float>* input, complex<float>* output) {
  ABSL_CHECK(input);
  ABSL_CHECK(output);
  impl_->InverseTransform(input, output);
  if (normalization_) {
    const int size = GetSize();
    MultiplyArrayByScalar(1.0f / size, size, output);
  }
}

RealFFTTransformer::RealFFTTransformer(int size, bool normalization)
    : normalization_(normalization) {
  ABSL_CHECK_GT(size, 0);
  // Kiss FFT is fast for sizes having factors 2, 3, and 5. Otherwise, Kiss FFT
  // degrades to O(N^2) performance. Use Bluestein for non-fast sizes.
  if (!IsFastKissFFTSize(size)) {
    impl_ = new ComplexBackendRealFFTTransformerImpl(
        new BluesteinComplexFFTTransformerImpl(size));
  } else if (size % 2 == 0) {
    // Kiss FFT only supports even sizes for real-to-complex transforms.
    impl_ = new KissRealFFTTransformerImpl(size);
  } else {
    // If size is odd, fall back to complex-to-complex transforms.
    impl_ = new ComplexBackendRealFFTTransformerImpl(
        new KissComplexFFTTransformerImpl(size));
  }
}

RealFFTTransformer::~RealFFTTransformer() {
  delete impl_;
}

int RealFFTTransformer::GetSize() const {
  return impl_->GetSize();
}

// static
int RealFFTTransformer::GetNextFastSize(int size) {
  return kiss_fftr_next_fast_size_real(size);
}

void RealFFTTransformer::ForwardTransform(
    const std::vector<float>& input, std::vector<complex<float>>* output) {
  ABSL_CHECK_EQ(input.size(), GetSize());
  ABSL_CHECK(output);
  output->resize(GetTransformedSize());
  ForwardTransform(input.data(), output->data());
}

void RealFFTTransformer::ForwardTransform(
    const float* input, complex<float>* output) {
  ABSL_CHECK(input);
  ABSL_CHECK(output);
  impl_->ForwardTransform(input, output);
}

void RealFFTTransformer::InverseTransform(
    const std::vector<complex<float>>& input, std::vector<float>* output) {
  ABSL_CHECK_EQ(input.size(), GetTransformedSize());
  ABSL_CHECK(output);
  output->resize(GetSize());
  InverseTransform(input.data(), output->data());
}

void RealFFTTransformer::InverseTransform(
    const complex<float>* input, float* output) {
  ABSL_CHECK(input);
  ABSL_CHECK(output);
  impl_->InverseTransform(input, output);
  if (normalization_) {
    const int size = GetSize();
    MultiplyArrayByScalar(1.0f / size, size, output);
  }
}

}  // namespace audio_dsp
