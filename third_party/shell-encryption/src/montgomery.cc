// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "montgomery.h"

#include "transcription.h"

namespace rlwe {

template <typename T>
rlwe::StatusOr<std::unique_ptr<const MontgomeryIntParams<T>>>
MontgomeryIntParams<T>::Create(Int modulus) {
  // Check that the modulus is smaller than max(Int) / 4.
  Int most_significant_bit = modulus >> (bitsize_int - 2);
  if (most_significant_bit != 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "The modulus should be less than 2^", (bitsize_int - 2), "."));
  }
  if ((modulus % 2) == 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("The modulus should be odd."));
  }
  return absl::WrapUnique<const MontgomeryIntParams>(
      new MontgomeryIntParams(modulus));
}

// From Hacker's Delight.
template <typename T>
std::tuple<T, T> MontgomeryIntParams<T>::Inverses(BigInt modulus_bigint,
                                                  BigInt r) {
  // Invariants
  //   1) sum = x * 2^w - y * modulus.
  //   2) sum is always a power of 2.
  //   3) modulus is odd.
  //   4) y is always even.
  // sum will decrease from 2^w to 2^0 = 1
  BigInt x = 1;
  BigInt y = 0;
  for (int i = bitsize_int; i > 0; i--) {
    // Ensure that x is even.
    if ((x & 1) == 1) {
      // If x is odd, make x even by adding modulus to x and changing the
      // value of y accordingly (y remains even).
      //
      //     sum = x * 2^w - y * modulus
      //     sum = (x + modulus) * 2^w - (y + 2^w) * modulus
      //
      // We can then divide the new values of x and y by 2 safely.
      x += modulus_bigint;
      y += r;
    }
    // Divide x and y by 2
    x >>= 1;
    y >>= 1;
  }
  // Return the inverses
  return std::make_tuple(static_cast<Int>(x), static_cast<Int>(y));
}

template <typename T>
rlwe::StatusOr<MontgomeryInt<T>> MontgomeryInt<T>::ImportInt(
    Int n, const Params* params) {
  BigInt product = static_cast<BigInt>(params->r_mod_modulus_barrett) * n;
  Int result = static_cast<Int>(product >> Params::bitsize_int);
  result = n * params->r_mod_modulus - result * params->modulus;
  // The steps above produce an integer that is in the range [0, 2N).
  // We now reduce to the range [0, N).
  result -= (result >= params->modulus) ? params->modulus : 0;
  return MontgomeryInt(result);
}

template <typename T>
MontgomeryInt<T> MontgomeryInt<T>::ImportZero(const Params* params) {
  return MontgomeryInt(params->Zero());
}

template <typename T>
MontgomeryInt<T> MontgomeryInt<T>::ImportOne(const Params* params) {
  // 1 should be multiplied by r_mod_modulus; we load directly r_mod_modulus.
  return MontgomeryInt(static_cast<Int>(params->r_mod_modulus));
}

template <typename T>
typename internal::BigInt<T>::value_type MontgomeryInt<T>::DivAndTruncate(
    BigInt dividend, BigInt divisor) {
  return dividend / divisor;
}

template <typename T>
rlwe::StatusOr<std::string> MontgomeryInt<T>::Serialize(
    const Params* params) const {
  // Use transcription to transform all the LogModulus() bits of input into a
  // vector of unsigned char.
  RLWE_ASSIGN_OR_RETURN(
      auto v, (TranscribeBits<Int, Uint8>({this->n_}, params->log_modulus,
                                          params->log_modulus, 8)));
  // Return a string
  return std::string(std::make_move_iterator(v.begin()),
                     std::make_move_iterator(v.end()));
}

