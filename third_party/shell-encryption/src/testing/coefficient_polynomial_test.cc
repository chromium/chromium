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

#include "testing/coefficient_polynomial.h"

#include <random>
#include <vector>

#include <google/protobuf/message_lite.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "constants.h"
#include "montgomery.h"
#include "testing/protobuf_matchers.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"

namespace {

using ::rlwe::testing::EqualsProto;
using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

using uint_m = rlwe::MontgomeryInt<rlwe::Uint16>;
using Polynomial = rlwe::testing::CoefficientPolynomial<uint_m>;

const int kDimension = 20;
unsigned int seed = 0;

class PolynomialTest : public ::testing::Test {
 protected:
  PolynomialTest()
      : params14_(uint_m::Params::Create(rlwe::kNewhopeModulus).value()),
        one_(uint_m::ImportOne(params14_.get())),
        zero_(uint_m::ImportZero(params14_.get())) {}

  rlwe::StatusOr<uint_m> SampleNonZero() {
    return uint_m::ImportInt(1 + rand_r(&seed) % (params14_->modulus - 1),
                             params14_.get());
  }

  rlwe::StatusOr<std::vector<uint_m>> SampleRandomCoeffs(int dimension) {
    std::vector<uint_m> v(dimension, zero_);
    for (int j = 0; j < dimension; j++) {
      RLWE_ASSIGN_OR_RETURN(v[j], SampleNonZero());
    }
    return v;
  }

  std::unique_ptr<const uint_m::Params> params14_;
  uint_m one_;
  uint_m zero_;
};

TEST_F(PolynomialTest, Len) {
  for (int dimension = 1; dimension < kDimension; dimension++) {
    ASSERT_OK_AND_ASSIGN(auto coeffs, SampleRandomCoeffs(dimension));
    Polynomial p(coeffs, params14_.get());
    EXPECT_EQ(dimension, p.Len());
  }
}

TEST_F(PolynomialTest, Equality) {
  for (int dimension = 1; dimension < kDimension; dimension++) {
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v, SampleRandomCoeffs(dimension));
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m> w, SampleRandomCoeffs(dimension));

    // Ensure v is really different than w.
    int k = rand_r(&seed) % dimension;
    v[k] = w[k].Add(one_, params14_.get());

    Polynomial p(v, params14_.get()), q(v, params14_.get()),
        r(w, params14_.get());

    // Ensure the equality relations hold.
    EXPECT_EQ(p, q);
    EXPECT_NE(p, r);
    EXPECT_NE(q, r);

    // Check that differing degrees are not equal
    uint_m tmp = v[dimension - 1];
    v[dimension - 1] = zero_;
    Polynomial s(v, params14_.get());
    EXPECT_NE(p, s);
    v[dimension - 1] = tmp;

    // Try flipping one value at a time.
    for (int j = 0; j < dimension; j++) {
      v[j] = v[j].Add(one_, params14_.get());
      Polynomial s(v, params14_.get());
      EXPECT_NE(p, s);
      v[j] = v[j].Sub(one_, params14_.get());
    }
  }
}

TEST_F(PolynomialTest, Degree) {
  for (int dimension = 1; dimension < kDimension; dimension++) {
    // Slowly increase the degree of the polynomial.
    std::vector<uint_m> v(dimension, zero_);
    EXPECT_EQ(0, Polynomial(v, params14_.get()).Degree());
    for (int j = 0; j < dimension; j++) {
      ASSERT_OK_AND_ASSIGN(v[j], SampleNonZero());
      EXPECT_EQ(j, Polynomial(v, params14_.get()).Degree()) << dimension;
    }
  }
}

TEST_F(PolynomialTest, PointwiseAdd) {
  for (int dimension = 1; dimension < kDimension; dimension++) {
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v, SampleRandomCoeffs(dimension));
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m> w, SampleRandomCoeffs(dimension));
    std::vector<uint_m> v_plus_w = v;

    for (int j = 0; j < dimension; j++) {
      v_plus_w[j] = v[j].Add(w[j], params14_.get());
    }

    Polynomial p(v, params14_.get()), q(w, params14_.get()),
        p_plus_q(v_plus_w, params14_.get());

    ASSERT_OK_AND_ASSIGN(Polynomial r, p + q);
    ASSERT_OK_AND_ASSIGN(Polynomial s, q + p);

    EXPECT_EQ(p_plus_q, r);
    EXPECT_EQ(p_plus_q, s);
    EXPECT_EQ(r.Len(), dimension);
    EXPECT_EQ(s.Len(), dimension);
    EXPECT_EQ(r, s);
  }
}

