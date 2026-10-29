/*
 * Copyright 2018 Google LLC.
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

// The constants in this file represent an expected bound on the size of certain
// NTT polynomials. The size is defined as the l-infinity norm over all
// coefficients, in other words, the size of the largest coefficient. Each bound
// is chosen to be 6 * sqrt(V), where V is the NTT coefficients' variance. Even
// after union-bounding over all N coefficients, this provides a
// high-probability bound for the l-infinity norm of the NTT polynomial.

#ifndef RLWE_ERROR_H_
#define RLWE_ERROR_H_

#include <cmath>

#include "montgomery.h"
#include "ntt_parameters.h"
#include "statusor.h"

namespace rlwe {

// A class that stores the error constants. This class only accurate computes
// error when the plaintext modulus is sufficiently small (less than 64 bits).
template <typename ModularInt>
class ErrorParams {
 public:
  static rlwe::StatusOr<ErrorParams> Create(
      const int log_t, Uint64 variance,
      const typename ModularInt::Params* params,
      const rlwe::NttParameters<ModularInt>* ntt_params) {
    if (log_t > static_cast<int>(params->log_modulus) - 1) {
      return absl::InvalidArgumentError(
          absl::StrCat("The value log_t, ", log_t,
                       ", must be smaller than log_modulus - 1, ",
                       params->log_modulus - 1, "."));
    } else if (log_t <= 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("The value log_t, ", log_t, ", must be positive."));
    }
    if (variance > kMaxVariance) {
      return absl::InvalidArgumentError(absl::StrCat(
          "The variance, ", variance, ", must be at most ", kMaxVariance, "."));
    }
    return ErrorParams(log_t, variance, params, ntt_params);
  }

  // Accessors for constants.
  double B_plaintext() const { return b_plaintext_; }
  double B_encryption() const { return b_encryption_; }
  double B_scale() const { return b_scale_; }

  // A polynomial consisting of error terms is added to the ciphertext during
  // relinearization. The noise of a ciphertext increases additively by the size
  // of the polynomial, which depends on the decomposition modulus of the
  // key-switching matrix and the number of ciphertext components applied on.
  double B_relinearize(int num_components,
                       int log_decomposition_modulus) const {
    // The number of digits needed to represent integers mod modulus in base
    // decomposition modulus.
    int num_digits = (log_decomposition_modulus + log_modulus_ - 1) /
                     log_decomposition_modulus;
    double decomposition_modulus = static_cast<double>(
        static_cast<typename ModularInt::Int>(1) << log_decomposition_modulus);
    return (8.0 / sqrt(3.0)) * ExportDoubleT() * num_digits * sigma_ *
           dimension_ * decomposition_modulus * num_components;
  }

  // Returns the "size" of the error term introduced during relinearization
  // using the auxiliary modulus technique. The error term can be expressed as
  // p^(-1) * (t * sum(ei * ci, i=1..k) + d0 + s * d1), where p is the auxiliary
  // modulus, ci is a ciphertext component wrt modulus q, ei is a fresh error
  // term in the relinearization key, k = `num_components` is the number of key
  // components of a relinearization key, s is the secret key polynomial, and
  // d0, d1 are small conversion errors.
  // We bound the coefficients of c_i by the ciphertext modulus q, and d0, d1
  // by t * p; thus in the NTT domain, the norm of the entire error term is
  // bounded by t * (N * q / p * 6 * sigma * k + sqrt(N) * 6 * sigma).
  double B_aux_mod_relinearize(
      int num_components,
      const typename ModularInt::Params& mod_params_aux) const {
    double p =
        static_cast<double>(ModularInt::ExportUInt64(mod_params_aux.modulus));
    double f = std::pow(2, log_modulus_);
    f /= p;
    return ExportDoubleT() * 6 * sigma_ *
           (dimension_ * f * num_components + sqrt(dimension_));
  }

  // This represents the "size" of a freshly encrypted ciphertext using a public
  // key, where the public key's error term, the public-key encryption's random
  // element and error terms are all sampled from a centered binomial
  // distribution with the specified standard deviation `sigma`. The error in a
  // fresh public-key encryption is t * (v * e + e' + s * e''), where s, v, e,
  // e', e'' are all sampled from the same error distribution of variance
  // sigma^2. In the NTT domain, the norm of this error term is bounded by t *
  // (72 * N * sigma^2 + 6 * sqrt(N) * sigma). Then adding the bound on the
  // message t * sqrt(3 * N) and we get the bound on the error and message.
  rlwe::StatusOr<double> B_publickey_encryption(int dimension,
                                                int variance) const {
    if (variance <= 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("The variance, ", variance, ", must be positive."));
    }
    double sigma = sqrt(static_cast<double>(variance));
    return ExportDoubleT() * (sqrt(dimension) * (6 * sigma + sqrt(3)) +
                              72 * dimension * variance);
  }

 private:
  // Constructor to set up the params.
  ErrorParams(const int log_t, Uint64 variance,
              const typename ModularInt::Params* params,
              const rlwe::NttParameters<ModularInt>* ntt_params)
      : t_(params->One()) {
    t_ = (params->One() << log_t) + params->One();
    dimension_ = ntt_params->number_coeffs;
    sigma_ = sqrt(variance);
    log_modulus_ = params->log_modulus;

    // Set error constants.
    b_plaintext_ = B_plaintext(dimension_);
    b_encryption_ = B_encryption(dimension_, sigma_);
    b_scale_ = B_scale(dimension_);
  }

  // This represents the "size" of an NTT coefficient of a randomly sampled
  // plaintext polynomial. The ciphertext error grows multiplicatively by this
  // constant under an absorb. Assume the plaintext polynomial has coefficients
  // chosen uniformly at random from the range [0, t), where t is the plaintext
  // modulus. Then the variance of a coefficient is  V = t ^ 2 / 12. In the NTT
  // domain, the variance is (dimension * t ^ 2 / 12).
  double B_plaintext(int dimension) {
    // Return 6 * sqrt(V) where V is the variance of a coefficient in the NTT
    // domain.
    return ExportDoubleT() * sqrt(3.0 * dimension);
  }

  // This represents the "size" of a freshly encrypted ciphertext with a secret
  // key and error sampled from a centered binomial distribution with the
  // specified variance. The error and message have size |m + et|. Like
  // B_plaintext, the variance of a coefficient of m is V = t ^ 2 / 12, and the
  // variance of a coefficient of e is sigma ^ 2. In the NTT domain we can bound
  // the coefficient's variance by (dimension * (t ^ 2 / 12 + t * sigma)). The
  // bound 6 * sqrt(V) is then t * sqrt(dimension) * (sqrt(3.0) + 6.0 * sigma).
  double B_encryption(int dimension, double sigma) {
    return ExportDoubleT() * sqrt(dimension) * (sqrt(3.0) + 6.0 * sigma);
  }

  // When modulus switching a ciphertext from a modulus q to a smaller modulus
  // p, the polynomial is scaled by (p / q) and a small rounding polynomial is
  // added so that the result is the closest integer polynomial with c' = c mod
  // t. The rounding polynomial's size contributes additively to the ciphertext
  // error, and its size is given by this constant.
  double B_scale(int dimension) {
    return ExportDoubleT() *
           (sqrt(3.0 * dimension) + 8.0 * dimension * sqrt(1 / 3.0));
  }

  // This returns the least 64 bits of t_. If t_ is much larger than 64 bits,
  // this will return inaccurate error estimates.
  double ExportDoubleT() const {
    return static_cast<double>(ModularInt::ExportUInt64(t_));
  }

  double b_plaintext_;
  double b_encryption_;
  double b_scale_;
  int log_modulus_;
  typename ModularInt::Int t_;
  int dimension_;
  double sigma_;
};

}  //  namespace rlwe

#endif  // RLWE_ERROR_H_
