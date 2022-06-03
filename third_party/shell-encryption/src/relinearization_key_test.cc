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

#include "relinearization_key.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "constants.h"
#include "montgomery.h"
#include "ntt_parameters.h"
#include "polynomial.h"
#include "prng/integral_prng_types.h"
#include "status_macros.h"
#include "symmetric_encryption.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"

namespace {

unsigned int seed = 1;

// Useful typedefs.
using uint_m = rlwe::MontgomeryInt<absl::uint128>;
using Polynomial = rlwe::Polynomial<uint_m>;
using Ciphertext = rlwe::SymmetricRlweCiphertext<uint_m>;
using Key = rlwe::SymmetricRlweKey<uint_m>;
using RelinearizationKey = rlwe::RelinearizationKey<uint_m>;
using ErrorParams = rlwe::ErrorParams<uint_m>;

// Set constants.
const ssize_t kLogPlaintextModulus = 1;
const ssize_t kPlaintextModulus = (1 << kLogPlaintextModulus) + 1;
const ssize_t kDefaultVariance = 4;
const ssize_t kCoeffs = rlwe::kNewhopeDegreeBound;
const ssize_t kLogCoeffs = rlwe::kNewhopeLogDegreeBound;
const ssize_t kSmallLogDecompositionModulus = 2;
const ssize_t kLargeLogDecompositionModulus = 20;

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

// Test fixture.
class RelinearizationKeyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(params14_,
                         uint_m::Params::Create(rlwe::kNewhopeModulus));
    ASSERT_OK_AND_ASSIGN(params80_, uint_m::Params::Create(rlwe::kModulus80));
    ASSERT_OK_AND_ASSIGN(auto ntt_params, rlwe::InitializeNttParameters<uint_m>(
                                              kLogCoeffs, params14_.get()));
    ASSERT_OK_AND_ASSIGN(
        auto ntt_params80,
        rlwe::InitializeNttParameters<uint_m>(kLogCoeffs, params80_.get()));
    ntt_params_ = absl::make_unique<const rlwe::NttParameters<uint_m>>(
        std::move(ntt_params));
    ntt_params80_ = absl::make_unique<const rlwe::NttParameters<uint_m>>(
        std::move(ntt_params80));
    ASSERT_OK_AND_ASSIGN(auto error_params,
                         rlwe::ErrorParams<uint_m>::Create(
                             kLogPlaintextModulus, kDefaultVariance,
                             params14_.get(), ntt_params_.get()));
    error_params_ = absl::make_unique<const ErrorParams>(error_params);
    ASSERT_OK_AND_ASSIGN(auto error_params80,
                         rlwe::ErrorParams<uint_m>::Create(
                             kLogPlaintextModulus, kDefaultVariance,
                             params80_.get(), ntt_params80_.get()));
    error_params80_ = absl::make_unique<const ErrorParams>(error_params80);
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

  // Sample a random key.
  rlwe::StatusOr<Key> SampleKey(rlwe::Uint64 variance = kDefaultVariance,
                                rlwe::Uint64 log_t = kLogPlaintextModulus) {
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return Key::Sample(kLogCoeffs, variance, log_t, params14_.get(),
                       ntt_params_.get(), prng.get());
  }

  // Sample a random plaintext.
  std::vector<uint_m::Int> SamplePlaintext(uint_m::Int t = kPlaintextModulus,
                                           rlwe::Uint64 coeffs = kCoeffs) {
    std::vector<uint_m::Int> plaintext(kCoeffs);
    for (unsigned int i = 0; i < kCoeffs; i++) {
      plaintext[i] = rand_r(&seed) % t;
    }
    return plaintext;
  }

  // Encrypt a plaintext.
  rlwe::StatusOr<Ciphertext> Encrypt(
      const Key& key, const std::vector<uint_m::Int>& plaintext,
      const uint_m::Params* params,
      const rlwe::NttParameters<uint_m>* ntt_params,
      const ErrorParams* error_params) {
    RLWE_ASSIGN_OR_RETURN(auto m_plaintext,
                          ConvertToMontgomery(plaintext, params));
    auto plaintext_ntt =
        Polynomial::ConvertToNtt(m_plaintext, ntt_params, params);
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return rlwe::Encrypt<uint_m>(key, plaintext_ntt, error_params, prng.get());
  }

