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

#include "polynomial.h"

#include <cmath>
#include <random>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "constants.h"
#include "montgomery.h"
#include "ntt_parameters.h"
#include "prng/integral_prng_testing_types.h"
#include "serialization.pb.h"
#include "status_macros.h"
#include "testing/coefficient_polynomial.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"


namespace {

using ::rlwe::testing::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Ne;

// Useful typedefs.
using uint_m = rlwe::MontgomeryInt<rlwe::Uint16>;

using CoefficientPolynomial = rlwe::testing::CoefficientPolynomial<uint_m>;
using Polynomial = rlwe::Polynomial<uint_m>;

unsigned int seed = 0;
const absl::string_view kPrngSeed =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

// Test fixture to take care of messy setup.
template <typename Prng>
class PolynomialTest : public ::testing::Test {
 protected:
  PolynomialTest()
      : params14_(uint_m::Params::Create(rlwe::kNewhopeModulus).value()),
        zero_(uint_m::ImportZero(params14_.get())) {}

  void SetUp() override { srand(0); }

  void SetParams(int n, int log_n) {
    // Prng to generate random values
    ASSERT_OK_AND_ASSIGN(auto prng,
                         Prng::Create(kPrngSeed.substr(0, Prng::SeedLength())));

    std::vector<uint_m> p_coeffs(n, zero_);
    std::vector<uint_m> q_coeffs(n, zero_);

    // Create some random polynomials. Ensure that they are different.
    for (int j = 0; j < n; j++) {
      ASSERT_OK_AND_ASSIGN(p_coeffs[j],
                           uint_m::ImportRandom(prng.get(), params14_.get()));
      ASSERT_OK_AND_ASSIGN(q_coeffs[j],
                           uint_m::ImportRandom(prng.get(), params14_.get()));
    }

    // Ensure the polynomials are different.
    uint_m::Int rand_index = rand_r(&seed) % n;
    auto one = uint_m::ImportOne(params14_.get());
    p_coeffs[rand_index] = q_coeffs[rand_index].Add(one, params14_.get());

    p_.reset(new CoefficientPolynomial(p_coeffs, params14_.get()));
    q_.reset(new CoefficientPolynomial(q_coeffs, params14_.get()));

    // Acquire all of the NTT parameters.
    ASSERT_OK_AND_ASSIGN(auto ntt_params, rlwe::InitializeNttParameters<uint_m>(
                                              log_n, params14_.get()));
    ntt_params_ =
        absl::make_unique<rlwe::NttParameters<uint_m>>(std::move(ntt_params));

    // Put p and q in the NTT domain.
    ntt_p_ =
        Polynomial::ConvertToNtt(p_coeffs, ntt_params_.get(), params14_.get());
    ntt_q_ =
        Polynomial::ConvertToNtt(q_coeffs, ntt_params_.get(), params14_.get());
  }

  std::unique_ptr<Prng> MakePrng(absl::string_view seed) {
    auto prng = Prng::Create(seed.substr(0, Prng::SeedLength())).value();
    return prng;
  }

