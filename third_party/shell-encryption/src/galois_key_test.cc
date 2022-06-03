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

#include "galois_key.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <google/protobuf/util/message_differencer.h>
#include "constants.h"
#include "montgomery.h"
#include "ntt_parameters.h"
#include "polynomial.h"
#include "prng/integral_prng_types.h"
#include "status_macros.h"
#include "symmetric_encryption.h"
#include "testing/protobuf_matchers.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"
#include "testing/testing_utils.h"

namespace {

using Uint64 = rlwe::Uint64;

unsigned int seed = 0;

// Set constants.
const Uint64 kLogPlaintextModulus = 1;
const Uint64 kPlaintextModulus = (1 << kLogPlaintextModulus) + 1;
const Uint64 kLogDecompositionModulus = 2;
const Uint64 kLargeLogDecompositionModulus = 31;

// Useful typedefs.
using uint_m = rlwe::MontgomeryInt<Uint64>;
using Polynomial = rlwe::Polynomial<uint_m>;
using Ciphertext = rlwe::SymmetricRlweCiphertext<uint_m>;
using Key = rlwe::SymmetricRlweKey<uint_m>;

using ::rlwe::testing::EqualsProto;
using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

// Test fixture.
class GaloisKeyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(params59_, uint_m::Params::Create(rlwe::kModulus59));
    ASSERT_OK_AND_ASSIGN(auto ntt_params,
                         rlwe::InitializeNttParameters<uint_m>(
                             rlwe::testing::kLogCoeffs, params59_.get()));
    ntt_params_ = absl::make_unique<const rlwe::NttParameters<uint_m>>(
        std::move(ntt_params));
    ASSERT_OK_AND_ASSIGN(
        auto error_params,
        rlwe::ErrorParams<uint_m>::Create(rlwe::testing::kDefaultLogT,
                                          rlwe::testing::kDefaultVariance,
                                          params59_.get(), ntt_params_.get()));
    error_params_ =
        absl::make_unique<const rlwe::ErrorParams<uint_m>>(error_params);
  }

  // Sample a random key.
  rlwe::StatusOr<Key> SampleKey(
      Uint64 variance = rlwe::testing::kDefaultVariance,
      Uint64 log_t = kLogPlaintextModulus) {
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return Key::Sample(rlwe::testing::kLogCoeffs, variance, log_t,
                       params59_.get(), ntt_params_.get(), prng.get());
  }

  // Convert a vector of integers to a vector of montgomery integers.
  rlwe::StatusOr<std::vector<uint_m>> ConvertToMontgomery(
      const std::vector<uint_m::Int>& coeffs, const uint_m::Params* params) {
    std::vector<uint_m> output(coeffs.size(), uint_m::ImportZero(params));
    for (unsigned int i = 0; i < output.size(); i++) {
      RLWE_ASSIGN_OR_RETURN(output[i], uint_m::ImportInt(coeffs[i], params));
    }
    return output;
  }

  // Sample a random plaintext.
  std::vector<uint_m::Int> SamplePlaintext(
      uint_m::Int t = kPlaintextModulus,
      Uint64 coeffs = rlwe::testing::kCoeffs) {
    std::vector<uint_m::Int> plaintext(coeffs);
    for (unsigned int i = 0; i < coeffs; i++) {
      plaintext[i] = rand_r(&seed) % t;
    }
    return plaintext;
  }

  // Encrypt a plaintext.
  rlwe::StatusOr<Ciphertext> Encrypt(
      const Key& key, const std::vector<uint_m::Int>& plaintext) {
    RLWE_ASSIGN_OR_RETURN(auto mp,
                          ConvertToMontgomery(plaintext, params59_.get()));
    auto plaintext_ntt =
        Polynomial::ConvertToNtt(mp, ntt_params_.get(), params59_.get());
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return rlwe::Encrypt<uint_m>(key, plaintext_ntt, error_params_.get(),
                                 prng.get());
  }

  std::unique_ptr<const uint_m::Params> params59_;
  std::unique_ptr<const rlwe::NttParameters<uint_m>> ntt_params_;
  std::unique_ptr<const rlwe::ErrorParams<uint_m>> error_params_;
};

TEST_F(GaloisKeyTest, GaloisKeyPowerOfSDoesNotMatchSubPower) {
  int substitution_power = 3;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(auto galois_key, rlwe::GaloisKey<uint_m>::Create(
                                            key, prng_seed, substitution_power,
                                            kLargeLogDecompositionModulus));
  auto plaintext = SamplePlaintext(kPlaintextModulus);

  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(
      auto subbed_ciphertext,
      ciphertext.Substitute(substitution_power + 2, ntt_params_.get()));
  EXPECT_THAT(
      galois_key.ApplyTo(subbed_ciphertext),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr(absl::StrCat(
                   "Ciphertext PowerOfS: ", subbed_ciphertext.PowerOfS(),
                   " doesn't match the key substitution power: ",
                   substitution_power))));
}