TEST_F(PolynomialTest, PointwiseSub) {
  for (int dimension = 1; dimension < kDimension; dimension++) {
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v, SampleRandomCoeffs(dimension));
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m> w, SampleRandomCoeffs(dimension));
    std::vector<uint_m> v_minus_w = v;
    std::vector<uint_m> w_minus_v = w;

    for (int j = 0; j < dimension; j++) {
      v_minus_w[j] = v[j].Sub(w[j], params14_.get());
      w_minus_v[j] = w[j].Sub(v[j], params14_.get());
    }

    Polynomial p(v, params14_.get()), q(w, params14_.get()),
        p_minus_q(v_minus_w, params14_.get()),
        q_minus_p(w_minus_v, params14_.get());

    ASSERT_OK_AND_ASSIGN(Polynomial r, p - q);
    ASSERT_OK_AND_ASSIGN(Polynomial s, q - p);

    EXPECT_EQ(p_minus_q, r);
    EXPECT_EQ(q_minus_p, s);
    EXPECT_EQ(r.Len(), dimension);
    EXPECT_EQ(s.Len(), dimension);
  }
}

TEST_F(PolynomialTest, AddDifferentDegreePolynomials) {
  // Ensure that polynomials of the same dimension add correctly, even if they
  // have different degrees.
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m> large,
                       SampleRandomCoeffs(kDimension));
  std::vector<uint_m> small(kDimension, zero_);
  std::vector<uint_m> sum = large;

  // Iteratively increase small's degree and check that addition works
  // correctly at each degree.
  for (int j = 0; j < kDimension; j++) {
    ASSERT_OK_AND_ASSIGN(small[j], SampleNonZero());
    sum[j] = sum[j].Add(small[j], params14_.get());

    Polynomial p(small, params14_.get()), q(large, params14_.get()),
        p_plus_q(sum, params14_.get());

    ASSERT_OK_AND_ASSIGN(Polynomial r, p + q);
    ASSERT_OK_AND_ASSIGN(Polynomial s, q + p);

    EXPECT_EQ(p_plus_q, r);
    EXPECT_EQ(p_plus_q, s);
    EXPECT_EQ(r.Len(), kDimension);
    EXPECT_EQ(s.Len(), kDimension);
  }
}

TEST_F(PolynomialTest, ScalarMultiply) {
  for (int dimension = 1; dimension < kDimension; dimension++) {
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v, SampleRandomCoeffs(dimension));
    std::vector<uint_m> v_times_k = v;

    ASSERT_OK_AND_ASSIGN(uint_m k, SampleNonZero());

    for (int j = 0; j < dimension; j++) {
      v_times_k[j] = v[j].Mul(k, params14_.get());
    }

    Polynomial p(v, params14_.get()), p_times_k(v_times_k, params14_.get());
    Polynomial r = p * k;

    EXPECT_EQ(p_times_k, r);
  }
}

TEST_F(PolynomialTest, Multiply) {
  for (int dimension = 2; dimension < kDimension; dimension++) {
    // Create two random polynomials.
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v, SampleRandomCoeffs(dimension));
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m> w, SampleRandomCoeffs(dimension));

    Polynomial p(v, params14_.get()), q(w, params14_.get());
    ASSERT_OK_AND_ASSIGN(Polynomial r, p * q);
    ASSERT_OK_AND_ASSIGN(Polynomial s, q * p);

    std::vector<uint_m> result(2 * dimension, zero_);
    std::vector<uint_m> reduced_result(dimension, zero_);
    for (int j = 0; j < dimension; j++) {
      for (int k = 0; k < dimension; k++) {
        result[j + k] =
            result[j + k].Add(v[j].Mul(w[k], params14_.get()), params14_.get());
      }
    }

    // Take out the multiples of x^N + 1.
    for (int j = 0; j < dimension; j++) {
      // Find the x^j coeff, and subtract that coeff from the x^{j-(N+1)}th
      // coeff.
      reduced_result[j] = result[j].Sub(result[j + dimension], params14_.get());
    }

    EXPECT_EQ(Polynomial(reduced_result, params14_.get()), r);
    EXPECT_EQ(Polynomial(reduced_result, params14_.get()), s);
    EXPECT_EQ(r.Len(), dimension);
    EXPECT_EQ(s.Len(), dimension);
    EXPECT_EQ(r, s);
  }
}