  std::unique_ptr<const uint_m::Params> params14_;
  std::unique_ptr<rlwe::NttParameters<uint_m>> ntt_params_;
  std::unique_ptr<CoefficientPolynomial> p_;
  std::unique_ptr<CoefficientPolynomial> q_;
  Polynomial ntt_p_;
  Polynomial ntt_q_;
  uint_m zero_;
};

TYPED_TEST_SUITE(PolynomialTest, rlwe::TestingPrngTypes);

// Ensure that a default NTT polynomial is invalid.
TYPED_TEST(PolynomialTest, DefaultIsInvalid) {
  EXPECT_FALSE(Polynomial().IsValid());
}

TYPED_TEST(PolynomialTest, CoeffsCorrectlyReturnsCoefficients) {
  auto prng = this->MakePrng(kPrngSeed);
  for (int i = 2; i < 11; i++) {
    int n = 1 << i;
    this->SetParams(n, i);

    // coeffs = {1, 0, 0, ...}, ntt_coeffs = {1, 1, 1, ...}
    // Test that NTT(coeffs) == ntt_coeffs
    std::vector<uint_m::Int> coeffs(n, 0), ntt_coeffs(n, 1);
    coeffs[0] = 1;

    std::vector<uint_m> v;
    for (const uint_m::Int& coeff : coeffs) {
      ASSERT_OK_AND_ASSIGN(auto elt,
                           uint_m::ImportInt(coeff, this->params14_.get()));
      v.push_back(elt);
    }

    Polynomial ntt_v = Polynomial::ConvertToNtt(v, this->ntt_params_.get(),
                                                this->params14_.get());

    for (int j = 0; j < n; j++) {
      EXPECT_EQ(ntt_v.Coeffs()[j].ExportInt(this->params14_.get()),
                ntt_coeffs[j]);
    }

    // Test that coeffs are the same when explicitly constructed
    std::vector<uint_m> coeffs2(n, this->zero_);
    for (int j = 0; j < n; j++) {
      ASSERT_OK_AND_ASSIGN(
          coeffs2[j], uint_m::ImportRandom(prng.get(), this->params14_.get()));
    }
    Polynomial ntt2 = Polynomial(coeffs2);
    std::vector<uint_m> nttcoeffs2 = ntt2.Coeffs();
    for (int j = 0; j < n; j++) {
      EXPECT_EQ(coeffs2[j], nttcoeffs2[j]);
    }

    // Test that coeffs are all zero when only length is given
    Polynomial ntt3 = Polynomial(n, this->params14_.get());
    std::vector<uint_m> nttcoeffs3 = ntt3.Coeffs();
    for (int j = 0; j < n; j++) {
      EXPECT_EQ(nttcoeffs3[j], this->zero_);
    }
  }
}

// Ensure that a polynomial converted to NTT form can be converted back.
TYPED_TEST(PolynomialTest, Symmetry) {
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);
    EXPECT_TRUE(this->ntt_p_.IsValid());
    CoefficientPolynomial p_prime(
        this->ntt_p_.InverseNtt(this->ntt_params_.get(), this->params14_.get()),
        this->params14_.get());
    EXPECT_EQ(*this->p_, p_prime);
  }
}

// Ensure that equality holds properly.
TYPED_TEST(PolynomialTest, Equality) {
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);
    Polynomial ntt_p_cpy = this->ntt_p_;
    Polynomial ntt_q_cpy = this->ntt_q_;

    EXPECT_TRUE(this->ntt_p_ == ntt_p_cpy);
    EXPECT_TRUE(this->ntt_q_ == ntt_q_cpy);
    EXPECT_FALSE(this->ntt_p_ != ntt_p_cpy);
    EXPECT_FALSE(this->ntt_q_ != ntt_q_cpy);

    EXPECT_TRUE(this->ntt_p_ != this->ntt_q_);
    EXPECT_TRUE(this->ntt_q_ != this->ntt_p_);
    EXPECT_FALSE(this->ntt_p_ == this->ntt_q_);
    EXPECT_FALSE(this->ntt_q_ == this->ntt_p_);
  }
}

// Ensure that a polynomial whose size is not a power of two gets rejected.
TYPED_TEST(PolynomialTest, NotPowerOfTwo) {
  for (int i = 2; i < 11; i++) {
    // j is any value that isn't a power of 2.
    for (int j = 1 + (1 << (i - 1)); j < (1 << i); j++) {
      this->SetParams(j, i);
      EXPECT_FALSE(this->ntt_p_.IsValid());
    }
  }
}

