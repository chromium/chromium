/*
 * Copyright 2023 Google LLC.
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

#ifndef RLWE_DFT_TRANSFORMATIONS_H_
#define RLWE_DFT_TRANSFORMATIONS_H_

#include <complex>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "ntt_parameters.h"
#include "status_macros.h"

// This file implements various discrete Fourier transformations to be used by
// arithmetic in polynomial rings.

namespace rlwe {

// Performs `log_len` iterations of the Cooley-Tukey butterfly on `coeffs`
// in-place, which has length 2^log_len.
//
// This is an implementation of the FFT from [Sei18, Sec. 2].
// [Sei18] https://eprint.iacr.org/2018/039
// All polynomial arithmetic performed is modulo (x^n+1) for n a power of two,
// with the coefficients operated on modulo a prime modulus.
//
// Let psi be a primitive 2n-th root of the unity, i.e., psi is a 2n-th root
// of unity such that psi^n = -1. Hence it holds that
//           x^n+1 = x^n-psi^n = (x^n/2-psi^n/2)*(x^n/2+psi^n/2)
//
//
// If f = f_0 + f_1*x + ... + f_{n-1}*x^(n-1) is the polynomial to transform,
// the i-th coefficient of the polynomial mod x^n/2-psi^n/2 can thus be
// computed as
//            f'_i = f_i + psi^(n/2)*f_(n/2+i),
// and the i-th coefficient of the polynomial mod x^n/2+psi^n/2 can thus be
// computed as
//            f''_i = f_i - psi^(n/2)*f_(n/2+i)
// This operation is called the Cooley-Tukey butterfly and is done
// iteratively during the NTT.
//
// The FFT can thus be performed in-place and after the k-th level, it
// produces the vector of polynomials with pairs of coefficients
//  f mod (x^(n/2^(k+1))-psi^brv[2^k+1]), f mod (x^(n/2^(k+1))+psi^brv[2^k+1])
// where brv maps a log(n)-bit number to its bitreversal.
template <typename ModularInt>
void IterativeCooleyTukey(std::vector<ModularInt>& coeffs, int log_len,
                          const NttParameters<ModularInt>& ntt_params,
                          const typename ModularInt::Params& mod_params) {
  int index_psi = 1;
  for (int i = log_len - 1; i >= 0; i--) {
    // Layer i.
    const int half_m = 1 << i;
    const int m = half_m << 1;
    for (int k = 0; k < coeffs.size(); k += m) {
      const auto& [psi_constant, psi_constant_barrett] =
          ntt_params.psis_bitrev_constant[index_psi];
      for (int j = 0; j < half_m; j++) {
        // The Cooley-Tukey butterfly operation.
        ModularInt t = coeffs[k + j + half_m].MulConstant(
            psi_constant, psi_constant_barrett, &mod_params);
        ModularInt u = coeffs[k + j];
        // Every odd layer, we perform *lazy operations* (i.e., we do not
        // reduce in [0, modulus] but keeps the result of the operations in
        // [0, 2*modulus]). At the beginning of the even layers, the input
        // will therefore be in [0, 2*modulus] and therefore after applying
        // addition or subtraction, the result will be in [0, 4*modulus].
        // Since 4*modulus fits in a Int, this enables the computation to
        // remains correct: we hence perform the regular modular addition and
        // subtraction which reduce the values to [0, modulus] at the end of
        // the even layers.
        if (i & 1) {
          coeffs[k + j].LazyAddInPlace(t, &mod_params);
          // Note that the variable t is reduced in [0, modulus], which is
          // required by the LazySubInPlace function.
          coeffs[k + j + half_m] = std::move(u.LazySubInPlace(t, &mod_params));
        } else {
          coeffs[k + j].AddInPlace(t, &mod_params);
          // Note that the variable t is reduced in [0, modulus], which is
          // required by the SubInPlace function.
          coeffs[k + j + half_m] = std::move(u.SubInPlace(t, &mod_params));
        }
      }
      index_psi++;
    }
  }
}

// Performs `log_len` iterations of the Gentleman-Sande butterfly on `coeffs`
// in-place, which has length 2^log_len.
//
// This implements the backward NTT transformation, which is computed similarly
// by iteratively inverting the NTT representation. For instance, using the same
// notation as the forward NTT above,
//    f'_i + f''_i = 2f_i and  psi^(-n/2)*(f'_i-f''_i) = 2c_(n/2+i).
//
// In particular, the butterfly operation differs from the Cooley-Tukey
// butterfly used during the forward transform in that addition and
// substraction come before multiplying with a power of the root of unity.
// This butterfly operation is called the Gentleman-Sande butterfly.
//
// Note: To complete the inverse NTT transformation, a normalization step by
// the inverse of n=2^log_len (the factor 2 obtained at each level of the
// butterfly) is required after calling this function.
template <typename ModularInt>
absl::Status IterativeGentlemanSande(
    std::vector<ModularInt>& coeffs, int log_len,
    const NttParameters<ModularInt>& ntt_params,
    const typename ModularInt::Params& mod_params) {
  int index_psi_inv = 0;
  for (int i = 0; i < log_len; i++) {
    const int half_m = 1 << i;
    const int m = half_m << 1;
    for (int k = 0; k < coeffs.size(); k += m) {
      if (index_psi_inv >= ntt_params.psis_inv_bitrev_constant.size()) {
        return absl::InvalidArgumentError("Not enough psis provided.");
      }
      const auto& [psi_inv_constant, psi_inv_constant_barrett] =
          ntt_params.psis_inv_bitrev_constant[index_psi_inv];
      for (int j = 0; j < half_m; j++) {
        // The Gentleman-Sande butterfly operation.
        if (k + j + half_m >= coeffs.size()) {
          return absl::InvalidArgumentError(
              "Vector too short for applying iterative Gentleman-Sande.");
        }
        ModularInt t = coeffs[k + j + half_m];
        ModularInt u = coeffs[k + j];
        coeffs[k + j].AddInPlace(t, &mod_params);
        // Since the input to MulConstantInPlace can be in [0, 2*modulus], we
        // only perform a lazy subtraction.
        coeffs[k + j + half_m] = std::move(
            u.LazySubInPlace(t, &mod_params)
                .MulConstantInPlace(psi_inv_constant, psi_inv_constant_barrett,
                                    &mod_params));
      }
      index_psi_inv++;
    }
  }
  return absl::OkStatus();
}

// Performs the forward NTT transformation from R_q = Z[X]/(q, X^N+1) to Z_q^N
// in-place, where q is a prime and N is a power of two such q == 1 (mod 2N).
// The input polynomial is represented by its coefficient vector `coeffs`.
template <typename ModularInt>
absl::Status ForwardNumberTheoreticTransform(
    std::vector<ModularInt>& coeffs,
    const NttParameters<ModularInt>& ntt_params,
    const typename ModularInt::Params& mod_params) {
  // Check to ensure that the coefficient vector is of the correct length.
  int log_len = log2(coeffs.size());
  int expected_len = 1 << log_len;
  if (coeffs.size() != expected_len) {
    return absl::InvalidArgumentError(absl::StrCat(
        "`coeffs` must have contain ", expected_len, " elements."));
  }

  // This is a simple wrapper of the Cooley-Tukey FFT impl for now, but we may
  // consider optimized implementation using different hardware acceleration
  // in the future.
  IterativeCooleyTukey(coeffs, log_len, ntt_params, mod_params);
  return absl::OkStatus();
}

// Performs the backward NTT transformation from Z_q^N to R_q = Z[X]/(q, X^N+1)
// in-place, where q is a prime and N is a power of two such q == 1 (mod 2N).
// The coefficients of the output polynomial is written to `coeffs`.
template <typename ModularInt>
absl::Status InverseNumberTheoreticTransform(
    std::vector<ModularInt>& coeffs,
    const NttParameters<ModularInt>& ntt_params,
    const typename ModularInt::Params& mod_params) {
  int log_len = log2(coeffs.size());
  int expected_len = 1 << log_len;
  if (coeffs.size() != expected_len) {
    return absl::InvalidArgumentError(absl::StrCat(
        "`coeffs` must have contain ", expected_len, " elements."));
  }

  RLWE_RETURN_IF_ERROR(
      IterativeGentlemanSande(coeffs, log_len, ntt_params, mod_params));
  // Normalize the result by multiplying by the inverse of n.
  ModularInt n_inv = ntt_params.n_inv_ptr.value();
  for (auto& coeff : coeffs) {
    coeff.MulInPlace(n_inv, &mod_params);
  }
  return absl::OkStatus();
}

// Performs the special log_len iterations of the Cooley-Tukey butterfly on
// `coeffs` in-place, which has length 2^log_len.
// This computes the DFT transform phi: coeffs -> {coeffs(psi^(4k+1))}_j
// where coeffs are coefficients of a polynomial of degree 2^log_len - 1, and
// {psi_j}_j are primitive 2N'th roots of unity for N = 2^(log_len + 1), i.e.
// psi = exp(PI * I / (2N)).
absl::Status IterativeHalfCooleyTukey(
    std::vector<std::complex<double>>& coeffs,
    const std::vector<std::complex<double>>& psis_bitrev);

// Performs the special log_len iterations of the Gentleman-Sande butterfly on
// `coeffs` in-place, which has length 2^log_len.
// This computes the inverse of the transform phi, where phi is the DFT
// phi: coeffs -> {coeffs(psi_j)}_{j = 4*k+1}.
absl::Status IterativeHalfGentlemanSande(
    std::vector<std::complex<double>>& coeffs,
    const std::vector<std::complex<double>>& psis_bitrev_inv);

}  // namespace rlwe

#endif  // RLWE_DFT_TRANSFORMATIONS_H_