TEST_F(PolynomialTest, MismatchedLengths) {
  for (int dimension : {1, 2, kDimension}) {
    std::vector<uint_m> v(dimension, zero_);
    std::vector<uint_m> w(dimension + 1, zero_);

    Polynomial p(v, params14_.get()), q(w, params14_.get());

    EXPECT_THAT(p + q, StatusIs(::absl::StatusCode::kInvalidArgument,
                                HasSubstr("dimensions mismatched")));
    EXPECT_THAT(p * q, StatusIs(::absl::StatusCode::kInvalidArgument,
                                HasSubstr("dimensions mismatched")));
  }
}

TEST_F(PolynomialTest, MonomialOutOfRange) {
  // Create a random polynomial and a random monomial.
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v, SampleRandomCoeffs(kDimension));

  // Multiply by monomial.
  Polynomial p(v, params14_.get());
  EXPECT_THAT(
      p.MonomialMultiplication(2 * kDimension),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr("must have non-negative degree less than 2n")));
}

TEST_F(PolynomialTest, MultiplyByMonomial) {
  for (int dimension = 2; dimension < kDimension; dimension++) {
    for (unsigned int monomial_degree :
         {0, 1, dimension / 2, dimension - 1, dimension, dimension + 1,
          2 * dimension - 1}) {
      // Create a random polynomial and a random monomial.
      ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v,
                           SampleRandomCoeffs(dimension));
      std::vector<uint_m> w(dimension, zero_);

      // If the monomial degree is >= dimension, we have x^{k} =
      // -x^{k-dimension}.
      if (monomial_degree >= dimension) {
        w[monomial_degree - dimension] = one_.Negate(params14_.get());
      } else {
        w[monomial_degree] = one_;
      }

      Polynomial monomial(w, params14_.get());

      // Multiply by monomial.
      Polynomial p(v, params14_.get());
      ASSERT_OK_AND_ASSIGN(Polynomial r,
                           p.MonomialMultiplication(monomial_degree));

      // Perform polynomial multiplication using * operator.
      ASSERT_OK_AND_ASSIGN(auto res, p* monomial);
      EXPECT_EQ(res, r);
    }
  }
}

TEST_F(PolynomialTest, TrivialSubstitution) {
  // Ensure a substitution of x^1 returns the same polynomial.
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v, SampleRandomCoeffs(kDimension));
  Polynomial p(v, params14_.get());

  ASSERT_OK_AND_ASSIGN(auto res, p.Substitute(1));
  EXPECT_EQ(res, p);
}