// Ensure that adding or multiplying two polynomials of different lengths gets
// rejected.
TYPED_TEST(PolynomialTest, BinopOfDifferentLengths) {
  for (int i = 2; i < 11; i++) {
    for (int j = 2; j < 11; j++) {
      if (j == i) {
        continue;
      }

      int bigger = std::max(i, j);
      this->SetParams(1 << bigger, bigger);

      std::vector<uint_m> x(1 << i, this->zero_);
      std::vector<uint_m> y(1 << j, this->zero_);

      this->ntt_params_->bitrevs = rlwe::internal::BitrevArray(i);
      Polynomial ntt_x = Polynomial::ConvertToNtt(x, this->ntt_params_.get(),
                                                  this->params14_.get());
      this->ntt_params_->bitrevs = rlwe::internal::BitrevArray(j);
      Polynomial ntt_y = Polynomial::ConvertToNtt(y, this->ntt_params_.get(),
                                                  this->params14_.get());

      EXPECT_TRUE(ntt_x.IsValid());
      EXPECT_TRUE(ntt_y.IsValid());

      // Lengths are different
      EXPECT_FALSE(ntt_x == ntt_y);

      EXPECT_THAT(ntt_x.Mul(ntt_y, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_y.Mul(ntt_x, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_x.Add(ntt_y, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_y.Add(ntt_x, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_x.Sub(ntt_y, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_y.Sub(ntt_x, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));

      // In-place operations return the original polynomial if invalid
      // To check, we keep a copy of the original and check that the output
      // remains the same
      Polynomial orig_ntt_x = ntt_x;
      Polynomial orig_ntt_y = ntt_y;
      EXPECT_THAT(ntt_x.MulInPlace(ntt_y, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_y.MulInPlace(ntt_x, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_x.AddInPlace(ntt_y, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_y.AddInPlace(ntt_x, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_x.SubInPlace(ntt_y, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
      EXPECT_THAT(ntt_y.SubInPlace(ntt_x, this->params14_.get()),
                  StatusIs(::absl::StatusCode::kInvalidArgument,
                           HasSubstr("do not have the same length")));
    }
  }
}

// Test that the convolution property holds. Let p, q be polynomials.
// CoefficientPolynomial multiplication of p and q =
// NTT_INV(the coordinate-wise product of NTT(p) and NTT(q))
TYPED_TEST(PolynomialTest, Multiply) {
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);

    EXPECT_TRUE(this->ntt_p_.IsValid());
    EXPECT_TRUE(this->ntt_q_.IsValid());

    ASSERT_OK_AND_ASSIGN(Polynomial ntt_res1,
                         this->ntt_p_.Mul(this->ntt_q_, this->params14_.get()));
    ASSERT_OK_AND_ASSIGN(Polynomial ntt_res2,
                         this->ntt_q_.Mul(this->ntt_p_, this->params14_.get()));

    CoefficientPolynomial res1(
        ntt_res1.InverseNtt(this->ntt_params_.get(), this->params14_.get()),
        this->params14_.get());
    CoefficientPolynomial res2(
        ntt_res2.InverseNtt(this->ntt_params_.get(), this->params14_.get()),
        this->params14_.get());

    ASSERT_OK_AND_ASSIGN(CoefficientPolynomial expected,
                         (*this->p_) * (*this->q_));

    EXPECT_EQ(res1, expected);
    EXPECT_EQ(res2, expected);
    EXPECT_EQ(res1, res2);
  }
}

// Test scalar multiplication.
TYPED_TEST(PolynomialTest, ScalarMultiply) {
  auto prng = this->MakePrng(kPrngSeed);
  ASSERT_OK_AND_ASSIGN(uint_m scalar,
                       uint_m::ImportRandom(prng.get(), this->params14_.get()));
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);

    EXPECT_TRUE(this->ntt_p_.IsValid());

    ASSERT_OK_AND_ASSIGN(Polynomial ntt_res,
                         this->ntt_p_.Mul(scalar, this->params14_.get()));

    CoefficientPolynomial res(
        ntt_res.InverseNtt(this->ntt_params_.get(), this->params14_.get()),
        this->params14_.get());

    CoefficientPolynomial expected = (*this->p_) * scalar;

    EXPECT_EQ(res, expected);
  }
}

// Test that p + (-p) = 0.
TYPED_TEST(PolynomialTest, Negate) {
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);

    // An NTT polynomial of all zeros.
    Polynomial zeros_ntt = Polynomial::ConvertToNtt(
        std::vector<uint_m>(1 << i, this->zero_), this->ntt_params_.get(),
        this->params14_.get());

    auto minus_p = this->ntt_p_.Negate(this->params14_.get());
    ASSERT_OK_AND_ASSIGN(auto p0,
                         this->ntt_p_.Add(minus_p, this->params14_.get()));
    EXPECT_EQ(zeros_ntt, p0);
    ASSERT_OK_AND_ASSIGN(p0, minus_p.Add(this->ntt_p_, this->params14_.get()));
    EXPECT_EQ(zeros_ntt, p0);
  }
}

// Test that p + q = NTT_INV(NTT(p) + NTT(q)).
TYPED_TEST(PolynomialTest, Add) {
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);
    EXPECT_TRUE(this->ntt_p_.IsValid());
    EXPECT_TRUE(this->ntt_q_.IsValid());

    ASSERT_OK_AND_ASSIGN(Polynomial ntt_res1,
                         this->ntt_p_.Add(this->ntt_q_, this->params14_.get()));
    ASSERT_OK_AND_ASSIGN(Polynomial ntt_res2,
                         this->ntt_q_.Add(this->ntt_p_, this->params14_.get()));

    CoefficientPolynomial res1(
        ntt_res1.InverseNtt(this->ntt_params_.get(), this->params14_.get()),
        this->params14_.get());
    CoefficientPolynomial res2(
        ntt_res2.InverseNtt(this->ntt_params_.get(), this->params14_.get()),
        this->params14_.get());

    ASSERT_OK_AND_ASSIGN(CoefficientPolynomial expected,
                         (*this->p_) + (*this->q_));
    EXPECT_EQ(res1, expected);
    EXPECT_EQ(res2, expected);
    EXPECT_EQ(res1, res2);
  }
}

// Test that p - q = NTT_INV(NTT(p) - NTT(q)) and
//           q - p = NTT_INV(NTT(q) - NTT(p)).
TYPED_TEST(PolynomialTest, Sub) {
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);
    EXPECT_TRUE(this->ntt_p_.IsValid());
    EXPECT_TRUE(this->ntt_q_.IsValid());

    ASSERT_OK_AND_ASSIGN(Polynomial ntt_res1,
                         this->ntt_p_.Sub(this->ntt_q_, this->params14_.get()));
    ASSERT_OK_AND_ASSIGN(Polynomial ntt_res2,
                         this->ntt_q_.Sub(this->ntt_p_, this->params14_.get()));

    CoefficientPolynomial res1(
        ntt_res1.InverseNtt(this->ntt_params_.get(), this->params14_.get()),
        this->params14_.get());
    CoefficientPolynomial res2(
        ntt_res2.InverseNtt(this->ntt_params_.get(), this->params14_.get()),
        this->params14_.get());

    ASSERT_OK_AND_ASSIGN(CoefficientPolynomial expected_res1,
                         (*this->p_) - (*this->q_));
    ASSERT_OK_AND_ASSIGN(CoefficientPolynomial expected_res2,
                         (*this->q_) - (*this->p_));
    EXPECT_EQ(res1, expected_res1);
    EXPECT_EQ(res2, expected_res2);
  }
}

TYPED_TEST(PolynomialTest, SubstitutionPowerMalformed) {
  // Ensure substitution fails on powers that are negative, even, and greater
  // than 2 * kDimension.
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);

    EXPECT_THAT(
        this->ntt_p_.Substitute(2, this->ntt_params_.get(),
                                this->params14_.get()),
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr("must be a non-negative odd integer less than")));

    // Even when not in debugging mode, the following two tests will yield a
    // segmentation fault. We therefore only do the tests in debug mode.
    EXPECT_THAT(
        this->ntt_p_.Substitute(-10, this->ntt_params_.get(),
                                this->params14_.get()),
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr("must be a non-negative odd integer less than")));
    EXPECT_THAT(
        this->ntt_p_.Substitute(2 * (1 << i) + 1, this->ntt_params_.get(),
                                this->params14_.get()),
        StatusIs(::absl::StatusCode::kInvalidArgument,
                 HasSubstr("must be a non-negative odd integer less than")));
  }
}

