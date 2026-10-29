/*
 * Copyright 2017 Google LLC.
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

#ifndef RLWE_POLYNOMIAL_H_
#define RLWE_POLYNOMIAL_H_

#include <cmath>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "constants.h"
#include "dft_transformations.h"
#include "dft_transformations_hwy.h"
#include "ntt_parameters.h"
#include "opt/constant_polynomial.h"
#include "prng/prng.h"
#include "serialization.pb.h"
#include "status_macros.h"
#include "statusor.h"

namespace rlwe {

// A polynomial in NTT form. The length of the polynomial must be a power of 2.
template <typename ModularInt>
class Polynomial {
  using ModularIntParams = typename ModularInt::Params;

 public:
  // Default constructor.
  Polynomial() = default;

  // Copy constructor.
  Polynomial(const Polynomial& p) = default;
  Polynomial& operator=(const Polynomial& that) = default;

  // Basic constructor.
  explicit Polynomial(std::vector<ModularInt> poly_coeffs)
      : log_len_(log2(poly_coeffs.size())), coeffs_(std::move(poly_coeffs)) {}

  // Create an empty polynomial of the specified length. The length must be
  // a power of 2.
  explicit Polynomial(int len, const ModularIntParams* params)
      : Polynomial(
            std::vector<ModularInt>(len, ModularInt::ImportZero(params))) {}

  // This is an implementation of the FFT from [Sei18, Sec. 2].
  // [Sei18] https://eprint.iacr.org/2018/039
  // For any polynomial f(X) in R_q = Z[X]/(q, X^n+1) for n a power of two and q
  // a prime modulus such that q == 1 (mod 2n), the NTT form of f(X) is the
  // vector
  //   NTT(f) = (f(psi), f(psi^3), .., f(psi^(n/2-1))),
  // where psi is a primitive 2n-th root of unity modulo q. By Chinese Remainder
  // Theorem, the map f -> NTT(f) is an isomorphism between R_q and Z_q^n, and
  // it can be computed using FFT (in bit reversed order).
  // For details see dft_transformations.h.
  static Polynomial ConvertToNtt(std::vector<ModularInt> poly_coeffs,
                                 const NttParameters<ModularInt>* ntt_params,
                                 const ModularIntParams* modular_params) {
    if (!ForwardNumberTheoreticTransform(poly_coeffs, *ntt_params,
                                         *modular_params)
             .ok()) {
      // An error value.
      return Polynomial();
    }
    return Polynomial(std::move(poly_coeffs));
  }

  static rlwe::StatusOr<Polynomial> ConvertToNttFast(
      std::vector<ModularInt> poly_coeffs,
      const NttParameters<ModularInt>* ntt_params,
      const ModularIntParams* modular_params) {
    RLWE_RETURN_IF_ERROR(internal::ForwardNumberTheoreticTransformHwy(
        poly_coeffs, *ntt_params, *modular_params));
    return Polynomial(std::move(poly_coeffs));
  }

  // Deprecated ConvertToNtt function taking NttParameters by constant reference
  ABSL_DEPRECATED("Use ConvertToNtt function with NttParameters pointer above.")
  static Polynomial ConvertToNtt(std::vector<ModularInt> poly_coeffs,
                                 const NttParameters<ModularInt>& ntt_params,
                                 const ModularIntParams* modular_params) {
    return ConvertToNtt(std::move(poly_coeffs), &ntt_params, modular_params);
  }

  // The inverse NTT transform is computed similarly by iteratively inverting
  // the NTT representation. For instance, using the same notation as above,
  //    f'_i + f''_i = 2f_i and  psi^(-n/2)*(f'_i-f''_i) = 2c_(n/2+i).
  //
  // For details see dft_transformations.h.
  //
  // Returns a copy of the polynomial on errors.
  std::vector<ModularInt> InverseNtt(
      const NttParameters<ModularInt>* ntt_params,
      const ModularIntParams* modular_params) const {
    if (ntt_params == nullptr || modular_params == nullptr ||
        !ntt_params->n_inv_ptr.has_value()) {
      return coeffs_;
    }

    std::vector<ModularInt> coeffs_copy = coeffs_;
    if (!InverseNumberTheoreticTransform(coeffs_copy, *ntt_params,
                                         *modular_params)
             .ok()) {
      return coeffs_;
    }

    return coeffs_copy;
  }

  // Deprecated InverseNtt function taking NttParameters by constant reference
  ABSL_DEPRECATED("Use InverseNtt function with NttParameters pointer above.")
  std::vector<ModularInt> InverseNtt(
      const NttParameters<ModularInt>& ntt_params,
      const ModularIntParams* modular_params) const {
    return InverseNtt(&ntt_params, modular_params);
  }

  // Specifies whether the Polynomial is valid.
  bool IsValid() const { return !coeffs_.empty(); }

  // Scalar multiply.
  rlwe::StatusOr<Polynomial> Mul(const ModularInt& scalar,
                                 const ModularIntParams* modular_params) const {
    Polynomial output = *this;
    RLWE_RETURN_IF_ERROR(output.MulInPlace(scalar, modular_params));
    return output;
  }

  // Scalar multiply in place.
  absl::Status MulInPlace(const ModularInt& scalar,
                          const ModularIntParams* modular_params) {
    return ModularInt::BatchMulInPlace(&coeffs_, scalar, modular_params);
  }

  // Coordinate-wise multiplication.
  rlwe::StatusOr<Polynomial> Mul(const Polynomial& that,
                                 const ModularIntParams* modular_params) const {
    Polynomial output = *this;
    RLWE_RETURN_IF_ERROR(output.MulInPlace(that, modular_params));
    return output;
  }

  // Coordinate-wise multiplication in place.
  absl::Status MulInPlace(const Polynomial& that,
                          const ModularIntParams* modular_params) {
    return ModularInt::BatchMulInPlace(&coeffs_, that.coeffs_, modular_params);
  }

  // Fused Multiply Add in place: this += a * b.
  absl::Status FusedMulAddInPlace(const Polynomial& a, const Polynomial& b,
                                  const ModularIntParams* modular_params) {
    return ModularInt::BatchFusedMulAddInPlace(&coeffs_, a.coeffs_, b.coeffs_,
                                               modular_params);
  }

  // Fused Multiply Add in place, where the multiplication is with a constant
  // polynomial.
  absl::Status FusedMulConstantAddInPlace(
      const Polynomial& a, const ConstantPolynomial<ModularInt>& b,
      const ModularIntParams* modular_params) {
    return ModularInt::BatchFusedMulConstantAddInPlace(
        &coeffs_, a.coeffs_, b.coeffs_constant_, b.coeffs_constant_barrett_,
        modular_params);
  }

  // Negation.
  Polynomial Negate(const ModularIntParams* modular_params) const {
    Polynomial output = *this;
    output.NegateInPlace(modular_params);
    return output;
  }

  // Negation in place.
  Polynomial& NegateInPlace(const ModularIntParams* modular_params) {
    for (auto& coeff : coeffs_) {
      coeff.NegateInPlace(modular_params);
    }
    return *this;
  }

  // Coordinate-wise addition.
  rlwe::StatusOr<Polynomial> Add(const Polynomial& that,
                                 const ModularIntParams* modular_params) const {
    Polynomial output = *this;
    RLWE_RETURN_IF_ERROR(output.AddInPlace(that, modular_params));
    return output;
  }

  // Coordinate-wise substraction.
  rlwe::StatusOr<Polynomial> Sub(const Polynomial& that,
                                 const ModularIntParams* modular_params) const {
    Polynomial output = *this;
    RLWE_RETURN_IF_ERROR(output.SubInPlace(that, modular_params));
    return output;
  }

  // Coordinate-wise addition in place.
  absl::Status AddInPlace(const Polynomial& that,
                          const ModularIntParams* modular_params) {
    return ModularInt::BatchAddInPlace(&coeffs_, that.coeffs_, modular_params);
  }

  // Coordinate-wise substraction in place.
  absl::Status SubInPlace(const Polynomial& that,
                          const ModularIntParams* modular_params) {
    return ModularInt::BatchSubInPlace(&coeffs_, that.coeffs_, modular_params);
  }

  // Substitute: Given an Polynomial representing p(x), returns an
  // Polynomial representing p(x^power). Power must be an odd non-negative
  // integer less than 2 * Len().
  rlwe::StatusOr<Polynomial> Substitute(
      const int power, const NttParameters<ModularInt>* ntt_params,
      const ModularIntParams* modulus_params) const {
    // The NTT representation consists in the evaluations of the polynomial at
    // roots psi^brv[n/2], psi^brv[n/2+1], ..., psi^brv[n/2+n/2-1],
    //       psi^(n/2+brv[n/2+1]), ...,         psi^(n/2+brv[n/2+n/2-1]).
    // Let f(x) be the original polynomial, and out(x) be the polynomial after
    // the substitution. Note that (psi^i)^power = psi^{(i * power) % (2 * n).
    if (0 > power || (power % 2) == 0 || power >= 2 * Len()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Substitution power must be a non-negative odd "
                       "integer less than 2*n."));
    }

    Polynomial out = *this;

    // Get the index of the psi^power evaluation
    int psi_power_index = (power - 1) / 2;
    // Update the coefficients one by one: remember that they are stored in
    // bitreversed order.
    for (int i = 0; i < Len(); i++) {
      if (ntt_params->bitrevs[psi_power_index] >= coeffs_.size()) {
        return absl::InternalError(absl::StrFormat(
            "Index %d out-of-bounds in coeffs_ of size %d.",
            ntt_params->bitrevs[psi_power_index], coeffs_.size()));
      }
      out.coeffs_[ntt_params->bitrevs[i]] =
          coeffs_[ntt_params->bitrevs[psi_power_index]];
      // Each time the index increases by 1, the psi_power_index increases by
      // power mod the length.
      psi_power_index = (psi_power_index + power) % Len();
    }

    return out;
  }

  // Deprecated Substitute function taking NttParameters by constant reference
  ABSL_DEPRECATED("Use Substitute function with NttParameters pointer above.")
  rlwe::StatusOr<Polynomial> Substitute(
      const int power, const NttParameters<ModularInt>& ntt_params,
      const ModularIntParams* modulus_params) const {
    return Substitute(power, &ntt_params, modulus_params);
  }

  // Boolean comparison.
  bool operator==(const Polynomial& that) const {
    if (Len() != that.Len()) {
      return false;
    }

    for (int i = 0; i < Len(); i++) {
      if (coeffs_[i] != that.coeffs_[i]) {
        return false;
      }
    }

    return true;
  }
  bool operator!=(const Polynomial& that) const { return !(*this == that); }

  int LogLen() const { return log_len_; }
  int Len() const { return coeffs_.size(); }

  // Accessor for coefficients.
  const std::vector<ModularInt>& Coeffs() const { return coeffs_; }

  rlwe::StatusOr<SerializedNttPolynomial> Serialize(
      const ModularIntParams* modular_params) const {
    SerializedNttPolynomial output;
    RLWE_ASSIGN_OR_RETURN(*(output.mutable_coeffs()),
                          ModularInt::SerializeVector(coeffs_, modular_params));
    output.set_num_coeffs(coeffs_.size());
    return output;
  }

  static rlwe::StatusOr<Polynomial> Deserialize(
      const SerializedNttPolynomial& serialized,
      const ModularIntParams* modular_params) {
    if (serialized.num_coeffs() <= 0) {
      return absl::InvalidArgumentError(
          "Number of serialized coefficients must be positive.");
    } else if (serialized.num_coeffs() > static_cast<int>(kMaxNumCoeffs)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Number of serialized coefficients, ", serialized.num_coeffs(),
          ", must be less than ", kMaxNumCoeffs, "."));
    }
    Polynomial output(serialized.num_coeffs(), modular_params);
    RLWE_ASSIGN_OR_RETURN(
        output.coeffs_,
        ModularInt::DeserializeVector(serialized.num_coeffs(),
                                      serialized.coeffs(), modular_params));
    return output;
  }

  // Compute a ConstantPolynomial representation of the coefficients for faster
  // multiplication.
  StatusOr<ConstantPolynomial<ModularInt>> ComputeConstantRepresentation(
      const ModularIntParams* modular_params) const {
    std::vector<typename ModularInt::Int> coeffs_constant,
        coeffs_constant_barrett;
    coeffs_constant.reserve(coeffs_.size());
    coeffs_constant_barrett.reserve(coeffs_.size());
    for (const ModularInt& coeff : coeffs_) {
      const auto [constant, constant_barrett] =
          coeff.GetConstant(modular_params);
      coeffs_constant.push_back(constant);
      coeffs_constant_barrett.push_back(constant_barrett);
    }
    return ConstantPolynomial<ModularInt>::Create(
        std::move(coeffs_constant), std::move(coeffs_constant_barrett));
  }

  // Coordinate-wise multiplication by a constant polynomial.
  rlwe::StatusOr<Polynomial> MulConstant(
      const ConstantPolynomial<ModularInt>& that,
      const ModularIntParams* modular_params) const {
    Polynomial output(*this);
    RLWE_RETURN_IF_ERROR(output.MulConstantInPlace(that, modular_params));
    return output;
  }

  // Coordinate-wise multiplication in place by a constant polynomial.
  absl::Status MulConstantInPlace(const ConstantPolynomial<ModularInt>& that,
                                  const ModularIntParams* modular_params) {
    return ModularInt::BatchMulConstantInPlace(&coeffs_, that.coeffs_constant_,
                                               that.coeffs_constant_barrett_,
                                               modular_params);
  }

 private:
  // Instance variables.
  size_t log_len_;
  std::vector<ModularInt> coeffs_;
};

template <typename ModularInt, typename Prng = rlwe::SecurePrng>
rlwe::StatusOr<Polynomial<ModularInt>> SamplePolynomialFromPrng(
    int num_coeffs, Prng* prng,
    const typename ModularInt::Params* modulus_params) {
  // Sample a from the uniform distribution. Since a is uniformly distributed,
  // it can be generated directly in NTT form since the NTT transformation is
  // an automorphism.
  if (num_coeffs < 1) {
    return absl::InvalidArgumentError(
        "SamplePolynomialFromPrng: number of coefficients must be a "
        "non-negative integer.");
  }
  std::vector<ModularInt> a_ntt_coeffs;
  a_ntt_coeffs.reserve(num_coeffs);
  for (int i = 0; i < num_coeffs; i++) {
    RLWE_ASSIGN_OR_RETURN(ModularInt coefficient,
                          ModularInt::ImportRandom(prng, modulus_params));
    a_ntt_coeffs.push_back(std::move(coefficient));
  }
  return Polynomial<ModularInt>(std::move(a_ntt_coeffs));
}

}  // namespace rlwe

#endif  // RLWE_POLYNOMIAL_H_