template <typename T>
rlwe::StatusOr<std::string> MontgomeryInt<T>::SerializeVector(
    const std::vector<MontgomeryInt>& coeffs, const Params* params) {
  if (coeffs.size() > kMaxNumCoeffs) {
    return absl::InvalidArgumentError(
        absl::StrCat("Number of coefficients, ", coeffs.size(),
                     ", cannot be larger than ", kMaxNumCoeffs, "."));
  } else if (coeffs.empty()) {
    return absl::InvalidArgumentError("Cannot serialize an empty vector.");
  }
  // Bits required to represent modulus.
  int bit_size = params->log_modulus;
  // Extract the values
  std::vector<Int> coeffs_values;
  coeffs_values.reserve(coeffs.size());
  for (const auto& c : coeffs) {
    coeffs_values.push_back(c.n_);
  }
  // Use transcription to transform all the bit_size bits of input into a
  // vector of unsigned char.
  RLWE_ASSIGN_OR_RETURN(
      auto v,
      (TranscribeBits<Int, Uint8>(
          coeffs_values, coeffs_values.size() * bit_size, bit_size, 8)));
  // Return a string
  return std::string(std::make_move_iterator(v.begin()),
                     std::make_move_iterator(v.end()));
}

template <typename T>
rlwe::StatusOr<MontgomeryInt<T>> MontgomeryInt<T>::Deserialize(
    absl::string_view payload, const Params* params) {
  // Parse the string as unsigned char
  std::vector<Uint8> input(payload.begin(), payload.end());
  // Bits required to represent modulus.
  int bit_size = params->log_modulus;
  // Recover the coefficients from the input stream.
  RLWE_ASSIGN_OR_RETURN(auto coeffs_values, (TranscribeBits<Uint8, Int>(
                                                input, bit_size, 8, bit_size)));
  // There will be at least one coefficient in coeff_values because bit_size
  // is always expected to be positive.
  return MontgomeryInt(coeffs_values[0]);
}

template <typename T>
rlwe::StatusOr<std::vector<MontgomeryInt<T>>>
MontgomeryInt<T>::DeserializeVector(int num_coeffs,
                                    absl::string_view serialized,
                                    const Params* params) {
  if (num_coeffs < 0) {
    return absl::InvalidArgumentError(
        "Number of coefficients must be non-negative.");
  }
  if (num_coeffs > kMaxNumCoeffs) {
    return absl::InvalidArgumentError(
        absl::StrCat("Number of coefficients, ", num_coeffs, ", cannot be ",
                     "larger than ", kMaxNumCoeffs, "."));
  }
  // Parse the string as unsigned char
  std::vector<Uint8> input(serialized.begin(), serialized.end());
  // Bits required to represent modulus.
  int bit_size = params->log_modulus;
  // Recover the coefficients from the input stream.
  RLWE_ASSIGN_OR_RETURN(
      auto coeffs_values,
      (TranscribeBits<Uint8, Int>(input, bit_size * num_coeffs, 8, bit_size)));
  // Check that the number of coefficients recovered is at least what is
  // expected.
  if (coeffs_values.size() < num_coeffs) {
    return absl::InvalidArgumentError("Given serialization is invalid.");
  }
  // Create a vector of Montgomery Int from the values.
  std::vector<MontgomeryInt> coeffs;
  coeffs.reserve(num_coeffs);
  for (int i = 0; i < num_coeffs; i++) {
    coeffs.push_back(MontgomeryInt(coeffs_values[i]));
  }
  return coeffs;
}

template <typename T>
std::tuple<T, T> MontgomeryInt<T>::GetConstant(const Params* params) const {
  Int constant = ExportInt(params);
  Int constant_barrett = static_cast<Int>(
      (static_cast<BigInt>(constant) << params->bitsize_int) / params->modulus);
  return std::make_tuple(constant, constant_barrett);
}

template <typename T>
rlwe::StatusOr<std::vector<MontgomeryInt<T>>> MontgomeryInt<T>::BatchAdd(
    const std::vector<MontgomeryInt>& in1,
    const std::vector<MontgomeryInt>& in2, const Params* params) {
  std::vector<MontgomeryInt> out = in1;
  RLWE_RETURN_IF_ERROR(BatchAddInPlace(&out, in2, params));
  return out;
}

template <typename T>
absl::Status MontgomeryInt<T>::BatchAddInPlace(
    std::vector<MontgomeryInt>* in1, const std::vector<MontgomeryInt>& in2,
    const Params* params) {
  // If the input vectors' sizes don't match, return an error.
  if (in1->size() != in2.size()) {
    return absl::InvalidArgumentError("Input vectors are not of same size");
  }
  int i = 0;
  // The remaining elements, if any, are added in place sequentially.
  for (; i < in1->size(); i++) {
    (*in1)[i].AddInPlace(in2[i], params);
  }
  return absl::OkStatus();
}

