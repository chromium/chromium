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

#include "symmetric_encryption.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "constants.h"
#include "context.h"
#include "montgomery.h"
#include "ntt_parameters.h"
#include "polynomial.h"
#include "prng/integral_prng_types.h"
#include "serialization.pb.h"
#include "status_macros.h"
#include "testing/parameters.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"
#include "testing/testing_utils.h"

namespace {

using ::rlwe::testing::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;

// Set constants.
const int kTestingRounds = 10;

// Tests symmetric-key encryption scheme, including the following homomorphic
// operations: addition, scalar multiplication by a polynomial (absorb), and
// multiplication. Substitutions are implemented in
// testing/coefficient_polynomial_ciphertext.h, and SymmetricRlweKey::Substitute
// and SymmetricRlweCiphertext::PowersOfS() (updated on substitution calls) are
// further tested in testing/coefficient_polynomial_ciphertext_test.cc.
template <typename ModularInt>
class SymmetricRlweEncryptionTest : public ::testing::Test {
 public:
  // Sample a random key.
  rlwe::StatusOr<rlwe::SymmetricRlweKey<ModularInt>> SampleKey(
      const rlwe::RlweContext<ModularInt>* context) {
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return rlwe::SymmetricRlweKey<ModularInt>::Sample(
        context->GetLogN(), context->GetVariance(), context->GetLogT(),
        context->GetModulusParams(), context->GetNttParams(), prng.get());
  }

  // Encrypt a plaintext.
  rlwe::StatusOr<rlwe::SymmetricRlweCiphertext<ModularInt>> Encrypt(
      const rlwe::SymmetricRlweKey<ModularInt>& key,
      const std::vector<typename ModularInt::Int>& plaintext,
      const rlwe::RlweContext<ModularInt>* context) {
    RLWE_ASSIGN_OR_RETURN(auto mont,
                          rlwe::testing::ConvertToMontgomery<ModularInt>(
                              plaintext, context->GetModulusParams()));
    auto plaintext_ntt = rlwe::Polynomial<ModularInt>::ConvertToNtt(
        mont, context->GetNttParams(), context->GetModulusParams());
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return rlwe::Encrypt<ModularInt>(key, plaintext_ntt,
                                     context->GetErrorParams(), prng.get());
  }
};
TYPED_TEST_SUITE(SymmetricRlweEncryptionTest, rlwe::testing::ModularIntTypes);

// Ensure that RemoveError works correctly on negative numbers for several
// different values of t.
TYPED_TEST(SymmetricRlweEncryptionTest, RemoveErrorNegative) {
  unsigned int seed = 0;
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));

    for (int t = 2; t < 16; t++) {
      for (int i = 0; i < kTestingRounds; i++) {
        // Sample a plaintext in the range (modulus/2, modulus)
        typename TypeParam::Int plaintext =
            (rand_r(&seed) % (context->GetModulus() / 2)) +
            context->GetModulus() / 2 + 1;
        // Create a vector that exclusively contains the value "plaintext".
        ASSERT_OK_AND_ASSIGN(
            auto m_plaintext,
            TypeParam::ImportInt(plaintext, context->GetModulusParams()));
        std::vector<TypeParam> error_and_message(context->GetN(), m_plaintext);
        auto result = rlwe::RemoveError<TypeParam>(error_and_message,
                                                   context->GetModulus(), t,
                                                   context->GetModulusParams());

        // Compute the expected result using signed arithmetic. Derive its
        // negative equivalent by subtracting out testing::kModulus and taking
        // that negative value (mod t).
        absl::int128 expected =
            (static_cast<absl::int128>(plaintext) -
             static_cast<absl::int128>(context->GetModulus())) %
            t;

        // Finally, turn any negative values into their positive equivalents
        // (mod t).
        if (expected < 0) {
          expected += t;
        }

        for (unsigned int j = 0; j < context->GetN(); j++) {
          EXPECT_EQ(expected, result[j]) << t << plaintext;
        }
      }
    }
  }
}

// Ensure that RemoveError works correctly on positive numbers for several
// different values of t.
TYPED_TEST(SymmetricRlweEncryptionTest, RemoveErrorPositive) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    unsigned int seed = 0;

    for (int t = 2; t < 16; t++) {
      for (int i = 0; i < kTestingRounds; i++) {
        // Sample a plaintext in the range (0, modulus/2)
        typename TypeParam::Int plaintext =
            rand_r(&seed) % (context->GetModulus() / 2);

        // Create a vector that exclusively contains the value "plaintext".
        ASSERT_OK_AND_ASSIGN(
            auto m_plaintext,
            TypeParam::ImportInt(plaintext, context->GetModulusParams()));
        std::vector<TypeParam> error_and_message(context->GetN(), m_plaintext);
        auto result = rlwe::RemoveError<TypeParam>(error_and_message,
                                                   context->GetModulus(), t,
                                                   context->GetModulusParams());

        for (unsigned int j = 0; j < context->GetN(); j++) {
          EXPECT_EQ(plaintext % t, result[j]);
        }
      }
    }
  }
}

// Ensure that the encryption scheme can decrypt its own ciphertexts.
TYPED_TEST(SymmetricRlweEncryptionTest, CanDecrypt) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    for (unsigned int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));
      auto plaintext = rlwe::testing::SamplePlaintext<TypeParam>(
          context->GetN(), context->GetT());
      ASSERT_OK_AND_ASSIGN(auto ciphertext,
                           this->Encrypt(key, plaintext, context.get()));
      ASSERT_OK_AND_ASSIGN(auto decrypted,
                           rlwe::Decrypt<TypeParam>(key, ciphertext));

      EXPECT_EQ(plaintext, decrypted);
    }
  }
}

// Accessing out of bounds raises errors
TYPED_TEST(SymmetricRlweEncryptionTest, OutOfBoundsIndex) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));
    auto plaintext = rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                               context->GetT());
    ASSERT_OK_AND_ASSIGN(auto ciphertext,
                         this->Encrypt(key, plaintext, context.get()));
    ASSERT_OK(ciphertext.Component(ciphertext.Len() - 1));
    EXPECT_THAT(ciphertext.Component(ciphertext.Len()),
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("Index out of range.")));
    EXPECT_THAT(ciphertext.Component(-1),
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("Index out of range.")));
  }
}