  std::unique_ptr<const uint_m::Params> params14_;
  std::unique_ptr<const uint_m::Params> params80_;
  std::unique_ptr<const rlwe::NttParameters<uint_m>> ntt_params_;
  std::unique_ptr<const rlwe::NttParameters<uint_m>> ntt_params80_;
  std::unique_ptr<const ErrorParams> error_params_;
  std::unique_ptr<const ErrorParams> error_params80_;
};

TEST_F(RelinearizationKeyTest, RelinearizationKeyReducesSizeOfCiphertext) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(
      auto relinearization_key,
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/3,
                                               kSmallLogDecompositionModulus));

  // Create the initial plaintexts.
  std::vector<uint_m::Int> plaintext1 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext1, params14_.get()));
  Polynomial plaintext1_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params_.get(), params14_.get());

  std::vector<uint_m::Int> plaintext2 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp2,
                       ConvertToMontgomery(plaintext2, params14_.get()));
  Polynomial plaintext2_ntt =
      Polynomial::ConvertToNtt(mp2, ntt_params_.get(), params14_.get());

  // Encrypt, multiply, apply the relinearization key and decrypt.
  ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                       Encrypt(key, plaintext1, params14_.get(),
                               ntt_params_.get(), error_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                       Encrypt(key, plaintext2, params14_.get(),
                               ntt_params_.get(), error_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto product, ciphertext1* ciphertext2);
  ASSERT_OK_AND_ASSIGN(auto relinearized_product,
                       relinearization_key.ApplyTo(product));

  EXPECT_EQ(product.Len(), 3);
  EXPECT_EQ(relinearized_product.Len(), 2);
}

TEST_F(RelinearizationKeyTest, RelinearizeKey3PartsDecrypts) {
  ASSERT_OK_AND_ASSIGN(std::string key_prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(auto key_prng,
                       rlwe::SingleThreadPrng::Create(key_prng_seed));
  ASSERT_OK_AND_ASSIGN(
      auto key,
      Key::Sample(kLogCoeffs, kDefaultVariance, kLogPlaintextModulus,
                  params80_.get(), ntt_params80_.get(), key_prng.get()));
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(
      auto relinearization_key,
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/3,
                                               kSmallLogDecompositionModulus));

  // Create the initial plaintexts.
  std::vector<uint_m::Int> plaintext1 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext1, params80_.get()));
  Polynomial plaintext1_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params80_.get(), params80_.get());

  std::vector<uint_m::Int> plaintext2 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp2,
                       ConvertToMontgomery(plaintext2, params80_.get()));
  Polynomial plaintext2_ntt =
      Polynomial::ConvertToNtt(mp2, ntt_params80_.get(), params80_.get());

  // Encrypt, multiply, apply the relinearization key and decrypt.
  ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                       Encrypt(key, plaintext1, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                       Encrypt(key, plaintext2, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto product, ciphertext1* ciphertext2);
  ASSERT_OK_AND_ASSIGN(auto relinearized_product,
                       relinearization_key.ApplyTo(product));
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, relinearized_product));

  // Create the polynomial we expect.
  ASSERT_OK(plaintext1_ntt.MulInPlace(plaintext2_ntt, params80_.get()));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      plaintext1_ntt.InverseNtt(ntt_params80_.get(), params80_.get()),
      params80_->modulus, kPlaintextModulus, params80_.get());

  EXPECT_EQ(decrypted, expected);
}

TEST_F(RelinearizationKeyTest, RelinearizeKey4PartsDecrypts) {
  ASSERT_OK_AND_ASSIGN(std::string key_prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(auto key_prng,
                       rlwe::SingleThreadPrng::Create(key_prng_seed));
  ASSERT_OK_AND_ASSIGN(
      auto key,
      Key::Sample(kLogCoeffs, kDefaultVariance, kLogPlaintextModulus,
                  params80_.get(), ntt_params80_.get(), key_prng.get()));
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(
      auto relinearization_key,
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/4,
                                               kLargeLogDecompositionModulus));

  // Create the initial plaintexts.
  std::vector<uint_m::Int> plaintext1 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext1, params80_.get()));
  Polynomial plaintext1_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params80_.get(), params80_.get());

  std::vector<uint_m::Int> plaintext2 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp2,
                       ConvertToMontgomery(plaintext2, params80_.get()));
  Polynomial plaintext2_ntt =
      Polynomial::ConvertToNtt(mp2, ntt_params80_.get(), params80_.get());

  std::vector<uint_m::Int> plaintext3 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp3,
                       ConvertToMontgomery(plaintext3, params80_.get()));
  Polynomial plaintext3_ntt =
      Polynomial::ConvertToNtt(mp3, ntt_params80_.get(), params80_.get());

  // Relinearize a 4 component ciphertext produced from three multiplications.
  ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                       Encrypt(key, plaintext1, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                       Encrypt(key, plaintext2, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext3,
                       Encrypt(key, plaintext3, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto intermediate, ciphertext1* ciphertext2);
  ASSERT_OK_AND_ASSIGN(auto product, intermediate* ciphertext3);
  ASSERT_OK_AND_ASSIGN(auto relinearized_product,
                       relinearization_key.ApplyTo(product));

  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, relinearized_product));

  // Create the polynomial we expect.
  ASSERT_OK(plaintext1_ntt.MulInPlace(plaintext2_ntt, params80_.get()));
  ASSERT_OK(plaintext1_ntt.MulInPlace(plaintext3_ntt, params80_.get()));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      plaintext1_ntt.InverseNtt(ntt_params80_.get(), params80_.get()),
      params80_->modulus, kPlaintextModulus, params80_.get());

  EXPECT_EQ(decrypted, expected);
}