template <typename T>
rlwe::StatusOr<std::vector<MontgomeryInt<T>>> MontgomeryInt<T>::BatchAdd(
    const std::vector<MontgomeryInt>& in1, const MontgomeryInt& in2,
    const Params* params) {
  std::vector<MontgomeryInt> out = in1;
  RLWE_RETURN_IF_ERROR(BatchAddInPlace(&out, in2, params));
  return out;
}

template <typename T>
absl::Status MontgomeryInt<T>::BatchAddInPlace(std::vector<MontgomeryInt>* in1,
                                               const MontgomeryInt& in2,
                                               const Params* params) {
  int i = 0;
  std::for_each(in1->begin() + i, in1->end(),
                [&in2 = in2, params](MontgomeryInt& coeff) {
                  coeff.AddInPlace(in2, params);
                });
  return absl::OkStatus();
}

template <typename T>
rlwe::StatusOr<std::vector<MontgomeryInt<T>>> MontgomeryInt<T>::BatchSub(
    const std::vector<MontgomeryInt>& in1,
    const std::vector<MontgomeryInt>& in2, const Params* params) {
  std::vector<MontgomeryInt> out = in1;
  RLWE_RETURN_IF_ERROR(BatchSubInPlace(&out, in2, params));
  return out;
}

template <typename T>
absl::Status MontgomeryInt<T>::BatchSubInPlace(
    std::vector<MontgomeryInt>* in1, const std::vector<MontgomeryInt>& in2,
    const Params* params) {
  // If the input vectors' sizes don't match, return an error.
  if (in1->size() != in2.size()) {
    return absl::InvalidArgumentError("Input vectors are not of same size");
  }
  int i = 0;
  for (; i < in1->size(); i++) {
    (*in1)[i].SubInPlace(in2[i], params);
  }
  return absl::OkStatus();
}

template <typename T>
rlwe::StatusOr<std::vector<MontgomeryInt<T>>> MontgomeryInt<T>::BatchSub(
    const std::vector<MontgomeryInt>& in1, const MontgomeryInt& in2,
    const Params* params) {
  std::vector<MontgomeryInt> out = in1;
  RLWE_RETURN_IF_ERROR(BatchSubInPlace(&out, in2, params));
  return out;
}

template <typename T>
absl::Status MontgomeryInt<T>::BatchSubInPlace(std::vector<MontgomeryInt>* in1,
                                               const MontgomeryInt& in2,
                                               const Params* params) {
  int i = 0;
  std::for_each(in1->begin() + i, in1->end(),
                [&in2 = in2, params](MontgomeryInt& coeff) {
                  coeff.SubInPlace(in2, params);
                });
  return absl::OkStatus();
}

template <typename T>
rlwe::StatusOr<std::vector<MontgomeryInt<T>>>
MontgomeryInt<T>::BatchMulConstant(const std::vector<MontgomeryInt>& in1,
                                   const std::vector<Int>& constant,
                                   const std::vector<Int>& constant_barrett,
                                   const Params* params) {
  std::vector<MontgomeryInt> out = in1;
  RLWE_RETURN_IF_ERROR(
      BatchMulConstantInPlace(&out, constant, constant_barrett, params));
  return out;
}

template <typename T>
absl::Status MontgomeryInt<T>::BatchMulConstantInPlace(
    std::vector<MontgomeryInt>* in1, const std::vector<Int>& constant,
    const std::vector<Int>& constant_barrett, const Params* params) {
  // If the input vectors' sizes don't match, return an error.
  if (in1->size() != constant.size() ||
      constant.size() != constant_barrett.size()) {
    return absl::InvalidArgumentError("Input vectors are not of same size");
  }
  int i = 0;
  for (; i < in1->size(); i++) {
    (*in1)[i].MulConstantInPlace(constant[i], constant_barrett[i], params);
  }
  return absl::OkStatus();
}

