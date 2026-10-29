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

// Defines types that are necessary for Ring-Learning with Errors (rlwe)
// encryption.

#ifndef RLWE_MONTGOMERY_H_
#define RLWE_MONTGOMERY_H_

#include <cmath>
#include <cstdint>
#include <tuple>
#include <vector>

#include <glog/logging.h>
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "bits_util.h"
#include "constants.h"
#include "int256.h"
#include "prng/prng.h"
#include "serialization.pb.h"
#include "status_macros.h"
#include "statusor.h"
#include "third_party/shell-encryption/base/shell_encryption_export.h"
#include "third_party/shell-encryption/base/shell_encryption_export_template.h"

namespace rlwe {

namespace internal {

// Struct to capture the "bigger int" type.
template <typename T>
struct BigInt;
// Specialization for uint8, uint16, uint32, uint64, and uint128.
template <>
struct BigInt<Uint8> {
  typedef Uint16 value_type;
};
template <>
struct BigInt<Uint16> {
  typedef Uint32 value_type;
};
template <>
struct BigInt<Uint32> {
  typedef Uint64 value_type;
};
template <>
struct BigInt<Uint64> {
  typedef absl::uint128 value_type;
};
template <>
struct BigInt<absl::uint128> {
  typedef uint256 value_type;
};
#ifdef ABSL_HAVE_INTRINSIC_INT128
template <>
struct BigInt<unsigned __int128> {
  typedef uint256 value_type;
};
#endif

}  // namespace internal

// The parameters necessary for a Montgomery integer. Note that the template
// parameters ensure that T is an unsigned integral of at least 8 bits.
template <typename T>
struct MontgomeryIntParams {
  // Expose Int and its greater type. BigInt is required in order to multiply
  // two Int and ensure that no overflow occurs.
  //
  // Thread safe.
  using Int = T;
  using BigInt = typename internal::BigInt<Int>::value_type;
  static const size_t bitsize_int = sizeof(Int) * 8;
  static const size_t bitsize_bigint = sizeof(BigInt) * 8;

  // Factory function to create MontgomeryIntParams.
  static rlwe::StatusOr<std::unique_ptr<const MontgomeryIntParams>> Create(
      Int modulus);

  // The value R to be used in Montgomery multiplication. R will be selected as
  // 2^bitsize(Int) and hence automatically verifies R > modulus.
  const BigInt r = static_cast<BigInt>(1) << bitsize_int;

  // The modulus over which these modular operations are being performed.
  const Int modulus;

  // The modulus over which these modular operations are being performed, cast
  // as a BigInt.
  const BigInt modulus_bigint;

  // The number of bits in the modulus.
  const unsigned int log_modulus;

  // The value of R taken modulo the modulus.
  const Int r_mod_modulus;
  const Int
      r_mod_modulus_barrett;  // = (r_mod_modulus << bitsize_int) / modulus, to
                              // speed up multiplication by r_mod_modulus.

  // The values below, inv_modulus and inv_r, satisfy the formula:
  //     R * inv_r - modulus * inv_modulus = 1
  // Note that inv_modulus is not literally the multiplicative inverse of
  // modulus modulo R.
  const Int inv_modulus;
  const Int
      inv_r;  // needed to translate from Montgomery to normal representation
  const Int inv_r_barrett;  // = (inv_r << bitsize_int) / modulus, to speed up
                            // multiplication by inv_r.

  // The numerator used in the Barrett reduction.
  const BigInt barrett_numerator;         // = 2^(sizeof(Int)*8) / modulus
  const BigInt barrett_numerator_bigint;  // = 2^(sizeof(BigInt)*8-1) / modulus

  const Int Zero() const { return 0; }
  const Int One() const { return 1; }
  const Int Two() const { return 2; }