TEST_F(RelinearizationKeyTest, RelinearizeKeyLargeModulusDecrypts) {
  ASSERT_OK_AND_ASSIGN(std::string key_prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(auto key_prng,
                       rlwe::SingleThreadPrng::Create(key_prng_seed));
  ASSERT_OK_AND_ASSIGN(
      auto key,
      Key::Sample(kLogCoeffs, kDefaultVariance, kLogPlaintextModulus,
                  params80_.get(), ntt_params80_.get(), key_prng.get()));
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(
      auto relinearization_key,
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/3,
                                               kLargeLogDecompositionModulus));

  // Create the initial plaintexts.
  std::vector<uint_m::Int> plaintext1 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext1, params80_.get()));
  Polynomial plaintext1_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params80_.get(), params80_.get());

  std::vector<uint_m::Int> plaintext2 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp2,
                       ConvertToMontgomery(plaintext2, params80_.get()));
  Polynomial plaintext2_ntt =
      Polynomial::ConvertToNtt(mp2, ntt_params80_.get(), params80_.get());

  // Multiply, apply the relinearization key, multiply, relinearize and decrypt.
  ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                       Encrypt(key, plaintext1, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                       Encrypt(key, plaintext2, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto product, ciphertext1* ciphertext2);
  ASSERT_OK_AND_ASSIGN(auto relinearized_product,
                       relinearization_key.ApplyTo(product));

  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, relinearized_product));

  // Create the polynomial we expect.
  ASSERT_OK(plaintext1_ntt.MulInPlace(plaintext2_ntt, params80_.get()));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      plaintext1_ntt.InverseNtt(ntt_params80_.get(), params80_.get()),
      params80_->modulus, kPlaintextModulus, params80_.get());

  EXPECT_EQ(decrypted, expected);
}

TEST_F(RelinearizationKeyTest, RepeatedRelinearizationDecrypts) {
  ASSERT_OK_AND_ASSIGN(std::string key_prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(auto key_prng,
                       rlwe::SingleThreadPrng::Create(key_prng_seed));
  ASSERT_OK_AND_ASSIGN(
      auto key,
      Key::Sample(kLogCoeffs, kDefaultVariance, kLogPlaintextModulus,
                  params80_.get(), ntt_params80_.get(), key_prng.get()));
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(
      auto relinearization_key,
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/3,
                                               kLargeLogDecompositionModulus));

  // Create the initial plaintexts.
  std::vector<uint_m::Int> plaintext1 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext1, params80_.get()));
  Polynomial plaintext1_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params80_.get(), params80_.get());

  std::vector<uint_m::Int> plaintext2 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp2,
                       ConvertToMontgomery(plaintext2, params80_.get()));
  Polynomial plaintext2_ntt =
      Polynomial::ConvertToNtt(mp2, ntt_params80_.get(), params80_.get());

  std::vector<uint_m::Int> plaintext3 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp3,
                       ConvertToMontgomery(plaintext3, params80_.get()));
  Polynomial plaintext3_ntt =
      Polynomial::ConvertToNtt(mp3, ntt_params80_.get(), params80_.get());

  // Multiply, apply the relinearization key, multiply, relinearize and decrypt.
  ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                       Encrypt(key, plaintext1, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                       Encrypt(key, plaintext2, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext3,
                       Encrypt(key, plaintext3, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto product1, ciphertext1* ciphertext2);
  ASSERT_OK_AND_ASSIGN(auto relinearized_product1,
                       relinearization_key.ApplyTo(product1));
  ASSERT_OK_AND_ASSIGN(auto product2, relinearized_product1* ciphertext3);
  ASSERT_OK_AND_ASSIGN(auto relinearized_product2,
                       relinearization_key.ApplyTo(product2));

  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, relinearized_product2));

  // Create the polynomial we expect.
  ASSERT_OK(plaintext1_ntt.MulInPlace(plaintext2_ntt, params80_.get()));
  ASSERT_OK(plaintext1_ntt.MulInPlace(plaintext3_ntt, params80_.get()));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      plaintext1_ntt.InverseNtt(ntt_params80_.get(), params80_.get()),
      params80_->modulus, kPlaintextModulus, params80_.get());

  EXPECT_EQ(decrypted, expected);
}