TEST_F(GaloisKeyTest, GaloisKeyUpdatesPowerOfS) {
  int substitution_power = 3;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(auto galois_key, rlwe::GaloisKey<uint_m>::Create(
                                            key, prng_seed, substitution_power,
                                            kLargeLogDecompositionModulus));
  auto plaintext = SamplePlaintext(kPlaintextModulus);

  // Substituted ciphertext has substition_power PowerOfS.
  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(
      auto subbed_ciphertext,
      ciphertext.Substitute(substitution_power, ntt_params_.get()));
  EXPECT_EQ(subbed_ciphertext.PowerOfS(), substitution_power);

  // PowerOfS transformed back to 1.
  ASSERT_OK_AND_ASSIGN(auto transformed_ciphertext,
                       galois_key.ApplyTo(subbed_ciphertext));
  EXPECT_EQ(transformed_ciphertext.PowerOfS(), 1);
}

TEST_F(GaloisKeyTest, KeySwitchedCiphertextDecrypts) {
  int substitution_power = 3;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(auto galois_key, rlwe::GaloisKey<uint_m>::Create(
                                            key, prng_seed, substitution_power,
                                            kLogDecompositionModulus));

  // Create the initial plaintexts.
  std::vector<uint_m::Int> plaintext = SamplePlaintext(kPlaintextModulus);

  // Create the expected polynomial output by substituting the plaintext.
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext, params59_.get()));
  Polynomial plaintext_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params_.get(), params59_.get());
  ASSERT_OK_AND_ASSIGN(
      Polynomial expected_ntt,
      plaintext_ntt.Substitute(substitution_power, ntt_params_.get(),
                               params59_.get()));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      expected_ntt.InverseNtt(ntt_params_.get(), params59_.get()),
      params59_->modulus, kPlaintextModulus, params59_.get());

  // Encrypt and substitute the ciphertext. Decrypt with a substituted key.
  ASSERT_OK_AND_ASSIGN(auto intermediate, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(
      auto ciphertext,
      intermediate.Substitute(substitution_power, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto transformed_ciphertext,
                       galois_key.ApplyTo(ciphertext));
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, transformed_ciphertext));

  EXPECT_EQ(decrypted, expected);
}

TEST_F(GaloisKeyTest, ComposingSubstitutions) {
  // Ensure that a ciphertext can be substituted by composing substitutions in
  // steps that have GaloisKeys.
  int substitution_power = 9;
  // Applying the substitution s -> s(x^3) twice will yield the substitution
  // power.
  int galois_power = 3;

  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(auto galois_key, rlwe::GaloisKey<uint_m>::Create(
                                            key, prng_seed, galois_power,
                                            kLogDecompositionModulus));
  auto plaintext = SamplePlaintext(kPlaintextModulus);

  // Create the expected polynomial output by substituting the plaintext.
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext, params59_.get()));
  Polynomial plaintext_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params_.get(), params59_.get());
  ASSERT_OK_AND_ASSIGN(
      Polynomial expected_ntt,
      plaintext_ntt.Substitute(substitution_power, ntt_params_.get(),
                               params59_.get()));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      expected_ntt.InverseNtt(ntt_params_.get(), params59_.get()),
      params59_->modulus, kPlaintextModulus, params59_.get());

  // Encrypt and substitute the ciphertext in steps using a single galois key.
  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(auto sub_ciphertext,
                       ciphertext.Substitute(galois_power, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext_power_3,
                       galois_key.ApplyTo(sub_ciphertext));
  ASSERT_OK_AND_ASSIGN(
      auto sub_ciphertext_power_3,
      ciphertext_power_3.Substitute(galois_power, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext_power_9,
                       galois_key.ApplyTo(sub_ciphertext_power_3));

  EXPECT_EQ(ciphertext_power_9.PowerOfS(), 1);
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, ciphertext_power_9));
  EXPECT_EQ(decrypted, expected);
}

TEST_F(GaloisKeyTest, LargeDecompositionModulus) {
  int substitution_power = 3;

  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(auto galois_key, rlwe::GaloisKey<uint_m>::Create(
                                            key, prng_seed, substitution_power,
                                            kLargeLogDecompositionModulus));
  auto plaintext = SamplePlaintext(kPlaintextModulus);

  // Create the expected polynomial output by substituting the plaintext.
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext, params59_.get()));
  Polynomial plaintext_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params_.get(), params59_.get());
  ASSERT_OK_AND_ASSIGN(
      Polynomial expected_ntt,
      plaintext_ntt.Substitute(substitution_power, ntt_params_.get(),
                               params59_.get()));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      expected_ntt.InverseNtt(ntt_params_.get(), params59_.get()),
      params59_->modulus, kPlaintextModulus, params59_.get());

  // Encrypt and substitute the ciphertext. Decrypt with a substituted key.
  ASSERT_OK_AND_ASSIGN(auto intermediate, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(
      auto ciphertext,
      intermediate.Substitute(substitution_power, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto transformed_ciphertext,
                       galois_key.ApplyTo(ciphertext));
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, transformed_ciphertext));

  EXPECT_EQ(decrypted, expected);
}