template <typename T>
rlwe::StatusOr<std::vector<MontgomeryInt<T>>>
MontgomeryInt<T>::BatchMulConstant(const std::vector<MontgomeryInt>& in1,
                                   const Int& constant,
                                   const Int& constant_barrett,
                                   const Params* params) {
  std::vector<MontgomeryInt> out = in1;
  RLWE_RETURN_IF_ERROR(
      BatchMulConstantInPlace(&out, constant, constant_barrett, params));
  return out;
}

template <typename T>
absl::Status MontgomeryInt<T>::BatchMulConstantInPlace(
    std::vector<MontgomeryInt>* in1, const Int& constant,
    const Int& constant_barrett, const Params* params) {
  int i = 0;
  for (; i < in1->size(); i++) {
    (*in1)[i].MulConstantInPlace(constant, constant_barrett, params);
  }
  return absl::OkStatus();
}

template <typename T>
rlwe::StatusOr<std::vector<MontgomeryInt<T>>> MontgomeryInt<T>::BatchMul(
    const std::vector<MontgomeryInt>& in1,
    const std::vector<MontgomeryInt>& in2, const Params* params) {
  std::vector<MontgomeryInt> out = in1;
  RLWE_RETURN_IF_ERROR(BatchMulInPlace(&out, in2, params));
  return out;
}

template <typename T>
absl::Status MontgomeryInt<T>::BatchMulInPlace(
    std::vector<MontgomeryInt>* in1, const std::vector<MontgomeryInt>& in2,
    const Params* params) {
  // If the input vectors' sizes don't match, return an error.
  if (in1->size() != in2.size()) {
    return absl::InvalidArgumentError("Input vectors are not of same size");
  }
  int i = 0;
  for (; i < in1->size(); i++) {
    (*in1)[i].MulInPlace(in2[i], params);
  }
  return absl::OkStatus();
}

template <typename T>
rlwe::StatusOr<std::vector<MontgomeryInt<T>>> MontgomeryInt<T>::BatchMul(
    const std::vector<MontgomeryInt>& in1, const MontgomeryInt& in2,
    const Params* params) {
  std::vector<MontgomeryInt> out = in1;
  RLWE_RETURN_IF_ERROR(BatchMulInPlace(&out, in2, params));
  return out;
}

template <typename T>
absl::Status MontgomeryInt<T>::BatchMulInPlace(std::vector<MontgomeryInt>* in1,
                                               const MontgomeryInt& in2,
                                               const Params* params) {
  int i = 0;
  std::for_each(in1->begin() + i, in1->end(),
                [&in2 = in2, params](MontgomeryInt& coeff) {
                  coeff.MulInPlace(in2, params);
                });
  return absl::OkStatus();
}

template <typename T>
MontgomeryInt<T> MontgomeryInt<T>::ModExp(Int exponent,
                                          const Params* params) const {
  MontgomeryInt result = MontgomeryInt::ImportOne(params);
  MontgomeryInt base = *this;

  // Uses the bits of the exponent to gradually compute the result.
  // When bit k of the exponent is 1, the result is multiplied by
  // base^{2^k}.
  while (exponent > 0) {
    // If the current bit (bit k) is 1, multiply base^{2^k} into the result.
    if (exponent % 2 == 1) {
      result.MulInPlace(base, params);
    }

    // Update base from base^{2^k} to base^{2^{k+1}}.
    base.MulInPlace(base, params);
    exponent >>= 1;
  }

  return result;
}

template <typename T>
MontgomeryInt<T> MontgomeryInt<T>::MultiplicativeInverse(
    const Params* params) const {
  return (*this).ModExp(static_cast<Int>(params->modulus - 2), params);
}

// Instantiations of MontgomeryInt and MontgomeryIntParams with specific
// integral types.
template struct MontgomeryIntParams<Uint16>;
template struct MontgomeryIntParams<Uint32>;
template struct MontgomeryIntParams<Uint64>;
template struct MontgomeryIntParams<absl::uint128>;
template class MontgomeryInt<Uint16>;
template class MontgomeryInt<Uint32>;
template class MontgomeryInt<Uint64>;
template class MontgomeryInt<absl::uint128>;
}  // namespace rlwe
