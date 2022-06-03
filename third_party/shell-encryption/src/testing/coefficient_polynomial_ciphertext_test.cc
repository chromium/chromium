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

#include "testing/coefficient_polynomial_ciphertext.h"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "constants.h"
#include "montgomery.h"
#include "ntt_parameters.h"
#include "polynomial.h"
#include "symmetric_encryption.h"
#include "testing/coefficient_polynomial.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"
#include "testing/testing_utils.h"

namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

unsigned int seed = 0;

// Useful typedefs.
using uint_m = rlwe::MontgomeryInt<rlwe::Uint32>;
using Polynomial = rlwe::Polynomial<uint_m>;
using CoefficientPolynomial = rlwe::testing::CoefficientPolynomial<uint_m>;
using Ciphertext = rlwe::SymmetricRlweCiphertext<uint_m>;
using PolynomialCiphertext =
    rlwe::testing::CoefficientPolynomialCiphertext<uint_m>;
using Key = rlwe::SymmetricRlweKey<uint_m>;

// Set constants.
const uint_m::Int kDefaultLogT = 2;
const uint_m::Int kDefaultT = (1 << kDefaultLogT) + 1;
const uint_m::Int kDefaultVariance = 8;
const uint_m::Int kCoeffs = rlwe::kNewhopeDegreeBound;
const uint_m::Int kLogCoeffs = rlwe::kNewhopeLogDegreeBound;

class PolynomialCiphertextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(params14_,
                         rlwe::testing::ConstructMontgomeryIntParams());
    ASSERT_OK_AND_ASSIGN(auto ntt_params,
                         rlwe::InitializeNttParameters<uint_m>(
                             rlwe::testing::kLogCoeffs, params14_.get()));
    ntt_params_ = absl::make_unique<const rlwe::NttParameters<uint_m>>(
        std::move(ntt_params));
    ASSERT_OK_AND_ASSIGN(
        auto error_params,
        rlwe::ErrorParams<uint_m>::Create(kDefaultLogT, kDefaultVariance,
                                          params14_.get(), ntt_params_.get()));
    error_params_ =
        absl::make_unique<const rlwe::ErrorParams<uint_m>>(error_params);
  }

  // Sample a random key.
  rlwe::StatusOr<Key> SampleKey(uint_m::Int variance = kDefaultVariance,
                                uint_m::Int log_t = kDefaultLogT) {
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return Key::Sample(kLogCoeffs, variance, log_t, params14_.get(),
                       ntt_params_.get(), prng.get());
  }

  // Encrypt a plaintext.
  rlwe::StatusOr<Ciphertext> Encrypt(
      const Key& key, const std::vector<uint_m::Int>& plaintext) {
    RLWE_ASSIGN_OR_RETURN(auto mp, rlwe::testing::ConvertToMontgomery<uint_m>(
                                       plaintext, params14_.get()));
    auto plaintext_ntt =
        Polynomial::ConvertToNtt(mp, ntt_params_.get(), key.ModulusParams());
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return rlwe::Encrypt<uint_m>(key, plaintext_ntt, error_params_.get(),
                                 prng.get());
  }

  std::unique_ptr<const uint_m::Params> params14_;
  std::unique_ptr<const rlwe::NttParameters<uint_m>> ntt_params_;
  std::unique_ptr<const rlwe::ErrorParams<uint_m>> error_params_;
};

TEST_F(PolynomialCiphertextTest, CanDecryptAfterConversion) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(auto coefficient_ciphertext,
                       PolynomialCiphertext::ConvertToCoefficients(
                           ciphertext, ntt_params_.get()));
  auto ntt_ciphertext = coefficient_ciphertext.ConvertToNtt(ntt_params_.get());
  ASSERT_OK_AND_ASSIGN(auto coefficient_decrypted,
                       rlwe::Decrypt<uint_m>(key, ntt_ciphertext));

  EXPECT_EQ(plaintext, coefficient_decrypted);
}