TYPED_TEST(PolynomialTest, Substitution) {
  // Tests substitutions of the form N/2^k + 1.
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);
    int dimension = 1 << i;
    for (int k = 0; k < i; k++) {
      int power = (dimension >> k) + 1;
      ASSERT_OK_AND_ASSIGN(
          auto ntt_res, this->ntt_p_.Substitute(power, this->ntt_params_.get(),
                                                this->params14_.get()));
      CoefficientPolynomial res(
          ntt_res.InverseNtt(this->ntt_params_.get(), this->params14_.get()),
          this->params14_.get());

      ASSERT_OK_AND_ASSIGN(auto r, this->p_->Substitute(power));
      EXPECT_EQ(res, r);
    }
  }
}

TYPED_TEST(PolynomialTest, Serialize) {
  for (int i = 2; i < 11; i++) {
    this->SetParams(1 << i, i);
    ASSERT_OK_AND_ASSIGN(rlwe::SerializedNttPolynomial serialized_p,
                         this->ntt_p_.Serialize(this->params14_.get()));
    ASSERT_OK_AND_ASSIGN(rlwe::SerializedNttPolynomial serialized_q,
                         this->ntt_q_.Serialize(this->params14_.get()));

    // Ensure that a serialized polynomial can be deserialized.
    ASSERT_OK_AND_ASSIGN(
        auto deserialized_p,
        Polynomial::Deserialize(serialized_p, this->params14_.get()));
    EXPECT_EQ(this->ntt_p_, deserialized_p);
    ASSERT_OK_AND_ASSIGN(
        auto deserialized_q,
        Polynomial::Deserialize(serialized_q, this->params14_.get()));
    EXPECT_EQ(this->ntt_q_, deserialized_q);

    // Ensure that the length of a Serialized polynomial is at most
    // SerializedSize times the number of coefficients.
    EXPECT_LE(serialized_p.coeffs().size(),
              this->ntt_p_.Len() * this->params14_->SerializedSize());
    EXPECT_LE(serialized_q.coeffs().size(),
              this->ntt_q_.Len() * this->params14_->SerializedSize());
  }
}