TEST_F(RelinearizationKeyTest, CiphertextWithTooManyComponents) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  // RelinearizationKey has length 2.
  ASSERT_OK_AND_ASSIGN(
      auto relinearization_key,
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/2,
                                               kSmallLogDecompositionModulus));

  // Create the initial plaintexts.
  std::vector<uint_m::Int> plaintext1 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext1, params14_.get()));
  Polynomial plaintext1_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params_.get(), params14_.get());

  std::vector<uint_m::Int> plaintext2 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp2,
                       ConvertToMontgomery(plaintext2, params14_.get()));
  Polynomial plaintext2_ntt =
      Polynomial::ConvertToNtt(mp2, ntt_params_.get(), params14_.get());

  // Encrypt, multiply, apply the relinearization key.
  ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                       Encrypt(key, plaintext1, params14_.get(),
                               ntt_params_.get(), error_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                       Encrypt(key, plaintext2, params14_.get(),
                               ntt_params_.get(), error_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto product, ciphertext1* ciphertext2);
  EXPECT_THAT(relinearization_key.ApplyTo(product),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("RelinearizationKey not large enough")));
}

TEST_F(RelinearizationKeyTest, LogDecompositionModulusOutOfBounds) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  // RelinearizationKey has length 2.
  EXPECT_THAT(
      rlwe::RelinearizationKey<uint_m>::Create(
          key, prng_seed, /*num_parts=*/2,
          /*log_decomposition_modulus=*/key.ModulusParams()->log_modulus + 1),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(absl::StrCat(
                   "Log decomposition modulus, ",
                   key.ModulusParams()->log_modulus + 1, ", ",
                   "must be at most: ", key.ModulusParams()->log_modulus))));

  int log_decomposition_modulus = 0;
  EXPECT_THAT(rlwe::RelinearizationKey<uint_m>::Create(
                  key, prng_seed, /*num_parts=*/3, log_decomposition_modulus),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(absl::StrCat("Log decomposition modulus, ",
                                              log_decomposition_modulus,
                                              ", must be positive."))));
}

TEST_F(RelinearizationKeyTest, NumPartsMustBePositive) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  EXPECT_THAT(
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/-1,
                                               kSmallLogDecompositionModulus),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Num parts: -1 must be positive.")));
}

TEST_F(RelinearizationKeyTest, InvalidDeserialize) {
  ASSERT_OK_AND_ASSIGN(std::string key_prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(auto key_prng,
                       rlwe::SingleThreadPrng::Create(key_prng_seed));
  ASSERT_OK_AND_ASSIGN(
      auto key,
      Key::Sample(kLogCoeffs, kDefaultVariance, kLogPlaintextModulus,
                  params80_.get(), ntt_params80_.get(), key_prng.get()));
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(
      auto relinearization_key,
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/3,
                                               kLargeLogDecompositionModulus));
  // Serialize and deserialize.
  ASSERT_OK_AND_ASSIGN(rlwe::SerializedRelinearizationKey serialized,
                       relinearization_key.Serialize());
  for (int i = -1; i <= 1; i++) {
    serialized.set_num_parts(i);
    EXPECT_THAT(RelinearizationKey::Deserialize(serialized, params80_.get(),
                                                ntt_params80_.get()),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(absl::StrCat(
                             "The number of parts, ", serialized.num_parts(),
                             ", must be greater than one."))));
  }
  ASSERT_GT(serialized.c_size(), 2);
  serialized.set_num_parts(serialized.c_size() - 1);
  EXPECT_THAT(
      RelinearizationKey::Deserialize(serialized, params80_.get(),
                                      ntt_params80_.get()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(absl::StrCat(
                   "The length of serialized, ", serialized.c_size(), ", ",
                   "must be divisible by the number of parts minus one ",
                   serialized.num_parts() - 1, "."))));
  ASSERT_EQ(serialized.c_size(),
            /* log2(kModulus80) / kLargeLogDecompositionModulus = */ 8);
  serialized.set_num_parts(serialized.c_size() + 1);
  EXPECT_THAT(RelinearizationKey::Deserialize(serialized, params80_.get(),
                                              ntt_params80_.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(absl::StrCat(
                           "Number of NTT Polynomials does not match expected ",
                           "number of matrix entries."))));
}