  // Functions to perform Barrett reduction. For more details, see
  // https://en.wikipedia.org/wiki/Barrett_reduction.
  // This function should remains in the header file to avoid performance
  // regressions.
  Int BarrettReduce(Int input) const {
    Int out =
        static_cast<Int>((this->barrett_numerator * input) >> bitsize_int);
    out = input - (out * this->modulus);
    // The steps above produce an integer that is in the range [0, 2N).
    // We now reduce to the range [0, N).
    return (out >= this->modulus) ? out - this->modulus : out;
  }

  Int BarrettReduceBigInt(BigInt input) const {
    using BigBigInt = typename internal::BigInt<BigInt>::value_type;
    Int out = static_cast<Int>(
        (static_cast<BigBigInt>(this->barrett_numerator_bigint) * input) >>
        (sizeof(BigInt) * 8 - 1));
    out = static_cast<Int>(input) - (out * this->modulus);
    // The steps above produce an integer that is in the range [0, 2N).
    // We now reduce to the range [0, N).
    return (out >= this->modulus) ? out - this->modulus : out;
  }

  // Function to multiply the input by the inverse of r, transforming back from
  // Montgomery representation.
  Int ExportInt(Int input) const {
    Int out =
        static_cast<Int>((static_cast<BigInt>(this->inv_r_barrett) * input) >>
                         this->bitsize_int);
    out = input * this->inv_r - out * this->modulus;
    // The steps above produce an integer that is in the range [0, 2N).
    // We now reduce to the range [0, N).
    return (out >= this->modulus) ? out - this->modulus : out;
  }

  // Computes the serialized byte length of an integer.
  unsigned int SerializedSize() const { return (log_modulus + 7) / 8; }

  // Check whether (1 << log_n) fits into the underlying Int type.
  static bool DoesLogNFit(Uint64 log_n) { return (log_n < bitsize_int - 1); }

  // Transform an Int into a double. Note that as soon as value is more than
  // 2^53, this is a potentially lossy conversion.
  static double GetDouble(Int value) { return static_cast<double>(value); }

 private:
  MontgomeryIntParams(Int mod)
      : modulus(mod),
        modulus_bigint(static_cast<BigInt>(this->modulus)),
        log_modulus(internal::BitLength(this->modulus)),
        r_mod_modulus(static_cast<Int>(this->r % this->modulus_bigint)),
        r_mod_modulus_barrett(static_cast<Int>(
            (static_cast<BigInt>(r_mod_modulus) << bitsize_int) / modulus)),
        inv_modulus(static_cast<Int>(
            std::get<1>(MontgomeryIntParams::Inverses(modulus_bigint, r)))),
        inv_r(std::get<0>(MontgomeryIntParams::Inverses(modulus_bigint, r))),
        inv_r_barrett(static_cast<Int>(
            (static_cast<BigInt>(inv_r) << bitsize_int) / modulus)),
        barrett_numerator(this->r / this->modulus_bigint),
        barrett_numerator_bigint(
            (static_cast<BigInt>(1) << (sizeof(BigInt) * 8 - 1)) /
            this->modulus_bigint) {}

  // Computes the Montgomery inverse coefficients for r and modulus using
  // the Extended Euclidean Algorithm.
  //
  // modulus must be odd.
  // Returns a tuple of (inv_r, inv_modulus) such that:
  //     r * inv_r - modulus * inv_modulus = 1
  static std::tuple<Int, Int> Inverses(
      BigInt modulus_bigint, BigInt r);
};

// Specialization for uint128, because BigBigInt = uint512 is not available.
template <>
inline absl::uint128 MontgomeryIntParams<absl::uint128>::BarrettReduceBigInt(
    uint256 input) const {
  return static_cast<absl::uint128>(input % modulus);
}
#ifdef ABSL_HAVE_INTRINSIC_INT128
template <>
inline unsigned __int128
MontgomeryIntParams<unsigned __int128>::BarrettReduceBigInt(
    uint256 input) const {
  return static_cast<unsigned __int128>(input % modulus);
}
#endif

// Stores an integer in Montgomery representation. The goal of this
// class is to provide a static type that differentiates between integers
// in Montgomery representation and standard integers. Once a Montgomery
// integer is created, it has the * and + operators necessary to be treated
// as another integer.
// The underlying integer type T must be unsigned and must not be bool.
// This class is thread safe.
template <typename T>
class ABSL_MUST_USE_RESULT MontgomeryInt {
 public:
  // Expose Int and its greater type. BigInt is required in order to multiply
  // two Int and ensure that no overflow occurs. This should also be used by
  // external classes.
  using Int = T;
  using BigInt = typename internal::BigInt<Int>::value_type;