TEST_F(PolynomialCiphertextTest, CoefficientHomomorphicAdd) {
  // Ensure PolynomialCiphertext adds.
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  std::vector<uint_m::Int> plaintext1 =
      rlwe::testing::SamplePlaintext<uint_m>();
  std::vector<uint_m::Int> plaintext2 =
      rlwe::testing::SamplePlaintext<uint_m>();

  ASSERT_OK_AND_ASSIGN(auto ciphertext1, Encrypt(key, plaintext1));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2, Encrypt(key, plaintext2));

  // Homomorphic add in the polynomial domain.
  ASSERT_OK_AND_ASSIGN(auto polynomial1,
                       PolynomialCiphertext::ConvertToCoefficients(
                           ciphertext1, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial2,
                       PolynomialCiphertext::ConvertToCoefficients(
                           ciphertext2, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial3, polynomial1 + polynomial2);

  // Decrypt result.
  auto ciphertext3 = polynomial3.ConvertToNtt(ntt_params_.get());
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, ciphertext3));

  for (unsigned int j = 0; j < plaintext1.size(); j++) {
    EXPECT_EQ((plaintext1[j] + plaintext2[j]) % kDefaultT, decrypted[j]);
  }
}

TEST_F(PolynomialCiphertextTest, HomomorphicAddDifferentComponents) {
  // Ensure PolynomialCiphertext adds.
  uint_m::Int log_t = 1;
  uint_m::Int variance = 4;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey(variance, log_t));

  std::vector<uint_m::Int> plaintext1 =
      rlwe::testing::SamplePlaintext<uint_m>(key.Len(), (1 << log_t) + 1);
  ASSERT_OK_AND_ASSIGN(auto mp1, rlwe::testing::ConvertToMontgomery<uint_m>(
                                     plaintext1, params14_.get()));
  CoefficientPolynomial plaintext1_poly(mp1, params14_.get());
  std::vector<uint_m::Int> plaintext2 =
      rlwe::testing::SamplePlaintext<uint_m>(key.Len(), (1 << log_t) + 1);
  ASSERT_OK_AND_ASSIGN(auto mp2, rlwe::testing::ConvertToMontgomery<uint_m>(
                                     plaintext2, params14_.get()));
  CoefficientPolynomial plaintext2_poly(mp2, params14_.get());

  // Create ciphertexts of different lengths.
  ASSERT_OK_AND_ASSIGN(auto two_component_ciphertext, Encrypt(key, plaintext1));
  ASSERT_OK_AND_ASSIGN(auto intermediate, Encrypt(key, plaintext2));
  ASSERT_OK_AND_ASSIGN(auto three_component_ciphertext,
                       intermediate* two_component_ciphertext);

  // Homomorphic add in the polynomial domain.
  ASSERT_OK_AND_ASSIGN(auto polynomial1,
                       PolynomialCiphertext::ConvertToCoefficients(
                           two_component_ciphertext, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial2,
                       PolynomialCiphertext::ConvertToCoefficients(
                           three_component_ciphertext, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial3, polynomial1 + polynomial2);

  // Decrypt result.
  auto ciphertext3 = polynomial3.ConvertToNtt(ntt_params_.get());
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, ciphertext3));

  // Create expected result.
  ASSERT_OK_AND_ASSIGN(auto product, plaintext2_poly* plaintext1_poly);
  ASSERT_OK_AND_ASSIGN(CoefficientPolynomial expected_poly,
                       plaintext1_poly + product);
  std::vector<uint_m::Int> expected =
      rlwe::RemoveError<uint_m>(expected_poly.Coeffs(), params14_->modulus,
                                (1 << log_t) + 1, params14_.get());

  EXPECT_EQ(expected, decrypted);
}

TEST_F(PolynomialCiphertextTest, MonomialOutOfRange) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  std::vector<uint_m::Int> plaintext = rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto encrypt, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(auto coeffs, PolynomialCiphertext::ConvertToCoefficients(
                                        encrypt, ntt_params_.get()));
  EXPECT_THAT(coeffs.MonomialAbsorb(2 * key.Len()),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("Monomial to absorb must have non-negative "
                                 "degree less than 2n.")));
}