TEST_F(GaloisKeyTest, CiphertextWithTooManyComponents) {
  int substitution_power = 3;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(auto galois_key, rlwe::GaloisKey<uint_m>::Create(
                                            key, prng_seed, substitution_power,
                                            kLargeLogDecompositionModulus));
  auto plaintext = SamplePlaintext(kPlaintextModulus);

  ASSERT_OK_AND_ASSIGN(auto intermediate, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(
      auto ciphertext,
      intermediate.Substitute(substitution_power, ntt_params_.get()));

  ASSERT_OK_AND_ASSIGN(auto product, ciphertext* ciphertext);
  EXPECT_THAT(galois_key.ApplyTo(product),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("RelinearizationKey not large enough")));
}

TEST_F(GaloisKeyTest, DeserializedKeySwitches) {
  int substitution_power = 3;
  auto plaintext = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(auto galois_key, rlwe::GaloisKey<uint_m>::Create(
                                            key, prng_seed, substitution_power,
                                            kLargeLogDecompositionModulus));

  // Serialize and deserialize.
  ASSERT_OK_AND_ASSIGN(auto serialized, galois_key.Serialize());
  ASSERT_OK_AND_ASSIGN(auto deserialized,
                       rlwe::GaloisKey<uint_m>::Deserialize(
                           serialized, params59_.get(), ntt_params_.get()));

  // Create the expected polynomial output by substituting the plaintext.
  ASSERT_OK_AND_ASSIGN(auto mp,
                       ConvertToMontgomery(plaintext, params59_.get()));
  Polynomial plaintext_ntt =
      Polynomial::ConvertToNtt(mp, ntt_params_.get(), params59_.get());
  ASSERT_OK_AND_ASSIGN(
      Polynomial expected_ntt,
      plaintext_ntt.Substitute(substitution_power, ntt_params_.get(),
                               params59_.get()));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      expected_ntt.InverseNtt(ntt_params_.get(), params59_.get()),
      params59_->modulus, kPlaintextModulus, params59_.get());

  // Encrypt and substitute the ciphertext.
  ASSERT_OK_AND_ASSIGN(auto intermediate, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(
      auto ciphertext,
      intermediate.Substitute(substitution_power, ntt_params_.get()));

  // Key-switch with the original galois key.
  ASSERT_OK_AND_ASSIGN(auto key_switched_ciphertext,
                       galois_key.ApplyTo(ciphertext));
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, key_switched_ciphertext));

  // Key-switch with the deserialized galois key.
  ASSERT_OK_AND_ASSIGN(auto key_switched_ciphertext_deserialized,
                       deserialized.ApplyTo(ciphertext));
  ASSERT_OK_AND_ASSIGN(
      std::vector<uint_m::Int> deserialized_decrypted,
      rlwe::Decrypt<uint_m>(key, key_switched_ciphertext_deserialized));

  EXPECT_EQ(deserialized_decrypted, expected);
  EXPECT_EQ(deserialized_decrypted, decrypted);
}

TEST_F(GaloisKeyTest, DeserializationFailsWithIncorrectModulus) {
  int substitution_power = 3;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(auto galois_key, rlwe::GaloisKey<uint_m>::Create(
                                            key, prng_seed, substitution_power,
                                            kLargeLogDecompositionModulus));

  ASSERT_OK_AND_ASSIGN(auto params29, uint_m::Params::Create(rlwe::kModulus29));
  // Serialize and deserialize.
  ASSERT_OK_AND_ASSIGN(auto serialized, galois_key.Serialize());
  EXPECT_THAT(
      rlwe::GaloisKey<uint_m>::Deserialize(serialized, params29.get(),
                                           ntt_params_.get()),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr(absl::StrCat(
                   "Log decomposition modulus, ", kLargeLogDecompositionModulus,
                   ", must be at most: ", params29->log_modulus, "."))));
}

TEST_F(GaloisKeyTest, SerializationsOfIdentialKeysEqual) {
  int substitution_power = 3;
  auto plaintext = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(auto galois_key, rlwe::GaloisKey<uint_m>::Create(
                                            key, prng_seed, substitution_power,
                                            kLargeLogDecompositionModulus));
  auto galois_key_copy = galois_key;

  // Serialize both matrices.
  ASSERT_OK_AND_ASSIGN(auto serialized, galois_key.Serialize());
  ASSERT_OK_AND_ASSIGN(auto serialized_copy, galois_key_copy.Serialize());

  // Check that two serializations of the same matrix are equal.
  EXPECT_EQ(serialized_copy.SerializeAsString(), serialized.SerializeAsString());
}

}  //  namespace