  // Expose the parameter type.
  using Params = MontgomeryIntParams<T>;

  // Static factory that converts a non-Montgomery representation integer, the
  // underlying integer type, into a Montgomery representation integer. Does not
  // take ownership of params. i.e., import "a".
  static rlwe::StatusOr<MontgomeryInt> ImportInt(
      Int n, const Params* params);

  // Static factory that converts a vector of a non-Montgomery representation
  // integers, the underlying integer types, into a vector of Montgomery
  // representation integer. Does not take ownership of params.
  static rlwe::StatusOr<std::vector<MontgomeryInt>> BatchImportInts(
      const std::vector<Int>& ints, const Params* params);

  // Construction given `n` in Montgomery representation. If the input in not in
  // Montgomery representation, one should use the ImportInt() function instead.
  explicit MontgomeryInt(Int n) : n_(n) {}

  // Static functions to create a MontgomeryInt of 0 and 1.
  static MontgomeryInt ImportZero(const Params* params);
  static MontgomeryInt ImportOne(const Params* params);

  // Import a random integer using entropy from specified prng. Does not take
  // ownership of params or prng.
  template <typename Prng = rlwe::SecurePrng>
  static rlwe::StatusOr<MontgomeryInt> ImportRandom(
      Prng* prng, const Params* params) {
    // In order to generate unbiased randomness, we uniformly and randomly
    // sample integers in [0, 2^params->log_modulus) until the generated integer
    // is less than the modulus (i.e., we perform rejection sampling).
    RLWE_ASSIGN_OR_RETURN(Int random_int,
                          GenerateRandomInt(params->log_modulus, prng));
    while (random_int >= params->modulus) {
      RLWE_ASSIGN_OR_RETURN(random_int,
                            GenerateRandomInt(params->log_modulus, prng));
    }
    return MontgomeryInt(random_int);
  }

  static BigInt DivAndTruncate(BigInt dividend,
                                                       BigInt divisor);

  // No default constructor.
  MontgomeryInt() = delete;

  // Default copy constructor.
  MontgomeryInt(const MontgomeryInt& that) = default;
  MontgomeryInt& operator=(const MontgomeryInt& that) = default;

  // Convert a Montgomery representation integer back to the underlying integer.
  // i.e., export "a".
  Int ExportInt(const Params* params) const { return params->ExportInt(n_); }

  // Get the Montgomery representation of the integer.
  const Int GetMontgomeryRepresentation() const { return n_; }

  // Returns the least significant 64 bits of n.
  static Uint64 ExportUInt64(Int n) { return static_cast<Uint64>(n); }

  // Serialization.
  rlwe::StatusOr<std::string> Serialize(const Params* params) const;
  static rlwe::StatusOr<std::string> SerializeVector(
      const std::vector<MontgomeryInt>& coeffs, const Params* params);

  // Deserialization.
  static rlwe::StatusOr<MontgomeryInt> Deserialize(
      absl::string_view payload, const Params* params);
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  DeserializeVector(int num_coeffs, absl::string_view serialized,
                    const Params* params);

  // Modular multiplication.
  // Perform a multiply followed by a modular reduction. Produces an output
  // in the range [0, N).
  // Taken from Hacker's Delight chapter on Montgomery multiplication.
  MontgomeryInt Mul(const MontgomeryInt& that, const Params* params) const {
    MontgomeryInt out(*this);
    return out.MulInPlace(that, params);
  }