// Check that the HE scheme is additively homomorphic.
TYPED_TEST(SymmetricRlweEncryptionTest, AdditivelyHomomorphic) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    for (unsigned int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

      std::vector<typename TypeParam::Int> plaintext1 =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());
      std::vector<typename TypeParam::Int> plaintext2 =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());

      ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                           this->Encrypt(key, plaintext1, context.get()));
      ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                           this->Encrypt(key, plaintext2, context.get()));
      ASSERT_OK_AND_ASSIGN(auto ciphertext_add, ciphertext1 + ciphertext2);
      ASSERT_OK_AND_ASSIGN(auto ciphertext_sub, ciphertext1 - ciphertext2);

      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted_add,
                           rlwe::Decrypt<TypeParam>(key, ciphertext_add));
      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted_sub,
                           rlwe::Decrypt<TypeParam>(key, ciphertext_sub));

      for (unsigned int j = 0; j < plaintext1.size(); j++) {
        EXPECT_EQ((plaintext1[j] + plaintext2[j]) % context->GetT(),
                  decrypted_add[j]);
        EXPECT_EQ(
            (context->GetT() + plaintext1[j] - plaintext2[j]) % context->GetT(),
            decrypted_sub[j]);
        // Check that the error grows additively.
        EXPECT_EQ(ciphertext_add.Error(),
                  ciphertext1.Error() + ciphertext2.Error());
        EXPECT_EQ(ciphertext_sub.Error(),
                  ciphertext1.Error() + ciphertext2.Error());
      }
    }
  }
}

// Check that homomorphic addition can be performed in place.
TYPED_TEST(SymmetricRlweEncryptionTest, AddHomomorphicallyInPlace) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    for (unsigned int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

      std::vector<typename TypeParam::Int> plaintext1 =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());
      std::vector<typename TypeParam::Int> plaintext2 =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());

      ASSERT_OK_AND_ASSIGN(auto ciphertext1_add,
                           this->Encrypt(key, plaintext1, context.get()));
      ASSERT_OK_AND_ASSIGN(auto ciphertext1_sub,
                           this->Encrypt(key, plaintext1, context.get()));
      ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                           this->Encrypt(key, plaintext2, context.get()));
      const double ciphertext1_add_error = ciphertext1_add.Error();
      const double ciphertext1_sub_error = ciphertext1_sub.Error();

      ASSERT_OK(ciphertext1_add.AddInPlace(ciphertext2));
      ASSERT_OK(ciphertext1_sub.SubInPlace(ciphertext2));

      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted1_add,
                           rlwe::Decrypt<TypeParam>(key, ciphertext1_add));
      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted1_sub,
                           rlwe::Decrypt<TypeParam>(key, ciphertext1_sub));

      for (unsigned int j = 0; j < plaintext1.size(); j++) {
        EXPECT_EQ((plaintext1[j] + plaintext2[j]) % context->GetT(),
                  decrypted1_add[j]);
        EXPECT_EQ(
            (context->GetT() + plaintext1[j] - plaintext2[j]) % context->GetT(),
            decrypted1_sub[j]);
        // Check that the error grows additively.
        EXPECT_EQ(ciphertext1_add.Error(),
                  ciphertext1_add_error + ciphertext2.Error());
        EXPECT_EQ(ciphertext1_sub.Error(),
                  ciphertext1_sub_error + ciphertext2.Error());
      }
    }
  }
}

// Check that homomorphic addition to a 0-ciphertext does not change the
// plaintext.
TYPED_TEST(SymmetricRlweEncryptionTest, AddToZero) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    for (unsigned int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

      std::vector<typename TypeParam::Int> plaintext =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());

      rlwe::SymmetricRlweCiphertext<TypeParam> ciphertext1(
          context->GetModulusParams(), context->GetErrorParams());
      ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                           this->Encrypt(key, plaintext, context.get()));
      ASSERT_OK_AND_ASSIGN(auto ciphertext3, ciphertext1 + ciphertext2);

      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted,
                           rlwe::Decrypt<TypeParam>(key, ciphertext3));

      EXPECT_EQ(plaintext, decrypted);
    }
  }
}

// Check that homomorphic absorption works.
TYPED_TEST(SymmetricRlweEncryptionTest, Absorb) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    for (unsigned int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

      // Create the initial plaintexts.
      std::vector<typename TypeParam::Int> plaintext =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());
      ASSERT_OK_AND_ASSIGN(auto m_plaintext,
                           rlwe::testing::ConvertToMontgomery<TypeParam>(
                               plaintext, context->GetModulusParams()));
      rlwe::Polynomial<TypeParam> plaintext_ntt =
          rlwe::Polynomial<TypeParam>::ConvertToNtt(
              m_plaintext, context->GetNttParams(),
              context->GetModulusParams());
      std::vector<typename TypeParam::Int> to_absorb =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());
      ASSERT_OK_AND_ASSIGN(auto m_to_absorb,
                           rlwe::testing::ConvertToMontgomery<TypeParam>(
                               to_absorb, context->GetModulusParams()));
      rlwe::Polynomial<TypeParam> to_absorb_ntt =
          rlwe::Polynomial<TypeParam>::ConvertToNtt(
              m_to_absorb, context->GetNttParams(),
              context->GetModulusParams());

      // Create our expected value.
      ASSERT_OK_AND_ASSIGN(
          rlwe::Polynomial<TypeParam> expected_ntt,
          plaintext_ntt.Mul(to_absorb_ntt, context->GetModulusParams()));
      std::vector<typename TypeParam::Int> expected =
          rlwe::RemoveError<TypeParam>(
              expected_ntt.InverseNtt(context->GetNttParams(),
                                      context->GetModulusParams()),
              context->GetModulus(), context->GetT(),
              context->GetModulusParams());

      // Encrypt, absorb, and decrypt.
      ASSERT_OK_AND_ASSIGN(auto encrypt,
                           this->Encrypt(key, plaintext, context.get()));
      ASSERT_OK_AND_ASSIGN(auto ciphertext, encrypt* to_absorb_ntt);
      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted,
                           rlwe::Decrypt<TypeParam>(key, ciphertext));

      EXPECT_EQ(expected, decrypted);

      // Check that the error is the product of an encryption and a plaintext.
      EXPECT_EQ(ciphertext.Error(),
                context->GetErrorParams()->B_encryption() *
                    context->GetErrorParams()->B_plaintext());
    }
  }
}