TEST_F(PolynomialCiphertextTest, CoefficientMonomialAbsorb) {
  // Ensure that an absorb for monomials works correctly.
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  // Create a random plaintext and a random monomial.
  std::vector<uint_m::Int> plaintext = rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto mp, rlwe::testing::ConvertToMontgomery<uint_m>(
                                    plaintext, params14_.get()));
  CoefficientPolynomial plaintext_polynomial(mp, params14_.get());
  std::vector<uint_m::Int> monomial(kCoeffs);
  int monomial_index = rand_r(&seed) % (2 * key.Len());
  monomial[monomial_index] = 1;

  // Create our expected value.
  ASSERT_OK_AND_ASSIGN(
      CoefficientPolynomial expected_polynomial,
      plaintext_polynomial.MonomialMultiplication(monomial_index));
  std::vector<uint_m::Int> expected =
      rlwe::RemoveError<uint_m>(expected_polynomial.Coeffs(),
                                params14_->modulus, kDefaultT, params14_.get());

  // Encrypt and absorb in the polynomial domain, then decrypt.
  ASSERT_OK_AND_ASSIGN(auto encrypt, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(auto coeffs, PolynomialCiphertext::ConvertToCoefficients(
                                        encrypt, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial, coeffs.MonomialAbsorb(monomial_index));
  auto ciphertext = polynomial.ConvertToNtt(ntt_params_.get());
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, ciphertext));

  EXPECT_EQ(expected, decrypted);
}

TEST_F(PolynomialCiphertextTest, Substitution) {
  int subtitution_power = 3;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();

  // Create the expected polynomial output by substituting the plaintext.
  ASSERT_OK_AND_ASSIGN(auto mp, rlwe::testing::ConvertToMontgomery<uint_m>(
                                    plaintext, params14_.get()));
  CoefficientPolynomial polynomial_coeffs(mp, params14_.get());
  ASSERT_OK_AND_ASSIGN(CoefficientPolynomial polynomial_expected,
                       polynomial_coeffs.Substitute(subtitution_power));
  std::vector<uint_m::Int> expected =
      rlwe::RemoveError<uint_m>(polynomial_expected.Coeffs(),
                                params14_->modulus, kDefaultT, params14_.get());

  // Encrypt and substitute the ciphertext. Decrypt with a substituted key.
  ASSERT_OK_AND_ASSIGN(auto encrypt, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(auto coeffs, PolynomialCiphertext::ConvertToCoefficients(
                                        encrypt, ntt_params_.get()));

  ASSERT_OK_AND_ASSIGN(auto polynomial, coeffs.Substitute(subtitution_power));
  auto ciphertext = polynomial.ConvertToNtt(ntt_params_.get());
  ASSERT_OK_AND_ASSIGN(auto sub_key, key.Substitute(subtitution_power));
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(sub_key, ciphertext));

  EXPECT_EQ(expected, decrypted);
  EXPECT_EQ(ciphertext.PowerOfS(), subtitution_power);
}

TEST_F(PolynomialCiphertextTest, SubstitutionFailsOnEvenPower) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();

  ASSERT_OK_AND_ASSIGN(auto encrypt, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(auto coeffs, PolynomialCiphertext::ConvertToCoefficients(
                                        encrypt, ntt_params_.get()));
  EXPECT_THAT(coeffs.Substitute(2),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("power must be a non-negative odd integer")));
}