  // This function should remains in the header file to avoid performance
  // regressions.
  MontgomeryInt& MulInPlace(const MontgomeryInt& that, const Params* params) {
    // This function computes the product of the two numbers (a and b), which
    // will equal a * R * b * R in Montgomery representation. It then performs
    // the reduction, creating a * b * R (mod N).
    BigInt product = static_cast<BigInt>(n_) * that.n_;  // a * R * b * R

    // Compute u = (product mod R) * N^(-1) (mod R). Since R = 2^bitsize(Int),
    // modulo reduction by R can be done by casting from BigInt to Int.
    Int u = static_cast<Int>(product * params->inv_modulus);

    // Compute t_msb = a * b * R (mod N).
    BigInt t = product + params->modulus_bigint * u;
    Int t_msb = static_cast<Int>(t >> Params::bitsize_int);

    // The steps above produce an integer that is in the range [0, 2N).
    // We now reduce to the range [0, N).
    n_ = (t_msb >= params->modulus) ? t_msb - params->modulus : t_msb;
    return *this;
  }

  // Modular multiplication by a constant with precomputation.
  // MontgomeryInt are stored under the form n * R, where R = 1 << bitsize_int.
  // When multiplying by a constant c, one can precompute
  //   constant = (c * R^(-1) mod modulus)
  //   constant_barrett = (constant << bitsize_int) / modulus
  // and perform modular reduction using Barrett reduction instead of
  // Montgomery multiplication.

  // The function GetConstant() returns a tuple (constant, constant_barrett):
  //       constant = ExportInt(params),
  // and
  //       constant_barrett = (constant << bitsize_int) / modulus
  std::tuple<Int, Int> GetConstant(const Params* params) const;

  MontgomeryInt MulConstant(const Int& constant, const Int& constant_barrett,
                            const Params* params) const {
    MontgomeryInt out(*this);
    return out.MulConstantInPlace(constant, constant_barrett, params);
  }

  // This function should remains in the header file to avoid performance
  // regressions.
  // The value this->n_ can be in [0, 4*modulus] since this operation is
  // actually a multiplication over a BigInt followed by a Barrett reduction.
  MontgomeryInt& MulConstantInPlace(const Int& constant,
                                    const Int& constant_barrett,
                                    const Params* params) {
    Int out = static_cast<Int>((static_cast<BigInt>(constant_barrett) * n_) >>
                               Params::bitsize_int);
    n_ = n_ * constant - out * params->modulus;
    // The steps above produce an integer that is in the range [0, 2N).
    // We now reduce to the range [0, N).
    n_ -= (n_ >= params->modulus) ? params->modulus : 0;
    return *this;
  }

  // Montgomery addition.
  MontgomeryInt Add(const MontgomeryInt& that, const Params* params) const {
    MontgomeryInt out(*this);
    return out.AddInPlace(that, params);
  }

  // This function should remains in the header file to avoid performance
  // regressions.
  // The sum of this->n_ + that.n_ needs to be in [0, 4 * modulus].
  MontgomeryInt& AddInPlace(const MontgomeryInt& that, const Params* params) {
    // We can use Barrett reduction because n_ <= modulus < Max(Int)/4.
    n_ = params->BarrettReduce(n_ + that.n_);
    return *this;
  }

  // Modular negation.
  MontgomeryInt Negate(const Params* params) const {
    return MontgomeryInt(params->modulus - n_);
  }

  // This function should remains in the header file to avoid performance
  // regressions.
  MontgomeryInt& NegateInPlace(const Params* params) {
    n_ = params->modulus - n_;
    return *this;
  }

  // Modular subtraction.
  MontgomeryInt Sub(const MontgomeryInt& that, const Params* params) const {
    MontgomeryInt out(*this);
    return out.SubInPlace(that, params);
  }

  // This function should remains in the header file to avoid performance
  // regressions.
  // The value this->n_ can be in [0, 3*modulus] and that.n_ needs to be in
  // [0, modulus].
  MontgomeryInt& SubInPlace(const MontgomeryInt& that, const Params* params) {
    // We can use Barrett reduction because n_ <= modulus < Max(Int)/4.
    n_ = params->BarrettReduce(n_ + (params->modulus - that.n_));
    return *this;
  }