// Check that homomorphic absorption in place works.
TYPED_TEST(SymmetricRlweEncryptionTest, AbsorbInPlace) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    for (unsigned int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

      // Create the initial plaintexts.
      std::vector<typename TypeParam::Int> plaintext =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());
      ASSERT_OK_AND_ASSIGN(auto m_plaintext,
                           rlwe::testing::ConvertToMontgomery<TypeParam>(
                               plaintext, context->GetModulusParams()));
      rlwe::Polynomial<TypeParam> plaintext_ntt =
          rlwe::Polynomial<TypeParam>::ConvertToNtt(
              m_plaintext, context->GetNttParams(),
              context->GetModulusParams());
      std::vector<typename TypeParam::Int> to_absorb =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());
      ASSERT_OK_AND_ASSIGN(auto m_to_absorb,
                           rlwe::testing::ConvertToMontgomery<TypeParam>(
                               to_absorb, context->GetModulusParams()));
      rlwe::Polynomial<TypeParam> to_absorb_ntt =
          rlwe::Polynomial<TypeParam>::ConvertToNtt(
              m_to_absorb, context->GetNttParams(),
              context->GetModulusParams());

      // Create our expected value.
      ASSERT_OK_AND_ASSIGN(
          rlwe::Polynomial<TypeParam> expected_ntt,
          plaintext_ntt.Mul(to_absorb_ntt, context->GetModulusParams()));
      std::vector<typename TypeParam::Int> expected =
          rlwe::RemoveError<TypeParam>(
              expected_ntt.InverseNtt(context->GetNttParams(),
                                      context->GetModulusParams()),
              context->GetModulus(), context->GetT(),
              context->GetModulusParams());

      // Encrypt, absorb in place, and decrypt.
      ASSERT_OK_AND_ASSIGN(auto ciphertext,
                           this->Encrypt(key, plaintext, context.get()));
      ASSERT_OK(ciphertext.AbsorbInPlace(to_absorb_ntt));
      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted,
                           rlwe::Decrypt<TypeParam>(key, ciphertext));

      EXPECT_EQ(expected, decrypted);

      // Check that the error is the product of an encryption and a plaintext.
      EXPECT_EQ(ciphertext.Error(),
                context->GetErrorParams()->B_encryption() *
                    context->GetErrorParams()->B_plaintext());
    }
  }
}

// Check that homomorphic absorption of a scalar works.
TYPED_TEST(SymmetricRlweEncryptionTest, AbsorbScalar) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    unsigned int seed = 0;

    for (unsigned int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

      // Create the initial plaintexts.
      std::vector<typename TypeParam::Int> plaintext =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());
      ASSERT_OK_AND_ASSIGN(auto m_plaintext,
                           rlwe::testing::ConvertToMontgomery<TypeParam>(
                               plaintext, context->GetModulusParams()));
      rlwe::Polynomial<TypeParam> plaintext_ntt =
          rlwe::Polynomial<TypeParam>::ConvertToNtt(
              m_plaintext, context->GetNttParams(),
              context->GetModulusParams());
      ASSERT_OK_AND_ASSIGN(TypeParam to_absorb,
                           TypeParam::ImportInt(rand_r(&seed) % context->GetT(),
                                                context->GetModulusParams()));

      // Create our expected value.
      ASSERT_OK_AND_ASSIGN(
          rlwe::Polynomial<TypeParam> expected_ntt,
          plaintext_ntt.Mul(to_absorb, context->GetModulusParams()));
      std::vector<typename TypeParam::Int> expected =
          rlwe::RemoveError<TypeParam>(
              expected_ntt.InverseNtt(context->GetNttParams(),
                                      context->GetModulusParams()),
              context->GetModulus(), context->GetT(),
              context->GetModulusParams());

      // Encrypt, absorb, and decrypt.
      ASSERT_OK_AND_ASSIGN(auto encrypt,
                           this->Encrypt(key, plaintext, context.get()));
      ASSERT_OK_AND_ASSIGN(auto ciphertext, encrypt* to_absorb);
      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted,
                           rlwe::Decrypt<TypeParam>(key, ciphertext));

      EXPECT_EQ(expected, decrypted);
      // Expect the error to grow multiplicatively.
      EXPECT_EQ(ciphertext.Error(), context->GetErrorParams()->B_encryption() *
                                        static_cast<double>(to_absorb.ExportInt(
                                            context->GetModulusParams())));
    }
  }
}

// Check that homomorphic absorption of a scalar in place works.
TYPED_TEST(SymmetricRlweEncryptionTest, AbsorbScalarInPlace) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    unsigned int seed = 0;

    for (unsigned int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

      // Create the initial plaintexts.
      std::vector<typename TypeParam::Int> plaintext =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());
      ASSERT_OK_AND_ASSIGN(auto m_plaintext,
                           rlwe::testing::ConvertToMontgomery<TypeParam>(
                               plaintext, context->GetModulusParams()));
      rlwe::Polynomial<TypeParam> plaintext_ntt =
          rlwe::Polynomial<TypeParam>::ConvertToNtt(
              m_plaintext, context->GetNttParams(),
              context->GetModulusParams());
      ASSERT_OK_AND_ASSIGN(TypeParam to_absorb,
                           TypeParam::ImportInt(rand_r(&seed) % context->GetT(),
                                                context->GetModulusParams()));

      // Create our expected value.
      ASSERT_OK_AND_ASSIGN(
          rlwe::Polynomial<TypeParam> expected_ntt,
          plaintext_ntt.Mul(to_absorb, context->GetModulusParams()));
      std::vector<typename TypeParam::Int> expected =
          rlwe::RemoveError<TypeParam>(
              expected_ntt.InverseNtt(context->GetNttParams(),
                                      context->GetModulusParams()),
              context->GetModulus(), context->GetT(),
              context->GetModulusParams());

      // Encrypt, absorb, and decrypt.
      ASSERT_OK_AND_ASSIGN(auto ciphertext,
                           this->Encrypt(key, plaintext, context.get()));
      ASSERT_OK(ciphertext.AbsorbInPlace(to_absorb));
      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted,
                           rlwe::Decrypt<TypeParam>(key, ciphertext));

      EXPECT_EQ(expected, decrypted);
      // Expect the error to grow multiplicatively.
      EXPECT_EQ(ciphertext.Error(), context->GetErrorParams()->B_encryption() *
                                        static_cast<double>(to_absorb.ExportInt(
                                            context->GetModulusParams())));
    }
  }
}