TEST_F(RelinearizationKeyTest, SerializeKey) {
  ASSERT_OK_AND_ASSIGN(std::string key_prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(auto key_prng,
                       rlwe::SingleThreadPrng::Create(key_prng_seed));
  ASSERT_OK_AND_ASSIGN(
      auto key,
      Key::Sample(kLogCoeffs, kDefaultVariance, kLogPlaintextModulus,
                  params80_.get(), ntt_params80_.get(), key_prng.get()));
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(
      auto relinearization_key,
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/3,
                                               kLargeLogDecompositionModulus));

  // Serialize and deserialize.
  ASSERT_OK_AND_ASSIGN(rlwe::SerializedRelinearizationKey serialized,
                       relinearization_key.Serialize());
  ASSERT_OK_AND_ASSIGN(auto deserialized,
                       RelinearizationKey::Deserialize(
                           serialized, params80_.get(), ntt_params80_.get()));

  // Create the initial plaintexts.
  std::vector<uint_m::Int> plaintext1 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext1, params80_.get()));
  Polynomial plaintext1_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params80_.get(), params80_.get());

  std::vector<uint_m::Int> plaintext2 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp2,
                       ConvertToMontgomery(plaintext2, params80_.get()));
  Polynomial plaintext2_ntt =
      Polynomial::ConvertToNtt(mp2, ntt_params80_.get(), params80_.get());

  // Encrypt, multiply, apply the relinearization key and decrypt.
  ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                       Encrypt(key, plaintext1, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                       Encrypt(key, plaintext2, params80_.get(),
                               ntt_params80_.get(), error_params80_.get()));
  ASSERT_OK_AND_ASSIGN(auto product, ciphertext1* ciphertext2);
  ASSERT_OK_AND_ASSIGN(auto relinearized_product,
                       deserialized.ApplyTo(product));
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, relinearized_product));

  // Create the polynomial we expect.
  ASSERT_OK(plaintext1_ntt.MulInPlace(plaintext2_ntt, params80_.get()));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      plaintext1_ntt.InverseNtt(ntt_params80_.get(), params80_.get()),
      params80_->modulus, kPlaintextModulus, params80_.get());

  EXPECT_EQ(decrypted, expected);
}

TEST_F(RelinearizationKeyTest, RelinearizationKeyIncreasesError) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());

  ASSERT_OK_AND_ASSIGN(
      auto relinearization_key,
      rlwe::RelinearizationKey<uint_m>::Create(key, prng_seed, /*num_parts=*/3,
                                               kSmallLogDecompositionModulus));

  // Create the initial plaintexts.
  std::vector<uint_m::Int> plaintext1 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp1,
                       ConvertToMontgomery(plaintext1, params14_.get()));
  Polynomial plaintext1_ntt =
      Polynomial::ConvertToNtt(mp1, ntt_params_.get(), params14_.get());

  std::vector<uint_m::Int> plaintext2 = SamplePlaintext(kPlaintextModulus);
  ASSERT_OK_AND_ASSIGN(auto mp2,
                       ConvertToMontgomery(plaintext2, params14_.get()));
  Polynomial plaintext2_ntt =
      Polynomial::ConvertToNtt(mp2, ntt_params_.get(), params14_.get());

  // Encrypt, multiply, apply the relinearization key and decrypt.
  ASSERT_OK_AND_ASSIGN(auto ciphertext1,
                       Encrypt(key, plaintext1, params14_.get(),
                               ntt_params_.get(), error_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                       Encrypt(key, plaintext2, params14_.get(),
                               ntt_params_.get(), error_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto product, ciphertext1* ciphertext2);
  ASSERT_OK_AND_ASSIGN(auto relinearized_product,
                       relinearization_key.ApplyTo(product));

  // Expect that the error grows after relinearization.
  EXPECT_GT(relinearized_product.Error(), product.Error());
}

}  // namespace
