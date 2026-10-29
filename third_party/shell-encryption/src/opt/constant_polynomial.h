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

#ifndef RLWE_OPT_CONSTANT_POLYNOMIAL_H_
#define RLWE_OPT_CONSTANT_POLYNOMIAL_H_

#include <vector>

#include "absl/status/status.h"
#include "statusor.h"

namespace rlwe {

// Forward declaration of a Polynomial.
template <typename ModularInt>
class Polynomial;

// This class defines a constant polynomial, which cannot be operated on, but
// enable to speed up polynomial multiplications.
template <typename ModularInt>
class ConstantPolynomial {
 public:
  using Int = typename ModularInt::Int;

  // Delete default constructor.
  ConstantPolynomial() = delete;

  // Factory function to create a ConstantPolynomial.
  static StatusOr<ConstantPolynomial> Create(
      std::vector<Int> constant, std::vector<Int> constant_barrett) {
    if (constant.size() != constant_barrett.size()) {
      return absl::InvalidArgumentError(
          "The vectors of Int do not have the same size.");
    }
    return ConstantPolynomial(std::move(constant), std::move(constant_barrett));
  }

  // Get the length.
  size_t Len() const { return coeffs_constant_.size(); }

 private:
  // Private constructor.
  ConstantPolynomial(std::vector<Int> constant,
                     std::vector<Int> constant_barrett)
      : coeffs_constant_(std::move(constant)),
        coeffs_constant_barrett_(std::move(constant_barrett)) {}

  // Enable MulConstantInPlace() and FusedMulConstantAddInPlace() inside
  // Polynomial to access internal members.
  friend absl::Status Polynomial<ModularInt>::MulConstantInPlace(
      const ConstantPolynomial<ModularInt>& that,
      const typename ModularInt::Params* modular_params);
  friend absl::Status Polynomial<ModularInt>::FusedMulConstantAddInPlace(
      const Polynomial<ModularInt>& a, const ConstantPolynomial<ModularInt>& b,
      const typename ModularInt::Params* modular_params);

  // Constant private members only usable from the Polynomial class.
  const std::vector<Int> coeffs_constant_;
  const std::vector<Int> coeffs_constant_barrett_;
};

}  // namespace rlwe

#endif  // RLWE_OPT_CONSTANT_POLYNOMIAL_H_