// Check that we cannot multiply with an empty ciphertext.
TYPED_TEST(SymmetricRlweEncryptionTest, EmptyCipherMultiplication) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

    // Create a plaintext
    std::vector<typename TypeParam::Int> plaintext =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());

    // Encrypt, multiply
    ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                         this->Encrypt(key, plaintext, context.get()));

    // empty cipher
    std::vector<rlwe::Polynomial<TypeParam>> c;
    rlwe::SymmetricRlweCiphertext<TypeParam> ciphertext2(
        c, 1, 0, context->GetModulusParams(), context->GetErrorParams());

    EXPECT_THAT(
        ciphertext1 * ciphertext2,
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr("Cannot multiply using an empty ciphertext.")));
    EXPECT_THAT(
        ciphertext2 * ciphertext1,
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr("Cannot multiply using an empty ciphertext.")));

    c.push_back(rlwe::Polynomial<TypeParam>());
    rlwe::SymmetricRlweCiphertext<TypeParam> ciphertext3(
        c, 1, 0, context->GetModulusParams(), context->GetErrorParams());
    EXPECT_THAT(ciphertext1 * ciphertext3,
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("Cannot multiply using an empty polynomial "
                                   "in the ciphertext.")));

    EXPECT_THAT(ciphertext3 * ciphertext1,
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("Cannot multiply using an empty polynomial "
                                   "in the ciphertext.")));
  }
}

// Check that the scheme is multiplicatively homomorphic.
TYPED_TEST(SymmetricRlweEncryptionTest, MultiplicativelyHomomorphic) {
  if (sizeof(TypeParam) > 2) {  // No multiplicative homomorphism possible when
                                // TypeParam = Uint16
    for (const auto& params :
         rlwe::testing::ContextParameters<TypeParam>::Value()) {
      ASSERT_OK_AND_ASSIGN(auto context,
                           rlwe::RlweContext<TypeParam>::Create(params));
      for (int i = 0; i < kTestingRounds; i++) {
        ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

        // Create the initial plaintexts.
        std::vector<typename TypeParam::Int> plaintext1 =
            rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                      context->GetT());
        ASSERT_OK_AND_ASSIGN(auto mp1,
                             rlwe::testing::ConvertToMontgomery<TypeParam>(
                                 plaintext1, context->GetModulusParams()));
        rlwe::Polynomial<TypeParam> plaintext1_ntt =
            rlwe::Polynomial<TypeParam>::ConvertToNtt(
                mp1, context->GetNttParams(), context->GetModulusParams());
        std::vector<typename TypeParam::Int> plaintext2 =
            rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                      context->GetT());
        ASSERT_OK_AND_ASSIGN(auto mp2,
                             rlwe::testing::ConvertToMontgomery<TypeParam>(
                                 plaintext2, context->GetModulusParams()));
        rlwe::Polynomial<TypeParam> plaintext2_ntt =
            rlwe::Polynomial<TypeParam>::ConvertToNtt(
                mp2, context->GetNttParams(), context->GetModulusParams());

        // Encrypt, multiply, and decrypt.
        ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                             this->Encrypt(key, plaintext1, context.get()));
        ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                             this->Encrypt(key, plaintext2, context.get()));
        ASSERT_OK_AND_ASSIGN(auto product, ciphertext1* ciphertext2);
        ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted,
                             rlwe::Decrypt<TypeParam>(key, product));

        // Create the polynomial we expect.
        ASSERT_OK_AND_ASSIGN(
            rlwe::Polynomial<TypeParam> expected_ntt,
            plaintext1_ntt.Mul(plaintext2_ntt, context->GetModulusParams()));
        std::vector<typename TypeParam::Int> expected =
            rlwe::RemoveError<TypeParam>(
                expected_ntt.InverseNtt(context->GetNttParams(),
                                        context->GetModulusParams()),
                context->GetModulus(), context->GetT(),
                context->GetModulusParams());

        EXPECT_EQ(expected, decrypted);
        // Expect that the error grows multiplicatively.
        EXPECT_EQ(product.Error(), ciphertext1.Error() * ciphertext2.Error());
      }
    }
  }
}

// Check that many homomorphic additions can be performed.
TYPED_TEST(SymmetricRlweEncryptionTest, ManyHomomorphicAdds) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    // Sample a starting plaintext and ciphertext and create aggregators;
    std::vector<typename TypeParam::Int> plaintext =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());
    std::vector<typename TypeParam::Int> plaintext_sum = plaintext;
    ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));
    ASSERT_OK_AND_ASSIGN(auto ciphertext_sum,
                         this->Encrypt(key, plaintext, context.get()));

    // Sample a fresh plaintext.
    plaintext = rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                          context->GetT());
    ASSERT_OK_AND_ASSIGN(auto ciphertext,
                         this->Encrypt(key, plaintext, context.get()));

    int num_adds = 50;
    // Perform 50 homomorphic ciphertext additions with the fresh ciphertext.
    for (int j = 0; j < num_adds; j++) {
      // Add the new plaintext to the old plaintext.
      for (unsigned int k = 0; k < context->GetN(); k++) {
        plaintext_sum[k] += plaintext[k];
        plaintext_sum[k] %= context->GetT();
      }

      // Add the new ciphertext to the old ciphertext.
      ASSERT_OK_AND_ASSIGN(ciphertext_sum, ciphertext_sum + ciphertext);
    }

    ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted,
                         rlwe::Decrypt<TypeParam>(key, ciphertext_sum));

    // Ensure the values are the same.
    EXPECT_EQ(plaintext_sum, decrypted);
    // Expect that the ciphertext sum's error grows by the additively by the
    // ciphertext's error.
    EXPECT_GT(ciphertext_sum.Error(), num_adds * ciphertext.Error());
  }
}

// Check that ciphertext deserialization cannot handle more than
// rlwe::kMaxNumCoeffs coefficients.
TYPED_TEST(SymmetricRlweEncryptionTest,
           ExceedMaxNumCoeffDeserializeCiphertext) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));

    int num_coeffs = rlwe::kMaxNumCoeffs + 1;
    std::vector<rlwe::Polynomial<TypeParam>> c;
    for (int i = 0; i < num_coeffs; i++) {
      c.push_back(rlwe::Polynomial<TypeParam>(1, context->GetModulusParams()));
    }
    rlwe::SymmetricRlweCiphertext<TypeParam> ciphertext(
        c, 1, 0, context->GetModulusParams(), context->GetErrorParams());
    // Serialize and deserialize.
    ASSERT_OK_AND_ASSIGN(rlwe::SerializedSymmetricRlweCiphertext serialized,
                         ciphertext.Serialize());

    EXPECT_THAT(
        rlwe::SymmetricRlweCiphertext<TypeParam>::Deserialize(
            serialized, context->GetModulusParams(), context->GetErrorParams()),
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr(absl::StrCat(
                     "Number of coefficients, ", serialized.c_size(),
                     ", cannot be more than ", rlwe::kMaxNumCoeffs, "."))));
  }
}