TEST_F(PolynomialCiphertextTest, PowersOfSMustMatchOnAdd) {
  int subtitution_power = 5;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  std::vector<uint_m::Int> plaintext1 =
      rlwe::testing::SamplePlaintext<uint_m>();
  std::vector<uint_m::Int> plaintext2 =
      rlwe::testing::SamplePlaintext<uint_m>();

  ASSERT_OK_AND_ASSIGN(auto ciphertext1, Encrypt(key, plaintext1));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2, Encrypt(key, plaintext2));

  // Add fails when only one ciphertext is substituted.
  ASSERT_OK_AND_ASSIGN(auto polynomial1,
                       PolynomialCiphertext::ConvertToCoefficients(
                           ciphertext1, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial2,
                       PolynomialCiphertext::ConvertToCoefficients(
                           ciphertext2, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial2_sub,
                       polynomial2.Substitute(subtitution_power));

  EXPECT_THAT(polynomial1 + polynomial2_sub,
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be encrypted with the same key")));
}

TEST_F(PolynomialCiphertextTest, AddOnSubstitutedCiphertexts) {
  int subtitution_power = 5;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  std::vector<uint_m::Int> plaintext1 =
      rlwe::testing::SamplePlaintext<uint_m>();
  std::vector<uint_m::Int> plaintext2 =
      rlwe::testing::SamplePlaintext<uint_m>();

  // Create the expected polynomial output by substituting the plaintext.
  ASSERT_OK_AND_ASSIGN(auto mp1, rlwe::testing::ConvertToMontgomery<uint_m>(
                                     plaintext1, params14_.get()));
  CoefficientPolynomial polynomial1_coeffs(mp1, params14_.get());
  ASSERT_OK_AND_ASSIGN(auto mp2, rlwe::testing::ConvertToMontgomery<uint_m>(
                                     plaintext2, params14_.get()));
  CoefficientPolynomial polynomial2_coeffs(mp2, params14_.get());
  ASSERT_OK_AND_ASSIGN(auto sum, polynomial1_coeffs + polynomial2_coeffs);
  ASSERT_OK_AND_ASSIGN(CoefficientPolynomial polynomial_expected,
                       sum.Substitute(subtitution_power));
  std::vector<uint_m::Int> expected =
      rlwe::RemoveError<uint_m>(polynomial_expected.Coeffs(),
                                params14_->modulus, kDefaultT, params14_.get());

  ASSERT_OK_AND_ASSIGN(auto ciphertext1, Encrypt(key, plaintext1));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2, Encrypt(key, plaintext2));

  // Add succeeds and decrypts when both ciphertexts are substituted.
  ASSERT_OK_AND_ASSIGN(auto coeffs1,
                       PolynomialCiphertext::ConvertToCoefficients(
                           ciphertext1, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial1_sub,
                       coeffs1.Substitute(subtitution_power));
  ASSERT_OK_AND_ASSIGN(auto coeffs2,
                       PolynomialCiphertext::ConvertToCoefficients(
                           ciphertext2, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial2_sub,
                       coeffs2.Substitute(subtitution_power));
  ASSERT_OK_AND_ASSIGN(PolynomialCiphertext result,
                       polynomial1_sub + polynomial2_sub);

  // Decrypt result.
  auto ciphertext_result = result.ConvertToNtt(ntt_params_.get());
  ASSERT_OK_AND_ASSIGN(auto key_sub, key.Substitute(subtitution_power));
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key_sub, ciphertext_result));

  EXPECT_EQ(expected, decrypted);
  EXPECT_EQ(result.PowerOfS(), subtitution_power);
}

TEST_F(PolynomialCiphertextTest, AbsorbOnSubstitutedCiphertexts) {
  int monomial = 3;
  int subtitution_power = 5;

  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  // Create a polynomial p(x).
  std::vector<uint_m::Int> plaintext = rlwe::testing::SamplePlaintext<uint_m>();

  // Create the expected polynomial output by absorbing in the p(x^sub).
  ASSERT_OK_AND_ASSIGN(auto mp, rlwe::testing::ConvertToMontgomery<uint_m>(
                                    plaintext, params14_.get()));
  CoefficientPolynomial polynomial_coeffs(mp, params14_.get());
  ASSERT_OK_AND_ASSIGN(auto coeffs,
                       polynomial_coeffs.Substitute(subtitution_power));
  ASSERT_OK_AND_ASSIGN(CoefficientPolynomial coeff_expected,
                       coeffs.MonomialMultiplication(monomial));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      coeff_expected.Coeffs(), params14_->modulus, kDefaultT, params14_.get());

  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));

  // Absorb x^monomial in the substituted ciphertext.
  ASSERT_OK_AND_ASSIGN(auto coeffs2,
                       PolynomialCiphertext::ConvertToCoefficients(
                           ciphertext, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto sub_coeffs, coeffs2.Substitute(subtitution_power));
  ASSERT_OK_AND_ASSIGN(auto coeff_result, sub_coeffs.MonomialAbsorb(monomial));

  // Decrypt result.
  auto ntt_result = coeff_result.ConvertToNtt(ntt_params_.get());
  ASSERT_OK_AND_ASSIGN(auto key_sub, key.Substitute(subtitution_power));
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key_sub, ntt_result));

  // Expect that the ciphertext x^monomial c(x^sub) decrypts to x^monomial
  // p(x^sub).
  EXPECT_EQ(expected, decrypted);
  EXPECT_EQ(coeff_result.PowerOfS(), subtitution_power);
}

