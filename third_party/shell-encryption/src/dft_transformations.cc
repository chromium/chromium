/*
 * Copyright 2025 Google LLC.
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

#include "dft_transformations.h"

#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

#include "absl/status/status.h"

namespace rlwe {

namespace {

// Returns true if n is a power of two.
inline bool IsPowerOfTwo(size_t n) { return (n & (n - 1)) == 0; }

}  // namespace

absl::Status IterativeHalfCooleyTukey(
    std::vector<std::complex<double>>& coeffs,
    const std::vector<std::complex<double>>& psis_bitrev) {
  if (!IsPowerOfTwo(coeffs.size())) {
    return absl::InvalidArgumentError(
        "The size of `coeffs` must be a power of two.");
  }
  int len = coeffs.size();
  if (psis_bitrev.size() < len) {
    return absl::InvalidArgumentError(
        "Not enough primitive roots in `psis_bitrev`.");
  }
  int log_len = log2(len);
  for (int i = log_len - 1; i >= 0; i--) {
    // Layer i.
    const int half_m = 1 << i;
    const int m = half_m << 1;
    for (int k = 0, index_psi = 1 << (log_len - i); k < coeffs.size();
         k += m, ++index_psi) {
      const std::complex<double> psi = psis_bitrev[index_psi];
      for (int j = 0; j < half_m; j++) {
        // The Cooley-Tukey butterfly operation.
        std::complex<double> t = coeffs[k + j + half_m] * psi;
        std::complex<double> u = coeffs[k + j];
        coeffs[k + j] += t;
        coeffs[k + j + half_m] = u - t;
      }
    }
  }
  return absl::OkStatus();
}

absl::Status IterativeHalfGentlemanSande(
    std::vector<std::complex<double>>& coeffs,
    const std::vector<std::complex<double>>& psis_bitrev_inv) {
  if (!IsPowerOfTwo(coeffs.size())) {
    return absl::InvalidArgumentError(
        "The size of `coeffs` must be a power of two.");
  }
  int len = coeffs.size();
  if (psis_bitrev_inv.size() < len * 2) {
    return absl::InvalidArgumentError(
        "Not enough primitive roots in `psis_bitrev_inv`.");
  }
  int log_len = log2(len);
  int index_psi_base = 0;
  for (int i = 0; i < log_len; i++) {
    const int half_m = 1 << i;
    const int m = half_m << 1;
    for (int k = 0, index_psi_inv = index_psi_base; k < coeffs.size();
         k += m, ++index_psi_inv) {
      for (int j = 0; j < half_m; j++) {
        // The Gentleman-Sande butterfly operation.
        std::complex<double> t = coeffs[k + j + half_m];
        std::complex<double> u = coeffs[k + j];
        coeffs[k + j] += t;
        coeffs[k + j + half_m] = (u - t) * psis_bitrev_inv[index_psi_inv];
      }
    }
    index_psi_base += 1 << (log_len - i);
  }
  return absl::OkStatus();
}

}  // namespace rlwe