// Check that ciphertext serialization works.
TYPED_TEST(SymmetricRlweEncryptionTest, SerializeCiphertext) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    for (int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));
      std::vector<typename TypeParam::Int> plaintext =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());
      ASSERT_OK_AND_ASSIGN(auto ciphertext,
                           this->Encrypt(key, plaintext, context.get()));

      // Serialize and deserialize.
      ASSERT_OK_AND_ASSIGN(rlwe::SerializedSymmetricRlweCiphertext serialized,
                           ciphertext.Serialize());
      ASSERT_OK_AND_ASSIGN(
          auto deserialized,
          rlwe::SymmetricRlweCiphertext<TypeParam>::Deserialize(
              serialized, context->GetModulusParams(),
              context->GetErrorParams()));

      // Decrypt and check equality.
      ASSERT_OK_AND_ASSIGN(
          std::vector<typename TypeParam::Int> deserialized_plaintext,
          rlwe::Decrypt<TypeParam>(key, deserialized));

      EXPECT_EQ(plaintext, deserialized_plaintext);
      // Check that the error stays the same.
      EXPECT_EQ(deserialized.Error(), ciphertext.Error());
    }
  }
}

// Check that key serialization works.
TYPED_TEST(SymmetricRlweEncryptionTest, SerializeKey) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    for (int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(auto original_key, this->SampleKey(context.get()));
      std::vector<typename TypeParam::Int> plaintext =
          rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                    context->GetT());

      // Serialize key, deserialize, and ensure the deserialized key is
      // interoperable with the original key.
      ASSERT_OK_AND_ASSIGN(rlwe::SerializedNttPolynomial serialized,
                           original_key.Serialize());
      ASSERT_OK_AND_ASSIGN(
          auto deserialized_key,
          rlwe::SymmetricRlweKey<TypeParam>::Deserialize(
              context->GetVariance(), context->GetLogT(), serialized,
              context->GetModulusParams(), context->GetNttParams()));

      // Test that a ciphertext encrypted with the original key decrypts under
      // the deserialized key.
      ASSERT_OK_AND_ASSIGN(
          auto ekey1, this->Encrypt(original_key, plaintext, context.get()));
      ASSERT_OK_AND_ASSIGN(auto dkey1,
                           rlwe::Decrypt<TypeParam>(deserialized_key, ekey1));
      EXPECT_EQ(dkey1, plaintext);

      // Test that a ciphertext encrypted with the deserialized key decrypts
      // under the original key.
      ASSERT_OK_AND_ASSIGN(auto ekey2, this->Encrypt(deserialized_key,
                                                     plaintext, context.get()));
      ASSERT_OK_AND_ASSIGN(auto dkey2,
                           rlwe::Decrypt<TypeParam>(original_key, ekey2));
      EXPECT_EQ(dkey2, plaintext);
    }
  }
}

// Try an ill-formed key modulus switching
TYPED_TEST(SymmetricRlweEncryptionTest, FailingKeyModulusReduction) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    // p is the original modulus and q is the new modulus we want to switch to
    // For modulus switching, p % t must be equal to q % t, where t is the
    // plaintext modulus, and both need to be congruent to 1 mod 2n.
    typename TypeParam::Int p = context->GetModulus();
    typename TypeParam::Int q = p - (context->GetN() << 1);
    EXPECT_NE(q % context->GetT(), p % context->GetT());

    ASSERT_OK_AND_ASSIGN(auto context_q, rlwe::RlweContext<TypeParam>::Create(
                                             {.modulus = q,
                                              .log_n = params.log_n,
                                              .log_t = params.log_t,
                                              .variance = params.variance}));

    ASSERT_OK_AND_ASSIGN(auto key_p, this->SampleKey(context.get()));
    auto status = key_p.template SwitchModulus<TypeParam>(
        context_q->GetModulusParams(), context_q->GetNttParams());
    EXPECT_THAT(status, StatusIs(::absl::StatusCode::kInvalidArgument,
                                 HasSubstr("p % t != q % t")));
  }
}

// Try an ill-formed ciphertext modulus switching
TYPED_TEST(SymmetricRlweEncryptionTest, FailingCiphertextModulusReduction) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    // p is the original modulus and q is the new modulus we want to switch to
    // For modulus switching, p % t must be equal to q % t, where t is the
    // plaintext modulus, and both need to be congruent to 1 mod 2n.
    typename TypeParam::Int p = context->GetModulus();
    typename TypeParam::Int q = p - (context->GetN() << 1);
    EXPECT_NE(q % context->GetT(), p % context->GetT());

    ASSERT_OK_AND_ASSIGN(auto context_q, rlwe::RlweContext<TypeParam>::Create(
                                             {.modulus = q,
                                              .log_n = params.log_n,
                                              .log_t = params.log_t,
                                              .variance = params.variance}));

    ASSERT_OK_AND_ASSIGN(auto key_p, this->SampleKey(context.get()));
    // sample ciphertext modulo p.
    auto plaintext = rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                               context->GetT());
    ASSERT_OK_AND_ASSIGN(auto ciphertext,
                         this->Encrypt(key_p, plaintext, context.get()));

    auto status = ciphertext.template SwitchModulus<TypeParam>(
        context->GetNttParams(), context_q->GetModulusParams(),
        context_q->GetNttParams(), context_q->GetErrorParams(),
        context->GetT());

    EXPECT_THAT(status, StatusIs(::absl::StatusCode::kInvalidArgument,
                                 HasSubstr("p % t != q % t")));
  }
}

// Test modulus switching.
TYPED_TEST(SymmetricRlweEncryptionTest, ModulusReduction) {
  for (const auto& params :
       rlwe::testing::ContextParametersModulusSwitching<TypeParam>::Value()) {
    auto params1 = std::get<0>(params), params2 = std::get<1>(params);
    ASSERT_OK_AND_ASSIGN(auto context1,
                         rlwe::RlweContext<TypeParam>::Create(params1));
    ASSERT_OK_AND_ASSIGN(auto context2,
                         rlwe::RlweContext<TypeParam>::Create(params2));

    for (int i = 0; i < kTestingRounds; i++) {
      // Create a key.
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context1.get()));
      ASSERT_OK_AND_ASSIGN(
          auto key_switched,
          key.template SwitchModulus<TypeParam>(context2->GetModulusParams(),
                                                context2->GetNttParams()));

      // Create a plaintext.
      std::vector<typename TypeParam::Int> plaintext =
          rlwe::testing::SamplePlaintext<TypeParam>(context1->GetN(),
                                                    context1->GetT());
      ASSERT_OK_AND_ASSIGN(auto ciphertext,
                           this->Encrypt(key, plaintext, context1.get()));

      // Switch moduli.
      ASSERT_OK_AND_ASSIGN(
          auto ciphertext_switched,
          ciphertext.template SwitchModulus<TypeParam>(
              context1->GetNttParams(), context2->GetModulusParams(),
              context2->GetNttParams(), context2->GetErrorParams(),
              context2->GetT()));

      // Decrypt in the smaller modulus.
      ASSERT_OK_AND_ASSIGN(
          auto decrypted,
          rlwe::Decrypt<TypeParam>(key_switched, ciphertext_switched));

      EXPECT_EQ(plaintext, decrypted);
    }
  }
}