TYPED_TEST(PolynomialTest,
           SamplePolynomialFromPrngReturnsPolynomialWithCorrectSize) {
  auto prng = this->MakePrng(kPrngSeed);
  ASSERT_OK_AND_ASSIGN(auto polynomial,
                       rlwe::SamplePolynomialFromPrng<uint_m>(
                           16, prng.get(), this->params14_.get()));
  EXPECT_THAT(polynomial.Len(), Eq(16));
}

TYPED_TEST(PolynomialTest, SamplePolynomialFromPrngFailsWithLengthLessThanOne) {
  auto prng = this->MakePrng(kPrngSeed);
  EXPECT_THAT(rlwe::SamplePolynomialFromPrng<uint_m>(0, prng.get(),
                                                     this->params14_.get()),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be a non-negative integer")));
  EXPECT_THAT(rlwe::SamplePolynomialFromPrng<uint_m>(-10, prng.get(),
                                                     this->params14_.get()),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be a non-negative integer")));
}

TYPED_TEST(PolynomialTest,
           SamplePolynomialFromPrngSamplesSamePolynomialFromSameSeed) {
  auto prng1 = this->MakePrng(kPrngSeed);
  auto prng2 = this->MakePrng(kPrngSeed);
  ASSERT_OK_AND_ASSIGN(auto polynomial1,
                       rlwe::SamplePolynomialFromPrng<uint_m>(
                           16, prng1.get(), this->params14_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial2,
                       rlwe::SamplePolynomialFromPrng<uint_m>(
                           16, prng2.get(), this->params14_.get()));

  EXPECT_THAT(polynomial1, Eq(polynomial2));
}

TYPED_TEST(PolynomialTest,
           SamplePolynomialFromPrngSamplesOtherPolynomialFromOtherSeed) {
  auto prng1 = this->MakePrng(kPrngSeed);
  auto prng2 = this->MakePrng(
      "0000000000000000000000000000000000000000000000000000000000000000");
  ASSERT_OK_AND_ASSIGN(auto polynomial1,
                       rlwe::SamplePolynomialFromPrng<uint_m>(
                           16, prng1.get(), this->params14_.get()));
  ASSERT_OK_AND_ASSIGN(auto polynomial2,
                       rlwe::SamplePolynomialFromPrng<uint_m>(
                           16, prng2.get(), this->params14_.get()));

  EXPECT_THAT(polynomial1, Ne(polynomial2));
}

}  // namespace
