/*
 * Copyright 2021 Google LLC.
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

#ifndef RLWE_OPT_LAZY_POLYNOMIAL_H_
#define RLWE_OPT_LAZY_POLYNOMIAL_H_

#include <algorithm>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "status_macros.h"
#include "statusor.h"

namespace rlwe {

// Forward declaration of a Polynomial.
template <typename ModularInt>
class Polynomial;

// This class defines a `lazy` polynomial, which stores its coefficients as
// BigInt, and only reduces them when they reach the maximal value a BigInt can
// hold.
//
// These lazy polynomials can only be operated on by fused multiply add
// operation.
template <typename ModularInt, typename BigInt>
class LazyPolynomial {
 public:
  using Int = typename ModularInt::Int;

  // Factory function to create a LazyPolynomial.
  static StatusOr<LazyPolynomial> Create(
      const std::vector<ModularInt>& a, const std::vector<ModularInt>& b,
      const typename ModularInt::Params* params) {
    if (a.size() != b.size()) {
      return absl::InvalidArgumentError(
          "The polynomials are not all of the same size.");
    }
    RLWE_ASSIGN_OR_RETURN(int log_maximum_level,
                          ComputeLogMaximalLevel(params));

    // Let's multiply the coefficients by r_mod_modulus.
    std::vector<BigInt> coeffs;
    coeffs.reserve(a.size());
    for (size_t i = 0; i < a.size(); i++) {
      coeffs.push_back(static_cast<BigInt>(a[i].GetMontgomeryRepresentation()) *
                       b[i].GetMontgomeryRepresentation());
    }
    return LazyPolynomial(std::move(coeffs),
                          /*current_level=*/static_cast<BigInt>(1),
                          static_cast<BigInt>(1) << log_maximum_level);
  }

  // Factory function to create an empty lazy polynomial.
  static StatusOr<LazyPolynomial> CreateEmpty(
      size_t len, const typename ModularInt::Params* params) {
    RLWE_ASSIGN_OR_RETURN(int log_maximum_level,
                          ComputeLogMaximalLevel(params));
    std::vector<BigInt> coeffs(len, 0);
    return LazyPolynomial(std::move(coeffs),
                          /*current_level=*/static_cast<BigInt>(0),
                          static_cast<BigInt>(1) << log_maximum_level);
  }

  // Transform a LazyPolynomial into a Polynomial.
  Polynomial<ModularInt> Export(const typename ModularInt::Params* params) {
    std::vector<ModularInt> coefficients;
    coefficients.reserve(coeffs_.size());
    for (size_t i = 0; i < coeffs_.size(); i++) {
      // A lazy polynomial has coefficients multiplied by r_mod_modulus, so it
      // needs to be multiplied by the inverse of r.
      coefficients.emplace_back(
          params->ExportInt(params->BarrettReduceBigInt(coeffs_[i])));
    }
    return Polynomial<ModularInt>(std::move(coefficients));
  }

  absl::Status FusedMulAddInPlace(const std::vector<ModularInt>& a,
                                  const std::vector<ModularInt>& b,
                                  const typename ModularInt::Params* params) {
    if (a.size() != b.size() || a.size() != coeffs_.size()) {
      return absl::InvalidArgumentError(
          "The polynomials are not all of the same size.");
    }

    if (current_level_ == maximum_level_) {
      Refresh(params);
    }

    for (size_t i = 0; i < a.size(); i++) {
      coeffs_[i] += static_cast<BigInt>(a[i].GetMontgomeryRepresentation()) *
                    b[i].GetMontgomeryRepresentation();
    }

    current_level_++;

    return absl::OkStatus();
  }

 private:
  // Private constructor.
  LazyPolynomial(std::vector<BigInt> coeffs, BigInt current_level,
                 BigInt maximum_level)
      : coeffs_(std::move(coeffs)),
        current_level_(current_level),
        maximum_level_(maximum_level) {}

  // Reduce the coefficients using the provided modulus parameters.
  void Refresh(const typename ModularInt::Params* params) {
    for (auto& coeff : coeffs_) {
      coeff = params->BarrettReduceBigInt(coeff);
    }
    current_level_ = static_cast<BigInt>(1);
  }

  static StatusOr<int> ComputeLogMaximalLevel(
      const typename ModularInt::Params* params) {
    // Let's compute the maximal level: after multiplication of two numbers
    // the coefficients of a lazy polynomial will be
    // < 2 * modulus < 2^(2 * log_modulus). LazyPolynomial can only be added
    // to vectors with coefficients < 2^(2 * log_modulus), resulting to a vector
    // with coefficients 2^(2 * log_modulus + 1), and so on. The maximum level,
    // at which we need to reduce the coefficients of the lazy polynomial, is
    // 2^(log_modulus+maximum_level) = 2^(bitsize(BigInt) - 1).
    int log_maximum_level =
        params->bitsize_bigint - 1 - 2 * params->log_modulus;
    if (log_maximum_level < 1) {
      return absl::InvalidArgumentError(absl::StrCat(
          "The current parameters do not allow for lazy polynomials, as the "
          "logarithm of the maximum level, ",
          log_maximum_level, " is stricly smaller than 1."));
    }
    return log_maximum_level;
  }

  std::vector<BigInt> coeffs_;
  BigInt current_level_;
  BigInt maximum_level_;
};

}  // namespace rlwe

#endif  // RLWE_OPT_LAZY_POLYNOMIAL_H_