// Check that modulus switching reduces the error.
TYPED_TEST(SymmetricRlweEncryptionTest, ModulusSwitchingReducesLargeError) {
  for (const auto& params :
       rlwe::testing::ContextParametersModulusSwitching<TypeParam>::Value()) {
    auto params1 = std::get<0>(params), params2 = std::get<1>(params);
    ASSERT_OK_AND_ASSIGN(auto context1,
                         rlwe::RlweContext<TypeParam>::Create(params1));
    ASSERT_OK_AND_ASSIGN(auto context2,
                         rlwe::RlweContext<TypeParam>::Create(params2));

    for (int i = 0; i < kTestingRounds; i++) {
      // Create a key.
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context1.get()));
      ASSERT_OK_AND_ASSIGN(
          auto key_switched,
          key.template SwitchModulus<TypeParam>(context2->GetModulusParams(),
                                                context2->GetNttParams()));

      // Create a plaintext.
      std::vector<typename TypeParam::Int> plaintext =
          rlwe::testing::SamplePlaintext<TypeParam>(context1->GetN(),
                                                    context1->GetT());
      ASSERT_OK_AND_ASSIGN(auto ciphertext,
                           this->Encrypt(key, plaintext, context1.get()));

      // Square the ciphertext
      ASSERT_OK_AND_ASSIGN(auto squared, ciphertext* ciphertext);

      // Switch moduli.
      ASSERT_OK_AND_ASSIGN(
          auto squared_switched,
          squared.template SwitchModulus<TypeParam>(
              context1->GetNttParams(), context2->GetModulusParams(),
              context2->GetNttParams(), context2->GetErrorParams(),
              context2->GetT()));

      // Decrypt
      ASSERT_OK_AND_ASSIGN(auto squared_decrypted,
                           rlwe::Decrypt<TypeParam>(key, squared));
      ASSERT_OK_AND_ASSIGN(
          auto squared_switched_decrypted,
          rlwe::Decrypt<TypeParam>(key_switched, squared_switched));

      EXPECT_EQ(squared_decrypted, squared_switched_decrypted);

      // Expect that the error reduces after a modulus switch when the error is
      // large.
      EXPECT_LT(squared_switched.Error(), squared.Error());
      // But that the error doesn't reduce when the error is small.
      EXPECT_GT(squared_switched.Error(), ciphertext.Error());
    }
  }
}

// Check that we cannot perform operations between ciphertexts encrypted under
// different powers of s.
TYPED_TEST(SymmetricRlweEncryptionTest, OperationsFailOnMismatchedPowersOfS) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

    std::vector<typename TypeParam::Int> plaintext1 =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());
    ASSERT_OK_AND_ASSIGN(auto m1, rlwe::testing::ConvertToMontgomery<TypeParam>(
                                      plaintext1, context->GetModulusParams()));
    auto plaintext1_ntt = rlwe::Polynomial<TypeParam>::ConvertToNtt(
        m1, context->GetNttParams(), context->GetModulusParams());
    std::vector<typename TypeParam::Int> plaintext2 =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());

    auto ciphertext1 = rlwe::SymmetricRlweCiphertext<TypeParam>(
        {plaintext1_ntt}, 1, context->GetErrorParams()->B_encryption(),
        context->GetModulusParams(), context->GetErrorParams());
    auto ciphertext2 = rlwe::SymmetricRlweCiphertext<TypeParam>(
        {plaintext1_ntt}, 2, context->GetErrorParams()->B_encryption(),
        context->GetModulusParams(), context->GetErrorParams());
    EXPECT_THAT(ciphertext1 + ciphertext2,
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("must be encrypted with the same key")));
    EXPECT_THAT(ciphertext1 * ciphertext2,
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("must be encrypted with the same key")));
  }
}

// Verifies that the power of S changes as expected in adds / mults.
TYPED_TEST(SymmetricRlweEncryptionTest, AddsAndMultPreservePowerOfS) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

    std::vector<typename TypeParam::Int> plaintext1 =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());
    ASSERT_OK_AND_ASSIGN(auto m1, rlwe::testing::ConvertToMontgomery<TypeParam>(
                                      plaintext1, context->GetModulusParams()));
    auto plaintext1_ntt = rlwe::Polynomial<TypeParam>::ConvertToNtt(
        m1, context->GetNttParams(), context->GetModulusParams());
    std::vector<typename TypeParam::Int> plaintext2 =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());

    auto ciphertext1 = rlwe::SymmetricRlweCiphertext<TypeParam>(
        {plaintext1_ntt}, 2, context->GetErrorParams()->B_encryption(),
        context->GetModulusParams(), context->GetErrorParams());
    auto ciphertext2 = rlwe::SymmetricRlweCiphertext<TypeParam>(
        {plaintext1_ntt}, 2, context->GetErrorParams()->B_encryption(),
        context->GetModulusParams(), context->GetErrorParams());

    EXPECT_EQ(ciphertext1.PowerOfS(), 2);
    EXPECT_EQ(ciphertext2.PowerOfS(), 2);
    ASSERT_OK_AND_ASSIGN(auto sum, ciphertext1 + ciphertext2);
    EXPECT_EQ(sum.PowerOfS(), 2);
    ASSERT_OK_AND_ASSIGN(auto prod, ciphertext1* ciphertext2);
    EXPECT_EQ(prod.PowerOfS(), 2);
  }
}