TEST_F(PolynomialCiphertextTest, RepeatedSubstitution) {
  // Verifies PowerOfS is updated correctly after repeated substitutions, and
  // the ciphertext can still be decrypted correctly.
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto encrypt, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(
      auto polynomial,
      PolynomialCiphertext::ConvertToCoefficients(encrypt, ntt_params_.get()));

  EXPECT_EQ(polynomial.PowerOfS(), 1);

  // Creates an encryption of p(x^3).
  ASSERT_OK_AND_ASSIGN(auto polynomial3, polynomial.Substitute(3));
  EXPECT_EQ(polynomial3.PowerOfS(), 3);

  ASSERT_OK_AND_ASSIGN(auto polynomial9, polynomial3.Substitute(3));
  EXPECT_EQ(polynomial9.PowerOfS(), 9 % kCoeffs);

  // Substitutes the inverse of 3 mod 1024 to retrieve an encryption of the
  // original polynomial: p((x^3)^683) =p(x).
  ASSERT_OK_AND_ASSIGN(auto polynomial_wraparound, polynomial3.Substitute(683));
  auto ciphertext_wraparound =
      polynomial_wraparound.ConvertToNtt(ntt_params_.get());
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, ciphertext_wraparound));

  // Verifies that a polynomial after repeated substitutions decrypts correctly.
  EXPECT_EQ(polynomial_wraparound.PowerOfS(), 1);
  EXPECT_EQ(decrypted, plaintext);
}

TEST_F(PolynomialCiphertextTest, NttConversionsPreservePowerOfS) {
  // Ensures that NTT conversion to / from polynomial ciphertexts preserves the
  // power of s index.
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));

  EXPECT_EQ(ciphertext.PowerOfS(), 1);

  ASSERT_OK_AND_ASSIGN(auto coeffs, PolynomialCiphertext::ConvertToCoefficients(
                                        ciphertext, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial, coeffs.Substitute(3));

  EXPECT_EQ(polynomial.PowerOfS(), 3);

  ASSERT_OK_AND_ASSIGN(
      auto ntt_converted_polynomial,
      PolynomialCiphertext::ConvertToCoefficients(
          polynomial.ConvertToNtt(ntt_params_.get()), ntt_params_.get()));

  EXPECT_EQ(ntt_converted_polynomial.PowerOfS(), 3);
}

TEST_F(PolynomialCiphertextTest, SubstitutionCommutesWithAbsorb) {
  // Checks if a monomial absorb can be factored out of a Substitution. In other
  // words, this checks that for a ciphertext c and a plaintext p, we can either
  // Absorb a plaintext and Substitute, or Substitute and Absorb the substituted
  // plaintext: (c.Absorb(p)).Substitute(power) ==
  // (c.Substitute(power)).Absorb(p.Substitute(power)).
  int monomial = kCoeffs - 1;
  int substitution_power = kCoeffs + 1;
  // For x^monomial, (x^monomial).Substitute(power) = x^{monomial_substituted}.
  int monomial_substituted = (monomial * substitution_power) % (2 * kCoeffs);

  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  // Create a polynomial p(x).
  std::vector<uint_m::Int> plaintext = rlwe::testing::SamplePlaintext<uint_m>();

  // Create the expected polynomial by first applying the absorb and then
  // substitution.
  ASSERT_OK_AND_ASSIGN(auto mp, rlwe::testing::ConvertToMontgomery<uint_m>(
                                    plaintext, params14_.get()));
  CoefficientPolynomial polynomial_coeffs(mp, params14_.get());
  ASSERT_OK_AND_ASSIGN(auto prod,
                       polynomial_coeffs.MonomialMultiplication(monomial));
  ASSERT_OK_AND_ASSIGN(CoefficientPolynomial coeff_expected,
                       prod.Substitute(substitution_power));
  std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
      coeff_expected.Coeffs(), params14_->modulus, kDefaultT, params14_.get());

  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));

  // Takes the ciphertext and FIRST applies the substitution, and then follows
  // with an absorb of the corresponding power.
  ASSERT_OK_AND_ASSIGN(auto coeffs, PolynomialCiphertext::ConvertToCoefficients(
                                        ciphertext, ntt_params_.get()));
  ASSERT_OK_AND_ASSIGN(auto sub_coeffs, coeffs.Substitute(substitution_power));
  ASSERT_OK_AND_ASSIGN(auto coeff_result,
                       sub_coeffs.MonomialAbsorb(monomial_substituted));

  // Decrypt result.
  auto ntt_result = coeff_result.ConvertToNtt(ntt_params_.get());
  ASSERT_OK_AND_ASSIGN(auto sub_key, key.Substitute(substitution_power));
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(sub_key, ntt_result));

  EXPECT_EQ(expected, decrypted);
  EXPECT_EQ(coeff_result.PowerOfS(), substitution_power);
}

}  // namespace