  // We enable lazy additions and lazy multiplications, where the output is not
  // assured to be in [0, modulus]. A lazy addition adds the underlying
  // Montgomery integers, whereas a lazy addition adds the left hand side with
  // modulus minus the right hand side.
  // These operations can be used in place of addition and subtraction, as long
  // as a Barrett reduction is performed on the value before the MontgomeryInt
  // is used in the rest of the library.
  //
  // As an example, these operations are used when performing an NTT operation
  // on all odd layers.
  //
  // These functions should remains in the header file to avoid performance
  // regressions.

  // The sum of this->n_ + that.n_ needs to be in [0, 4 * modulus].
  MontgomeryInt& LazyAddInPlace(const MontgomeryInt& that,
                                const Params* params) {
    n_ += that.n_;
    return *this;
  }

  // The value this->n_ can be in [0, 3*modulus] and that.n_ needs to be in
  // [0, modulus].
  MontgomeryInt& LazySubInPlace(const MontgomeryInt& that,
                                const Params* params) {
    n_ += (params->modulus - that.n_);
    return *this;
  }

  MontgomeryInt& FusedMulAddInPlace(const MontgomeryInt& a,
                                    const MontgomeryInt& b,
                                    const Params* params) {
    // Compute this += a * b by using r_mod_modulus which is 1 in Montgomery
    // representation. Since a, b, and this are smaller than the modulus N, then
    // t is smaller than 2N^2 < r * N, which is the condition for Montgomery
    // reduction.
    // en.wikipedia.org/wiki/Montgomery_modular_multiplication#The_REDC_algorithm
    BigInt t = static_cast<BigInt>(params->r_mod_modulus) * n_ +
               static_cast<BigInt>(a.n_) * b.n_;
    // Perform a regular Montgomery reduction.
    Int u = static_cast<Int>(t) * params->inv_modulus;
    t += params->modulus_bigint * u;
    Int t_msb = static_cast<Int>(t >> Params::bitsize_int);

    // The steps above produce an integer that is in the range [0, 2N).
    // We now reduce to the range [0, N).
    n_ = (t_msb >= params->modulus) ? t_msb - params->modulus : t_msb;
    return *this;
  }

  MontgomeryInt& FusedMulConstantAddInPlace(const MontgomeryInt& a,
                                            const Int& constant,
                                            const Int& constant_barrett,
                                            const Params* params) {
    // Compute this += a * constant, where this is multiplied by
    // the Barrett numerator.
    // Denote:
    // - m = modulus
    // - n = this
    // - b = constant
    // - k = bitsize_int
    // - CB = constant_barrett = (b << k) / m = floor(b * 2^k/m),
    // - BN = barrett_numerator = (1 << k) / m = floor(2^k/m),
    //
    // We recall that m <= k/4. The function below will compute
    //           a * b + n - floor((CB * a + BN * n)/2^k) * m
    // and we will prove that this is smaller than 2 * m, which enables to
    // perform a unique conditional subtraction by m to reduce the final result
    // in [0, m].
    //
    // We first express the floor as a difference,
    //   a * b + n - floor((CB * a + BN * n)/2^k) * m
    //   = ab + n - m * (CB * a + BN * n - (a * CB + n * BN mod 2^k))/2^k
    // and we bound m * (a * CB + n * BN mod 2^k)/2^k by m, which yields:
    //   <= ab + n - (CB*a + BN*n) * m/2^k + m.
    //
    // We know expand CB and BN with their definition,
    //   = ab+n - m /2^k *(floor(b * 2^k/m) * a + floor(2^k/m) * n) + m
    //   = ab+n - ma/2^k *(b2^k-(b2^k mod m))/m - mn/2^k*(2^k-(2^k mod m))/m + m
    //   = ab+n - a /2^k *(b2^k-(b2^k mod m))   - n /2^k*(2^k-(2^k mod m)) + m
    //   = ab+n - a*b + a*(b2^k mod m)/2^k - n + n(2^k mod m)/2^k + m
    //   = a*(b2^k mod m)/2^k + n(2^k mod m)/2^k + m
    //
    // Finally, we note that a, n <= m, and we recall that m <= k/4. We get
    //   <= 2m^2 / 2^k + m <= 2m^2/(4m) + m <= m/2+m < 2*m
    Int out = static_cast<Int>((static_cast<BigInt>(constant_barrett) * a.n_ +
                                params->barrett_numerator * n_) >>
                               Params::bitsize_int);
    n_ += a.n_ * constant - out * params->modulus;
    n_ -= (n_ >= params->modulus) ? params->modulus : 0;
    return *this;
  }