// Check that substitutions of the form 2^k + 1 work.
TYPED_TEST(SymmetricRlweEncryptionTest, Substitutes) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    for (int k = 1; k < context->GetLogN(); k++) {
      int substitution_power = (1 << k) + 1;
      ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));
      auto plaintext = rlwe::testing::SamplePlaintext<TypeParam>(
          context->GetN(), context->GetT());

      // Create the expected polynomial output by substituting the plaintext.
      ASSERT_OK_AND_ASSIGN(auto m_plaintext,
                           rlwe::testing::ConvertToMontgomery<TypeParam>(
                               plaintext, context->GetModulusParams()));
      auto plaintext_ntt = rlwe::Polynomial<TypeParam>::ConvertToNtt(
          m_plaintext, context->GetNttParams(), context->GetModulusParams());
      ASSERT_OK_AND_ASSIGN(
          auto expected_ntt,
          plaintext_ntt.Substitute(substitution_power, context->GetNttParams(),
                                   context->GetModulusParams()));
      std::vector<typename TypeParam::Int> expected =
          rlwe::RemoveError<TypeParam>(
              expected_ntt.InverseNtt(context->GetNttParams(),
                                      context->GetModulusParams()),
              context->GetModulus(), context->GetT(),
              context->GetModulusParams());

      // Encrypt and substitute the ciphertext. Decrypt with a substituted key.
      ASSERT_OK_AND_ASSIGN(auto ciphertext,
                           this->Encrypt(key, plaintext, context.get()));
      ASSERT_OK_AND_ASSIGN(
          auto substituted,
          ciphertext.Substitute(substitution_power, context->GetNttParams()));
      ASSERT_OK_AND_ASSIGN(auto key_sub, key.Substitute(substitution_power));
      ASSERT_OK_AND_ASSIGN(std::vector<typename TypeParam::Int> decrypted,
                           rlwe::Decrypt<TypeParam>(key_sub, substituted));

      EXPECT_EQ(decrypted, expected);
      EXPECT_EQ(substituted.PowerOfS(), substitution_power);
      EXPECT_EQ(substituted.Error(), ciphertext.Error());
    }
  }
}

// Check that substitution of 2 does not work.
TYPED_TEST(SymmetricRlweEncryptionTest, SubstitutionFailsOnEvenPower) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));
    auto plaintext = rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                               context->GetT());

    ASSERT_OK_AND_ASSIGN(auto enc,
                         this->Encrypt(key, plaintext, context.get()));
    EXPECT_THAT(
        enc.Substitute(2, context->GetNttParams()),
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr("power must be a non-negative odd integer")));
  }
}

// Check that the power of s updates after several substitutions.
TYPED_TEST(SymmetricRlweEncryptionTest, PowerOfSUpdatedAfterRepeatedSubs) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    int substitution_power = 5;
    ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));
    auto plaintext = rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                               context->GetT());

    // Encrypt and substitute the ciphertext. Decrypt with a substituted key.
    ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                         this->Encrypt(key, plaintext, context.get()));
    ASSERT_OK_AND_ASSIGN(
        auto ciphertext2,
        ciphertext1.Substitute(substitution_power, context->GetNttParams()));
    ASSERT_OK_AND_ASSIGN(
        auto ciphertext3,
        ciphertext2.Substitute(substitution_power, context->GetNttParams()));
    EXPECT_EQ(ciphertext3.PowerOfS(),
              (substitution_power * substitution_power) % (2 * key.Len()));
  }
}

// Check that operations can only be performed when powers of s match.
TYPED_TEST(SymmetricRlweEncryptionTest, PowersOfSMustMatchOnOperations) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    int substitution_power = 5;
    ASSERT_OK_AND_ASSIGN(auto key, this->SampleKey(context.get()));

    std::vector<typename TypeParam::Int> plaintext1 =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());
    std::vector<typename TypeParam::Int> plaintext2 =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());

    ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                         this->Encrypt(key, plaintext1, context.get()));
    ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                         this->Encrypt(key, plaintext2, context.get()));
    ASSERT_OK_AND_ASSIGN(
        auto ciphertext2_sub,
        ciphertext2.Substitute(substitution_power, context->GetNttParams()));

    EXPECT_THAT(ciphertext1 + ciphertext2_sub,
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("must be encrypted with the same key")));
    EXPECT_THAT(ciphertext1 * ciphertext2_sub,
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("must be encrypted with the same key")));
  }
}

// Check that the null key has value 0.
TYPED_TEST(SymmetricRlweEncryptionTest, NullKeyHasValueZero) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    rlwe::Polynomial<TypeParam> zero(context->GetN(),
                                     context->GetModulusParams());

    ASSERT_OK_AND_ASSIGN(
        auto null_key,
        rlwe::SymmetricRlweKey<TypeParam>::NullKey(
            context->GetLogN(), context->GetVariance(), context->GetLogT(),
            context->GetModulusParams(), context->GetNttParams()));

    EXPECT_THAT(zero, Eq(null_key.Key()));
  }
}

// Check the addition and subtraction of keys.
TYPED_TEST(SymmetricRlweEncryptionTest, AddAndSubKeys) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto key_1, this->SampleKey(context.get()));
    ASSERT_OK_AND_ASSIGN(auto key_2, this->SampleKey(context.get()));

    ASSERT_OK_AND_ASSIGN(auto key_3, key_1.Add(key_2));
    ASSERT_OK_AND_ASSIGN(auto key_4, key_1.Sub(key_2));

    ASSERT_OK_AND_ASSIGN(
        rlwe::Polynomial<TypeParam> poly_3,
        key_1.Key().Add(key_2.Key(), context->GetModulusParams()));
    ASSERT_OK_AND_ASSIGN(
        rlwe::Polynomial<TypeParam> poly_4,
        key_1.Key().Sub(key_2.Key(), context->GetModulusParams()));

    EXPECT_THAT(key_3.Key(), Eq(poly_3));
    EXPECT_THAT(key_4.Key(), Eq(poly_4));
  }
}

// Check that decryption works with added and subtracted keys.
TYPED_TEST(SymmetricRlweEncryptionTest, EncryptAndDecryptWithAddAndSubKeys) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto key_1, this->SampleKey(context.get()));
    ASSERT_OK_AND_ASSIGN(auto key_2, this->SampleKey(context.get()));
    ASSERT_OK_AND_ASSIGN(auto add_keys, key_1.Add(key_2));
    ASSERT_OK_AND_ASSIGN(auto sub_keys, key_1.Sub(key_2));
    std::vector<typename TypeParam::Int> plaintext =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());

    ASSERT_OK_AND_ASSIGN(auto add_ciphertext,
                         this->Encrypt(add_keys, plaintext, context.get()));
    ASSERT_OK_AND_ASSIGN(auto sub_ciphertext,
                         this->Encrypt(sub_keys, plaintext, context.get()));
    ASSERT_OK_AND_ASSIGN(auto decrypted_add_ciphertext,
                         rlwe::Decrypt(add_keys, add_ciphertext));
    ASSERT_OK_AND_ASSIGN(auto decrypted_sub_ciphertext,
                         rlwe::Decrypt(sub_keys, sub_ciphertext));

    EXPECT_EQ(plaintext, decrypted_add_ciphertext);
    EXPECT_EQ(plaintext, decrypted_sub_ciphertext);
  }
}