TEST_F(PolynomialTest, SubstitutionPowerMalformed) {
  // Ensure substitution fails on powers that are negative, even, and greater
  // than 2 * kDimension.
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v, SampleRandomCoeffs(kDimension));
  Polynomial p(v, params14_.get());

  EXPECT_THAT(p.Substitute(2),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be a non-negative odd integer")));
  EXPECT_THAT(p.Substitute(-10),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be a non-negative odd integer")));
  EXPECT_THAT(p.Substitute(2 * kDimension + 10),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be a non-negative odd integer")));
}

TEST_F(PolynomialTest, SubstituteDimensionPlusOne) {
  // Create a random polynomial.
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m> v, SampleRandomCoeffs(kDimension));

  // Substitute x for x^{n + 1}.
  Polynomial p(v, params14_.get());
  ASSERT_OK_AND_ASSIGN(Polynomial result, p.Substitute(kDimension + 1));

  for (int i = 0; i < kDimension; i = i + 2) {
    // Test even indexed coefficients are equal.
    EXPECT_EQ(result.Coeffs()[i], p.Coeffs()[i]);

    // Test odd index coefficients were negated.
    EXPECT_EQ(result.Coeffs()[i + 1],
              p.Coeffs()[i + 1].Negate(params14_.get()));
  }
}

TEST_F(PolynomialTest, SubstitutionLessThanDimension) {
  int dimension = 8;
  int substitution = 3;

  // Coefficients of a polynomial p(x).
  std::vector<uint_m::Int> coeffs = {1, 1, 3, 0, 2, 3, 1, 3};
  // Coefficients of p(x^3).
  std::vector<uint_m::Int> expected = {
      1, 0, 1, 1, rlwe::kNewhopeModulus - 2, 3, 3, rlwe::kNewhopeModulus - 3};

  std::vector<uint_m> v;
  for (const uint_m::Int& coeff : coeffs) {
    ASSERT_OK_AND_ASSIGN(auto elt, uint_m::ImportInt(coeff, params14_.get()));
    v.push_back(elt);
  }

  ASSERT_OK_AND_ASSIGN(Polynomial result,
                       Polynomial(v, params14_.get()).Substitute(substitution));

  for (int i = 0; i < dimension; i++) {
    EXPECT_EQ(result.Coeffs()[i].ExportInt(params14_.get()), expected[i]);
  }
}

TEST_F(PolynomialTest, SubstitutionGreaterThanDimension) {
  int dimension = 8;
  int substitution = 15;

  // Coefficients of a polynomial p(x).
  std::vector<uint_m::Int> coeffs = {1, 1, 3, 0, 2, 3, 1, 3};
  // Coefficients of p(x^15).
  std::vector<uint_m::Int> expected = {1,
                                       rlwe::kNewhopeModulus - 3,
                                       rlwe::kNewhopeModulus - 1,
                                       rlwe::kNewhopeModulus - 3,
                                       rlwe::kNewhopeModulus - 2,
                                       0,
                                       rlwe::kNewhopeModulus - 3,
                                       rlwe::kNewhopeModulus - 1};

  std::vector<uint_m> v;
  for (const uint_m::Int& coeff : coeffs) {
    ASSERT_OK_AND_ASSIGN(auto elt, uint_m::ImportInt(coeff, params14_.get()));
    v.push_back(elt);
  }

  ASSERT_OK_AND_ASSIGN(Polynomial result,
                       Polynomial(v, params14_.get()).Substitute(substitution));

  for (int i = 0; i < dimension; i++) {
    EXPECT_EQ(result.Coeffs()[i].ExportInt(params14_.get()), expected[i]);
  }
}

TEST_F(PolynomialTest, Serialize) {
  ASSERT_OK_AND_ASSIGN(std::vector<uint_m> coeffs,
                       SampleRandomCoeffs(kDimension));
  for (int dimension = 1; dimension < kDimension; dimension++) {
    // Initialize a vector with a dimension number of coefficients from coeffs.
    std::vector<uint_m> v(coeffs.begin(), coeffs.begin() + dimension);

    Polynomial p(v, params14_.get()), q(v, params14_.get());
    ASSERT_OK_AND_ASSIGN(rlwe::SerializedCoefficientPolynomial serialized_p,
                         p.Serialize());
    ASSERT_OK_AND_ASSIGN(rlwe::SerializedCoefficientPolynomial serialized_q,
                         q.Serialize());
    EXPECT_EQ(serialized_p.SerializeAsString(), serialized_q.SerializeAsString());

    // Ensure that a serialized polynomial can be deserialized.
    ASSERT_OK_AND_ASSIGN(
        auto deserialized_p,
        Polynomial::Deserialize(serialized_p, params14_.get()));
    ASSERT_OK_AND_ASSIGN(
        auto deserialized_q,
        Polynomial::Deserialize(serialized_q, params14_.get()));
    EXPECT_EQ(deserialized_p, deserialized_q);

    // Ensure that the length of a Serialized polynomial is SerializedSize
    // times the number of coefficients.
    EXPECT_LE(serialized_p.coeffs().size(),
              p.Len() * params14_->SerializedSize());
    EXPECT_LE(serialized_q.coeffs().size(),
              q.Len() * params14_->SerializedSize());
  }
}

}  // namespace