  // Batch operations (and in-place variants) over vectors of MontgomeryInt.
  // We define two versions of the batch operations:
  // -  the first input is a vector and the second is a MontgomeryInt scalar:
  //    in that case, the scalar will be added/subtracted/multiplied with each
  //    element of the first input.
  // -  both inputs are vectors of same length: in that case, the operations
  //    will be performed component wise.
  // These batch operations may fail if the input vectors are not of the same
  // size.

  // Batch addition of two vectors.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchAdd(const std::vector<MontgomeryInt>& in1,
           const std::vector<MontgomeryInt>& in2, const Params* params);
  static absl::Status BatchAddInPlace(std::vector<MontgomeryInt>* in1,
                                      const std::vector<MontgomeryInt>& in2,
                                      const Params* params);

  // Batch addition of one vector with a scalar.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchAdd(const std::vector<MontgomeryInt>& in1, const MontgomeryInt& in2,
           const Params* params);
  static absl::Status BatchAddInPlace(std::vector<MontgomeryInt>* in1,
                                      const MontgomeryInt& in2,
                                      const Params* params);

  // Batch subtraction of two vectors.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchSub(const std::vector<MontgomeryInt>& in1,
           const std::vector<MontgomeryInt>& in2, const Params* params);
  static absl::Status BatchSubInPlace(
      std::vector<MontgomeryInt>* in1, const std::vector<MontgomeryInt>& in2,
      const Params* params);

  // Batch subtraction of one vector with a scalar.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchSub(const std::vector<MontgomeryInt>& in1, const MontgomeryInt& in2,
           const Params* params);
  static absl::Status BatchSubInPlace(
      std::vector<MontgomeryInt>* in1, const MontgomeryInt& in2,
      const Params* params);

  // Batch multiplication of two vectors.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchMul(const std::vector<MontgomeryInt>& in1,
           const std::vector<MontgomeryInt>& in2, const Params* params);
  static absl::Status BatchMulInPlace(
      std::vector<MontgomeryInt>* in1, const std::vector<MontgomeryInt>& in2,
      const Params* params);

  // Batch fused multiply add of two vectors.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchFusedMulAdd(const std::vector<MontgomeryInt>& in1,
                   const std::vector<MontgomeryInt>& in2,
                   const std::vector<MontgomeryInt>& in3, const Params* params);
  static absl::Status BatchFusedMulAddInPlace(
      std::vector<MontgomeryInt>* in1, const std::vector<MontgomeryInt>& in2,
      const std::vector<MontgomeryInt>& in3, const Params* params);

  // Batch fused multiply add of two vectors, one being constant.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchFusedMulConstantAdd(const std::vector<MontgomeryInt>& in1,
                           const std::vector<MontgomeryInt>& in2,
                           const std::vector<Int>& constant,
                           const std::vector<Int>& constant_barrett,
                           const Params* params);
  static absl::Status BatchFusedMulConstantAddInPlace(
      std::vector<MontgomeryInt>* in1, const std::vector<MontgomeryInt>& in2,
      const std::vector<Int>& constant,
      const std::vector<Int>& constant_barrett, const Params* params);

  // Batch multiplication of two vectors, where the second vector is a constant.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchMulConstant(const std::vector<MontgomeryInt>& in1,
                   const std::vector<Int>& in2_constant,
                   const std::vector<Int>& in2_constant_barrett,
                   const Params* params);
  static absl::Status BatchMulConstantInPlace(
      std::vector<MontgomeryInt>* in1, const std::vector<Int>& in2_constant,
      const std::vector<Int>& in2_constant_barrett, const Params* params);

  // Batch multiplication of a vector with a scalar.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchMul(const std::vector<MontgomeryInt>& in1, const MontgomeryInt& in2,
           const Params* params);
  static absl::Status BatchMulInPlace(
      std::vector<MontgomeryInt>* in1, const MontgomeryInt& in2,
      const Params* params);

  // Batch multiplication of a vector with a constant scalar.
  static rlwe::StatusOr<std::vector<MontgomeryInt>>
  BatchMulConstant(const std::vector<MontgomeryInt>& in1, const Int& constant,
                   const Int& constant_barrett, const Params* params);
  static absl::Status BatchMulConstantInPlace(
      std::vector<MontgomeryInt>* in1, const Int& constant,
      const Int& constant_barrett, const Params* params);

  // Equality.
  bool operator==(const MontgomeryInt& that) const { return (n_ == that.n_); }
  bool operator!=(const MontgomeryInt& that) const { return !(*this == that); }

  // Modular exponentiation.
  MontgomeryInt ModExp(Int exponent,
                                               const Params* params) const;

  // Inverse using modular exponentiation.
  //
  // Note: The implementation assumes the modulus is a prime number.
  MontgomeryInt
  MultiplicativeInverse(const Params* params) const;

  // Inverse using extended Euclidean algorithm.
  // Returns this^(-1) (mod modulus) if the inverse exists, or returns an error
  // if gcd(this, modulus) != 1.
  //
  // WARNING: This function is not constant time, so it should not be used on
  //          ciphertext or key materials.
  absl::StatusOr<MontgomeryInt> MultiplicativeInverseFast(
      const Params* params) const;

 private:
  template <typename Prng = rlwe::SecurePrng>
  static rlwe::StatusOr<Int> GenerateRandomInt(
      int log_modulus, Prng* prng) {
    // Generate a random Int. As the modulus is always smaller than max(Int),
    // there will be no issues with overflow.
    int max_bits_per_step = std::min((int)Params::bitsize_int, (int)64);
    auto bits_required = log_modulus;
    Int rand = 0;
    while (bits_required > 0) {
      Int rand_bits = 0;
      if (bits_required <= 8) {
        // Generate 8 bits of randomness.
        RLWE_ASSIGN_OR_RETURN(rand_bits, prng->Rand8());

        // Extract bits_required bits and add them to rand.
        Int needed_bits =
            rand_bits & ((static_cast<Int>(1) << bits_required) - 1);
        rand = (rand << bits_required) + needed_bits;
        break;
      } else {
        // Generate 64 bits of randomness.
        RLWE_ASSIGN_OR_RETURN(rand_bits, prng->Rand64());

        // Extract min(64, bits in Int, bits_required) bits and add them to rand
        int bits_to_extract = std::min(bits_required, max_bits_per_step);
        Int needed_bits =
            rand_bits & ((static_cast<Int>(1) << bits_to_extract) - 1);
        rand = (rand << bits_to_extract) + needed_bits;
        bits_required -= bits_to_extract;
      }
    }
    return rand;
  }

  Int n_;
};

// Instantiations of MontgomeryInt and MontgomeryIntParams with specific
// integral types.
extern template struct EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryIntParams<Uint16>;
extern template struct EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryIntParams<Uint32>;
extern template struct EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryIntParams<Uint64>;
extern template struct EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryIntParams<absl::uint128>;
extern template class EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryInt<Uint16>;
extern template class EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryInt<Uint32>;
extern template class EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryInt<Uint64>;
extern template class EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryInt<absl::uint128>;

#ifdef ABSL_HAVE_INTRINSIC_INT128
extern template struct EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryIntParams<unsigned __int128>;
extern template class EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryInt<unsigned __int128>;
#endif
}  // namespace rlwe

#endif  // RLWE_MONTGOMERY_H_