// Check that the scheme is key homomorphic.
TYPED_TEST(SymmetricRlweEncryptionTest, IsKeyHomomorphic) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto prng_seed,
                         rlwe::SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    // Generate the keys.
    ASSERT_OK_AND_ASSIGN(auto key_1, this->SampleKey(context.get()));
    ASSERT_OK_AND_ASSIGN(auto key_2, this->SampleKey(context.get()));
    // Generate the plaintexts.
    std::vector<typename TypeParam::Int> plaintext_1 =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());
    std::vector<typename TypeParam::Int> plaintext_2 =
        rlwe::testing::SamplePlaintext<TypeParam>(context->GetN(),
                                                  context->GetT());
    ASSERT_OK_AND_ASSIGN(auto plaintext_mont_1,
                         rlwe::testing::ConvertToMontgomery<TypeParam>(
                             plaintext_1, context->GetModulusParams()));
    ASSERT_OK_AND_ASSIGN(auto plaintext_mont_2,
                         rlwe::testing::ConvertToMontgomery<TypeParam>(
                             plaintext_2, context->GetModulusParams()));
    auto poly_1 = rlwe::Polynomial<TypeParam>::ConvertToNtt(
        plaintext_mont_1, context->GetNttParams(), context->GetModulusParams());
    auto poly_2 = rlwe::Polynomial<TypeParam>::ConvertToNtt(
        plaintext_mont_2, context->GetNttParams(), context->GetModulusParams());
    // Compute the expected plaintexts.
    std::vector<typename TypeParam::Int> add_plaintext = plaintext_1;
    std::vector<typename TypeParam::Int> sub_plaintext = plaintext_2;
    std::transform(
        plaintext_1.begin(), plaintext_1.end(), plaintext_2.begin(),
        add_plaintext.begin(),
        [&context = context](typename TypeParam::Int u,
                             typename TypeParam::Int v) ->
        typename TypeParam::Int { return (u + v) % context->GetT(); });
    std::transform(plaintext_1.begin(), plaintext_1.end(), plaintext_2.begin(),
                   sub_plaintext.begin(),
                   [&context = context](typename TypeParam::Int u,
                                        typename TypeParam::Int v) ->
                   typename TypeParam::Int {
                     return (context->GetT() + u - v) % context->GetT();
                   });

    // Sample the "a" to be used in both ciphertexts.
    ASSERT_OK_AND_ASSIGN(auto a,
                         rlwe::SamplePolynomialFromPrng<TypeParam>(
                             key_1.Len(), prng.get(), key_1.ModulusParams()));
    // Encrypt with the same a and different keys
    ASSERT_OK_AND_ASSIGN(auto poly_ciphertext_1,
                         rlwe::internal::Encrypt(key_1, poly_1, a, prng.get()));
    ASSERT_OK_AND_ASSIGN(auto poly_ciphertext_2,
                         rlwe::internal::Encrypt(key_2, poly_2, a, prng.get()));
    // Add and Substract the ciphertexts
    ASSERT_OK_AND_ASSIGN(
        auto add_poly_ciphertext,
        poly_ciphertext_1.Add(poly_ciphertext_2, context->GetModulusParams()));
    ASSERT_OK_AND_ASSIGN(
        auto sub_poly_ciphertext,
        poly_ciphertext_1.Sub(poly_ciphertext_2, context->GetModulusParams()));
    // The resulting ciphertexts should be decryptable unded the added (resp.
    // substracted) keys.
    ASSERT_OK_AND_ASSIGN(auto add_keys, key_1.Add(key_2));
    ASSERT_OK_AND_ASSIGN(auto sub_keys, key_1.Sub(key_2));
    ASSERT_OK_AND_ASSIGN(
        auto decrypted_add_ciphertext,
        rlwe::Decrypt(
            add_keys,
            rlwe::SymmetricRlweCiphertext<TypeParam>(
                {add_poly_ciphertext, a.Negate(context->GetModulusParams())}, 1,
                context->GetErrorParams()->B_encryption(),
                context->GetModulusParams(), context->GetErrorParams())));
    ASSERT_OK_AND_ASSIGN(
        auto decrypted_sub_ciphertext,
        rlwe::Decrypt(
            sub_keys,
            rlwe::SymmetricRlweCiphertext<TypeParam>(
                {sub_poly_ciphertext, a.Negate(context->GetModulusParams())}, 1,
                context->GetErrorParams()->B_encryption(),
                context->GetModulusParams(), context->GetErrorParams())));

    EXPECT_EQ(add_plaintext, decrypted_add_ciphertext);
    EXPECT_EQ(sub_plaintext, decrypted_sub_ciphertext);
  }
}

// Check that incompatible key cannot be added or subtracted.
TYPED_TEST(SymmetricRlweEncryptionTest, CannotAddOrSubIncompatibleKeys) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto context,
                         rlwe::RlweContext<TypeParam>::Create(params));
    ASSERT_OK_AND_ASSIGN(auto context_different_variance,
                         rlwe::RlweContext<TypeParam>::Create(
                             {.modulus = params.modulus,
                              .log_n = params.log_n,
                              .log_t = params.log_t,
                              .variance = params.variance + 1}));
    ASSERT_OK_AND_ASSIGN(
        auto context_different_log_t,
        rlwe::RlweContext<TypeParam>::Create({.modulus = params.modulus,
                                              .log_n = params.log_n,
                                              .log_t = params.log_t + 1,
                                              .variance = params.variance}));
    ASSERT_OK_AND_ASSIGN(auto key_1, this->SampleKey(context.get()));
    ASSERT_OK_AND_ASSIGN(auto key_2,
                         this->SampleKey(context_different_variance.get()));
    ASSERT_OK_AND_ASSIGN(auto key_3,
                         this->SampleKey(context_different_log_t.get()));

    EXPECT_THAT(
        key_1.Add(key_2),
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr("is different than the variance of this key")));
    EXPECT_THAT(
        key_1.Sub(key_2),
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr("is different than the variance of this key")));
    EXPECT_THAT(key_1.Add(key_3),
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("is different than the log_t of this key")));
    EXPECT_THAT(key_1.Sub(key_3),
                StatusIs(::absl::StatusCode::kInvalidArgument,
                         HasSubstr("is different than the log_t of this key")));
  }
}

}  // namespace
