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

#ifndef RLWE_ERROR_PARAMS_TEST_H_
#define RLWE_ERROR_PARAMS_TEST_H_

#include "error_params.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "constants.h"
#include "context.h"
#include "montgomery.h"
#include "ntt_parameters.h"
#include "prng/integral_prng_types.h"
#include "status_macros.h"
#include "symmetric_encryption.h"
#include "testing/parameters.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"
#include "testing/testing_utils.h"

namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

// Number of samples used to compute the actual variance.
const rlwe::Uint64 kSamples = 50;

template <typename ModularInt>
class ErrorParamsTest : public testing::Test {
  using Int = typename ModularInt::Int;
  using Polynomial = rlwe::Polynomial<ModularInt>;
  using Ciphertext = rlwe::SymmetricRlweCiphertext<ModularInt>;
  using Key = rlwe::SymmetricRlweKey<ModularInt>;

 public:
  // Computes the l-infinity norm of a vector of Ints.
  double ComputeNorm(const std::vector<Int>& coeffs) {
    return static_cast<double>(*std::max_element(coeffs.begin(), coeffs.end()));
  }

  // Sample a random key.
  rlwe::StatusOr<Key> SampleKey(const rlwe::RlweContext<ModularInt>* context) {
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return Key::Sample(context->GetLogN(), context->GetVariance(),
                       context->GetLogT(), context->GetModulusParams(),
                       context->GetNttParams(), prng.get());
  }

  // Encrypt a plaintext.
  rlwe::StatusOr<Ciphertext> Encrypt(
      const Key& key, const std::vector<Int>& plaintext,
      const rlwe::RlweContext<ModularInt>* context) {
    RLWE_ASSIGN_OR_RETURN(auto m_p,
                          rlwe::testing::ConvertToMontgomery<ModularInt>(
                              plaintext, context->GetModulusParams()));
    auto plaintext_ntt = Polynomial::ConvertToNtt(m_p, context->GetNttParams(),
                                                  context->GetModulusParams());
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return rlwe::Encrypt<ModularInt>(key, plaintext_ntt,
                                     context->GetErrorParams(), prng.get());
  }

  // Decrypt without removing the error, returning (m + et).
  rlwe::StatusOr<std::vector<Int>> GetErrorAndMessage(
      const Key& key, const Ciphertext& ciphertext) {
    Polynomial error_and_message_ntt(key.Len(), key.ModulusParams());
    Polynomial key_powers = key.Key();
    for (int i = 0; i < ciphertext.Len(); i++) {
      // Extract component i.
      RLWE_ASSIGN_OR_RETURN(Polynomial ci, ciphertext.Component(i));

      if (i > 1) {
        RLWE_RETURN_IF_ERROR(
            key_powers.MulInPlace(key.Key(), key.ModulusParams()));
      }
      // Beyond c0, multiply the exponentiated key in.
      if (i > 0) {
        RLWE_RETURN_IF_ERROR(ci.MulInPlace(key_powers, key.ModulusParams()));
      }
      RLWE_RETURN_IF_ERROR(
          error_and_message_ntt.AddInPlace(ci, key.ModulusParams()));
    }
    auto error_and_message =
        error_and_message_ntt.InverseNtt(key.NttParams(), key.ModulusParams());

    // Convert the integers mod q to integers.
    std::vector<Int> error_and_message_ints(error_and_message.size(), 0);
    for (int i = 0; i < error_and_message.size(); i++) {
      error_and_message_ints[i] =
          error_and_message[i].ExportInt(key.ModulusParams());
      if (error_and_message_ints[i] > (key.ModulusParams()->modulus >> 1)) {
        error_and_message_ints[i] =
            key.ModulusParams()->modulus - error_and_message_ints[i];
      }
    }
    return error_and_message_ints;
  }
};
TYPED_TEST_SUITE(ErrorParamsTest, rlwe::testing::ModularIntTypes);

TYPED_TEST(ErrorParamsTest, CreateError) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));

    // large value for log_t
    const int log_t = context->GetModulusParams()->log_modulus;

    EXPECT_THAT(rlwe::ErrorParams<TypeParam>::Create(
                    log_t, rlwe::testing::kDefaultVariance,
                    context->GetModulusParams(), context->GetNttParams()),
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr(absl::StrCat(
                             "The value log_t, ", log_t,
                             ", must be smaller than log_modulus - 1, ",
                             log_t - 1, "."))));
  }
}

TYPED_TEST(ErrorParamsTest, PlaintextError) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));

    // Randomly sample polynomials and expect l-infinity norm is bounded by
    // b_plaintext.
    for (int i = 0; i < kSamples; i++) {
      // Samples a polynomial with kLogT and kDefaultCoeffs.
      auto plaintext = rlwe::testing::SamplePlaintext<TypeParam>(
          context->GetN(), context->GetT());

      // Expect that the norm of the coefficients of the plaintext is less than
      // b_plaintext.
      double norm = this->ComputeNorm(plaintext);
      EXPECT_LT(norm, context->GetErrorParams()->B_plaintext());
    }
  }
}

TYPED_TEST(ErrorParamsTest, EncryptionError) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

    // Randomly sample polynomials, decrypt, and compute the size of the result
    // before removing error.
    for (int i = 0; i < kSamples; i++) {
      // Expect that the norm of the coefficients of (m + et) is less than
      // b_encryption.
      auto plaintext = rlwe::testing::SamplePlaintext<TypeParam>(
          context->GetN(), context->GetT());
      ASSERT_OK_AND_ASSIGN(auto ciphertext,
                           this->Encrypt(key, plaintext, context.get()));
      ASSERT_OK_AND_ASSIGN(auto error_and_message,
                           this->GetErrorAndMessage(key, ciphertext));

      EXPECT_LT(this->ComputeNorm(error_and_message),
                context->GetErrorParams()->B_encryption());
    }
  }
}

TYPED_TEST(ErrorParamsTest, RelinearizationErrorScalesWithT) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    // Error scales by (T / logT) when all other constants are fixed.
    int small_decomposition_modulus = 1;
    int large_decomposition_modulus = 10;
    EXPECT_LT(
        context->GetErrorParams()->B_relinearize(small_decomposition_modulus),
        context->GetErrorParams()->B_relinearize(large_decomposition_modulus));
  }
}

}  //  namespace

#endif  // RLWE_ERROR_PARAMS_TEST_H_
