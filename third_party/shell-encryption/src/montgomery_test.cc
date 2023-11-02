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
#include "montgomery.h"

#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <random>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/numeric/int128.h"
#include "constants.h"
#include "serialization.pb.h"
#include "status_macros.h"
#include "testing/parameters.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"
#include "third_party/shell-encryption/base/shell_encryption_export.h"
#include "third_party/shell-encryption/base/shell_encryption_export_template.h"

namespace rlwe {
namespace {

// Random uniform distribution for Uint64.
std::uniform_int_distribution<Uint64> uniform_uint64;

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

const Uint64 kTestingRounds = 10;
const size_t kExhaustiveTest = 1000;

// Generate a random integer of a specified number of bits.
template <class TypeParam>
TypeParam GenerateRandom(unsigned int* seed) {
  std::minstd_rand random(*seed);
  *seed += 1;
  return static_cast<TypeParam>(uniform_uint64(random));
}
// Specialization for absl::uint128 and uint256.
template <>
absl::uint128 GenerateRandom(unsigned int* seed) {
  Uint64 hi = GenerateRandom<Uint64>(seed);
  Uint64 lo = GenerateRandom<Uint64>(seed);
  return absl::MakeUint128(hi, lo);
}

template <typename T>
class EXPORT_TEMPLATE_DECLARE(SHELL_ENCRYPTION_EXPORT) MontgomeryTest : public ::testing::Test {};
TYPED_TEST_SUITE(MontgomeryTest, testing::ModularIntTypes);

TYPED_TEST(MontgomeryTest, ModulusTooLarge) {
  using Int = typename TypeParam::Int;

  unsigned int seed = 0;
  Int modulus;
  for (Int i = 0; i < kTestingRounds; ++i) {
    // Sample an invalid odd modulus in (max(Int)/2, max(Int)).
    modulus =
        (std::numeric_limits<Int>::max() / 2) +
        (GenerateRandom<Int>(&seed) % (std::numeric_limits<Int>::max() / 2));
    modulus |= 1;  // Ensure that the modulus is odd.

    EXPECT_THAT(
        MontgomeryIntParams<Int>::Create(modulus),
        StatusIs(absl::StatusCode::kInvalidArgument,
                 HasSubstr(absl::StrCat("The modulus should be less than 2^",
                                        (sizeof(Int) * 8 - 2), "."))));

    // Sample an even modulus in the allowed range.
    modulus =
        (GenerateRandom<Int>(&seed) % (std::numeric_limits<Int>::max() / 8))
        << 1;
    EXPECT_THAT(
        MontgomeryIntParams<Int>::Create(modulus),
        StatusIs(absl::StatusCode::kInvalidArgument,
                 HasSubstr(absl::StrCat("The modulus should be odd."))));
  }
}

// Verifies that the MontgomeryIntParams code computes the inverse modulus.
TYPED_TEST(MontgomeryTest, ParamsInvModulus) {
  using Int = typename TypeParam::Int;
  using BigInt = typename internal::BigInt<Int>::value_type;
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    EXPECT_EQ(modulus_params->r * modulus_params->inv_r -
                  static_cast<BigInt>(modulus_params->modulus) *
                      modulus_params->inv_modulus,
              1);
  }
}

// Verifies that numbers can be imported and exported properly.
TYPED_TEST(MontgomeryTest, ImportExportInt) {
  using Int = typename TypeParam::Int;
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    for (Int i = 0; i < kTestingRounds; ++i) {
      Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      ASSERT_OK_AND_ASSIGN(auto m,
                           TypeParam::ImportInt(a, modulus_params.get()));
      Int after = m.ExportInt(modulus_params.get());
      EXPECT_EQ(after, a);
    }
  }
}

// Verifies that numbers can be added correctly.
TYPED_TEST(MontgomeryTest, AddSub) {
  using Int = typename TypeParam::Int;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      Int b = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      ASSERT_OK_AND_ASSIGN(TypeParam ma,
                           TypeParam::ImportInt(a, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(TypeParam mb,
                           TypeParam::ImportInt(b, modulus_params.get()));
      TypeParam mc = ma.Add(mb, modulus_params.get());
      Int c = mc.ExportInt(modulus_params.get());

      Int expected = (a + b) % modulus_params->modulus;
      EXPECT_EQ(expected, c);

      TypeParam md = ma.Sub(mb, modulus_params.get());
      Int d = md.ExportInt(modulus_params.get());

      Int expected2 =
          (a + modulus_params->modulus - b) % modulus_params->modulus;
      EXPECT_EQ(expected2, d);
    }
  }
}

TYPED_TEST(MontgomeryTest, InlineAddSub) {
  using Int = typename TypeParam::Int;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      Int b = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      ASSERT_OK_AND_ASSIGN(TypeParam ma,
                           TypeParam::ImportInt(a, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(TypeParam mb,
                           TypeParam::ImportInt(b, modulus_params.get()));
      ma.AddInPlace(mb, modulus_params.get());
      Int c = ma.ExportInt(modulus_params.get());

      Int expected = (a + b) % modulus_params->modulus;
      EXPECT_EQ(expected, c);

      ASSERT_OK_AND_ASSIGN(ma, TypeParam::ImportInt(a, modulus_params.get()));

      ma.SubInPlace(mb, modulus_params.get());
      Int d = ma.ExportInt(modulus_params.get());

      Int expected2 =
          (a + modulus_params->modulus - b) % modulus_params->modulus;
      EXPECT_EQ(expected2, d);
    }
  }
}

// Verifies that equality functions properly.
TYPED_TEST(MontgomeryTest, Equality) {
  using Int = typename TypeParam::Int;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      Int b = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      while (b == a) {
        b = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      }

      ASSERT_OK_AND_ASSIGN(auto ma1,
                           TypeParam::ImportInt(a, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(auto ma2,
                           TypeParam::ImportInt(a, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb1,
                           TypeParam::ImportInt(b, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb2,
                           TypeParam::ImportInt(b, modulus_params.get()));

      EXPECT_TRUE(ma1 == ma2);
      EXPECT_TRUE(ma2 == ma1);
      EXPECT_FALSE(ma1 != ma2);
      EXPECT_FALSE(ma2 != ma1);

      EXPECT_TRUE(mb1 == mb2);
      EXPECT_TRUE(mb2 == mb1);
      EXPECT_FALSE(mb1 != mb2);
      EXPECT_FALSE(mb2 != mb1);

      EXPECT_TRUE(ma1 != mb1);
      EXPECT_TRUE(mb1 != ma1);
      EXPECT_FALSE(ma1 == mb1);
      EXPECT_FALSE(mb1 == ma1);
    }
  }
}

// Verifies that numbers can be negated correctly.
TYPED_TEST(MontgomeryTest, Negate) {
  using Int = typename TypeParam::Int;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    for (Int i = 0; i < 4 * kNewhopeModulus; i++) {
      ASSERT_OK_AND_ASSIGN(auto mi,
                           TypeParam::ImportInt(i, modulus_params.get()));
      EXPECT_EQ(0, mi.Add(mi.Negate(modulus_params.get()), modulus_params.get())
                       .ExportInt(modulus_params.get()));
    }
  }
}

// Verifies that repeated addition works properly.
TYPED_TEST(MontgomeryTest, AddRepeatedly) {
  using Int = typename TypeParam::Int;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    for (int i = 0; i < kTestingRounds; i++) {
      Int sum = 0;
      Int diff = 0;
      TypeParam mont_sum = TypeParam::ImportZero(modulus_params.get());
      TypeParam mont_diff = TypeParam::ImportZero(modulus_params.get());

      for (int j = 0; j < 1000; j++) {
        Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;

        sum = (sum + a) % modulus_params->modulus;
        ASSERT_OK_AND_ASSIGN(auto ma,
                             TypeParam::ImportInt(a, modulus_params.get()));
        mont_sum = mont_sum.Add(ma, modulus_params.get());

        diff = (diff + modulus_params->modulus - a) % modulus_params->modulus;
        mont_diff = mont_diff.Sub(ma, modulus_params.get());
      }

      EXPECT_EQ(sum, mont_sum.ExportInt(modulus_params.get()));
      EXPECT_EQ(diff, mont_diff.ExportInt(modulus_params.get()));
    }
  }
}

// Verifies that numbers can be multiplied correctly.
TYPED_TEST(MontgomeryTest, Multiply) {
  using Int = typename TypeParam::Int;
  using BigInt = typename internal::BigInt<Int>::value_type;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    // Test over many random values.
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      Int b = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      ASSERT_OK_AND_ASSIGN(auto ma,
                           TypeParam::ImportInt(a, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb,
                           TypeParam::ImportInt(b, modulus_params.get()));
      TypeParam mc = ma.Mul(mb, modulus_params.get());
      Int c = mc.ExportInt(modulus_params.get());

      Int expected =
          static_cast<Int>((static_cast<BigInt>(a) * static_cast<BigInt>(b)) %
                           static_cast<BigInt>(modulus_params->modulus));
      EXPECT_EQ(expected, c);
    }
    // Test the multiplication of the maximum values together.
    Int a = modulus_params->modulus - 1;
    ASSERT_OK_AND_ASSIGN(auto ma,
                         TypeParam::ImportInt(a, modulus_params.get()));

    TypeParam mb = ma.Mul(ma, modulus_params.get());
    Int b = mb.ExportInt(modulus_params.get());

    EXPECT_EQ(1, b);
  }
}

TYPED_TEST(MontgomeryTest, MulInPlace) {
  using Int = typename TypeParam::Int;
  using BigInt = typename internal::BigInt<Int>::value_type;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    // Test over many random values.
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      Int b = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      ASSERT_OK_AND_ASSIGN(auto ma,
                           TypeParam::ImportInt(a, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb,
                           TypeParam::ImportInt(b, modulus_params.get()));
      ma.MulInPlace(mb, modulus_params.get());
      Int c = ma.ExportInt(modulus_params.get());

      Int expected =
          static_cast<Int>((static_cast<BigInt>(a) * static_cast<BigInt>(b)) %
                           static_cast<BigInt>(modulus_params->modulus));
      EXPECT_EQ(expected, c);
    }
  }
}

TYPED_TEST(MontgomeryTest, MulConstantInPlace) {
  using Int = typename TypeParam::Int;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    // Test over many random values.
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      Int b = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      ASSERT_OK_AND_ASSIGN(auto ma,
                           TypeParam::ImportInt(a, modulus_params.get()));
      auto ma_clone = ma;
      ASSERT_OK_AND_ASSIGN(auto mb,
                           TypeParam::ImportInt(b, modulus_params.get()));
      auto constants_tuple = mb.GetConstant(modulus_params.get());
      auto constant = std::get<0>(constants_tuple);
      auto constant_barrett = std::get<1>(constants_tuple);
      ma.MulInPlace(mb, modulus_params.get());
      ma_clone.MulConstantInPlace(constant, constant_barrett,
                                  modulus_params.get());

      EXPECT_EQ(ma.ExportInt(modulus_params.get()),
                ma_clone.ExportInt(modulus_params.get()));
    }
  }
}

// Verifies that repeated addition works properly.
TYPED_TEST(MontgomeryTest, MultiplyRepeatedly) {
  using Int = typename TypeParam::Int;
  using BigInt = typename internal::BigInt<Int>::value_type;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    for (int i = 0; i < kTestingRounds; i++) {
      BigInt prod = 1;
      TypeParam mont_prod = TypeParam::ImportOne(modulus_params.get());

      for (int j = 0; j < 1000; j++) {
        Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;

        prod = (prod * static_cast<BigInt>(a)) %
               static_cast<BigInt>(modulus_params->modulus);
        ASSERT_OK_AND_ASSIGN(auto ma,
                             TypeParam::ImportInt(a, modulus_params.get()));
        mont_prod = mont_prod.Mul(ma, modulus_params.get());
      }

      EXPECT_EQ(static_cast<Int>(prod),
                mont_prod.ExportInt(modulus_params.get()));
    }
  }
}

// Test the entire space for a small modulus.
TYPED_TEST(MontgomeryTest, SmallModulus) {
  using Int = typename TypeParam::Int;
  using BigInt = typename internal::BigInt<Int>::value_type;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    const BigInt modulus = static_cast<BigInt>(modulus_params->modulus);
    for (Int a = 0; a < kExhaustiveTest; a++) {
      Int b = a + 1;
      BigInt a_BigInt = static_cast<BigInt>(a);
      BigInt b_BigInt = static_cast<BigInt>(b);
      ASSERT_OK_AND_ASSIGN(auto ma,
                           TypeParam::ImportInt(a, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb,
                           TypeParam::ImportInt(b, modulus_params.get()));
      TypeParam mc = ma.Add(mb, modulus_params.get());

      // Equality.
      if (a_BigInt % modulus == b_BigInt % modulus) {
        EXPECT_TRUE(ma == mb);
        EXPECT_FALSE(ma != mb);
      } else {
        EXPECT_TRUE(ma != mb);
        EXPECT_FALSE(ma == mb);
      }

      // Addition.
      EXPECT_EQ(static_cast<Int>((a_BigInt + b_BigInt) % modulus),
                mc.ExportInt(modulus_params.get()));

      // Negation.
      EXPECT_EQ(
          static_cast<Int>((2 * modulus - a_BigInt) % modulus),
          (ma.Negate(modulus_params.get())).ExportInt(modulus_params.get()));
      EXPECT_EQ(
          static_cast<Int>((2 * modulus - b_BigInt) % modulus),
          (mb.Negate(modulus_params.get())).ExportInt(modulus_params.get()));
      EXPECT_EQ(
          static_cast<Int>((4 * modulus - a_BigInt - b_BigInt) % modulus),
          (mc.Negate(modulus_params.get())).ExportInt(modulus_params.get()));

      // Subtraction.
      EXPECT_EQ(
          static_cast<Int>((2 * modulus - a_BigInt + b_BigInt) % modulus),
          (mb.Sub(ma, modulus_params.get()).ExportInt(modulus_params.get())));
      EXPECT_EQ(
          static_cast<Int>((2 * modulus - b_BigInt + a_BigInt) % modulus),
          (ma.Sub(mb, modulus_params.get()).ExportInt(modulus_params.get())));

      // Multiplication and commutativity.
      EXPECT_EQ(
          static_cast<Int>((a_BigInt * b_BigInt) % modulus),
          (ma.Mul(mb, modulus_params.get())).ExportInt(modulus_params.get()));
      EXPECT_EQ(
          static_cast<Int>((a_BigInt * b_BigInt) % modulus),
          (mb.Mul(ma, modulus_params.get())).ExportInt(modulus_params.get()));
    }
  }
}

TYPED_TEST(MontgomeryTest, ModExpModulus) {
  using Int = typename TypeParam::Int;
  using BigInt = typename internal::BigInt<Int>::value_type;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    const BigInt modulus = static_cast<BigInt>(modulus_params->modulus);
    for (int i = 0; i < kTestingRounds; i++) {
      BigInt expected = 1;
      Int base = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      for (Int exp = 0; exp < kExhaustiveTest; exp++) {
        ASSERT_OK_AND_ASSIGN(auto base_m,
                             TypeParam::ImportInt(base, modulus_params.get()));
        auto actual_m = base_m.ModExp(exp, modulus_params.get());
        Int actual = actual_m.ExportInt(modulus_params.get());
        ASSERT_EQ(actual, expected);

        expected *= static_cast<BigInt>(base);
        expected %= modulus;
      }
    }
  }
}

TYPED_TEST(MontgomeryTest, InverseModulus) {
  using Int = typename TypeParam::Int;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % modulus_params->modulus;
      ASSERT_OK_AND_ASSIGN(auto a_m,
                           TypeParam::ImportInt(a, modulus_params.get()));
      TypeParam inv = a_m.MultiplicativeInverse(modulus_params.get());
      ASSERT_EQ(
          (a_m.Mul(inv, modulus_params.get())).ExportInt(modulus_params.get()),
          1);
    }
  }
}

TYPED_TEST(MontgomeryTest, Serialization) {
  using Int = typename TypeParam::Int;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    for (Int i = 0; i < kExhaustiveTest; i++) {
      Int input_int = i % modulus_params->modulus;
      // Serialize and ensure the byte length is as expected.
      ASSERT_OK_AND_ASSIGN(
          auto int_value,
          TypeParam::ImportInt(input_int, modulus_params.get()));

      ASSERT_OK_AND_ASSIGN(std::string serialized,
                           int_value.Serialize(modulus_params.get()));

      EXPECT_EQ(serialized.length(), modulus_params->SerializedSize());

      // Ensure that deserialization works properly.
      ASSERT_OK_AND_ASSIGN(
          auto int_deserialized,
          TypeParam::Deserialize(serialized, modulus_params.get()));
      EXPECT_EQ(int_deserialized, int_value);

      // Ensure that that any bit beyond bit the serialized bit length can be
      // wiped out without issue. That is, ensure that the bit size is accurate.
      serialized[serialized.length() - 1] &=
          (static_cast<Uint8>(1)
           << (modulus_params->log_modulus - 8 * (serialized.length() - 1))) -
          1;
      ASSERT_OK_AND_ASSIGN(
          auto int_deserialized2,
          TypeParam::Deserialize(serialized, modulus_params.get()));
      EXPECT_EQ(int_deserialized2, int_value);
    }
  }
}

TYPED_TEST(MontgomeryTest, ExceedMaxNumCoeffVectorSerialization) {
  int num_coeffs = kMaxNumCoeffs + 1;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    std::vector<TypeParam> coeffs;
    for (int i = 0; i < num_coeffs; ++i) {
      coeffs.push_back(TypeParam::ImportOne(modulus_params.get()));
    }
    EXPECT_THAT(TypeParam::SerializeVector(coeffs, modulus_params.get()),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(absl::StrCat(
                             "Number of coefficients, ", num_coeffs,
                             ", cannot be larger than ", kMaxNumCoeffs, "."))));
  }
}

TYPED_TEST(MontgomeryTest, EmptyVectorSerialization) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    std::vector<TypeParam> coeffs;
    EXPECT_THAT(TypeParam::SerializeVector(coeffs, modulus_params.get()),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr("Cannot serialize an empty vector")));
  }
}

TYPED_TEST(MontgomeryTest, VectorSerialization) {
  // Prng to generate random values
  auto prng = absl::make_unique<rlwe::testing::TestingPrng>(0);

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    for (int num_coeffs = 3; num_coeffs <= 25; ++num_coeffs) {
      std::vector<TypeParam> coeffs;
      coeffs.reserve(num_coeffs);
      for (int i = 0; i < num_coeffs; ++i) {
        ASSERT_OK_AND_ASSIGN(
            auto int_value,
            TypeParam::ImportRandom(prng.get(), modulus_params.get()));
        coeffs.push_back(int_value);
      }
      ASSERT_OK_AND_ASSIGN(
          std::string serialized,
          TypeParam::SerializeVector(coeffs, modulus_params.get()));
      int expected_size = (num_coeffs * modulus_params->log_modulus + 7) / 8;
      EXPECT_EQ(serialized.size(), expected_size);
      ASSERT_OK_AND_ASSIGN(auto deserialized,
                           TypeParam::DeserializeVector(num_coeffs, serialized,
                                                        modulus_params.get()));
      EXPECT_EQ(deserialized.size(), num_coeffs);
      for (int i = 0; i < num_coeffs; ++i) {
        EXPECT_EQ(coeffs[i], deserialized[i]);
      }
    }
  }
}

TYPED_TEST(MontgomeryTest, ExceedMaxNumCoeffVectorDeserialization) {
  int num_coeffs = kMaxNumCoeffs + 1;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    EXPECT_THAT(TypeParam::DeserializeVector(num_coeffs, std::string(),
                                             modulus_params.get()),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(absl::StrCat(
                             "Number of coefficients, ", num_coeffs,
                             ", cannot be larger than ", kMaxNumCoeffs, "."))));
  }
}

TYPED_TEST(MontgomeryTest, NegativeVectorDeserialization) {
  int num_coeffs = -1;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    EXPECT_THAT(
        TypeParam::DeserializeVector(num_coeffs, std::string(),
                                     modulus_params.get()),
        StatusIs(absl::StatusCode::kInvalidArgument,
                 HasSubstr("Number of coefficients must be non-negative.")));
  }
}

TYPED_TEST(MontgomeryTest, ImportRandomWithPrngWithSameKeys) {
  unsigned seed = 0;
  unsigned int seed_prng = GenerateRandom<unsigned int>(&seed);

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    auto prng1 = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng);
    auto prng2 = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng);

    ASSERT_OK_AND_ASSIGN(
        auto r1, TypeParam::ImportRandom(prng1.get(), modulus_params.get()));
    ASSERT_OK_AND_ASSIGN(
        auto r2, TypeParam::ImportRandom(prng2.get(), modulus_params.get()));
    EXPECT_EQ(r1, r2);
  }
}

TYPED_TEST(MontgomeryTest, ImportRandomWithPrngWithDifferentKeys) {
  unsigned seed = 0;
  unsigned int seed_prng1 = GenerateRandom<unsigned int>(&seed);
  unsigned int seed_prng2 = seed_prng1 + 1;  // Different seed

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    auto prng1 = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng1);
    auto prng2 = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng2);
    ASSERT_OK_AND_ASSIGN(
        auto r1, TypeParam::ImportRandom(prng1.get(), modulus_params.get()));
    ASSERT_OK_AND_ASSIGN(
        auto r2, TypeParam::ImportRandom(prng2.get(), modulus_params.get()));
    EXPECT_NE(r1, r2);
  }
}

// Verifies that Barrett reductions functions properly.
TYPED_TEST(MontgomeryTest, VerifyBarrett) {
  using Int = typename TypeParam::Int;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    // Test over a selection of the possible input space.
    for (unsigned int j = 0; j < kTestingRounds; j++) {
      unsigned int seed = j;

      for (int i = 0; i < kTestingRounds; i++) {
        // Verify Barrett reduction up to max(Int).
        Int a = modulus_params->modulus +
                (GenerateRandom<Int>(&seed) %
                 (std::numeric_limits<Int>::max() - modulus_params->modulus));
        EXPECT_EQ(modulus_params->BarrettReduce(a), a % params.modulus);
      }
    }
  }
}

TYPED_TEST(MontgomeryTest, BatchOperations) {
  using Int = typename TypeParam::Int;

  unsigned int seed = 0;
  unsigned int seed_prng = GenerateRandom<unsigned int>(&seed);
  auto prng = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng);

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));

    for (size_t length : {1, 2, 7, 32, 500, 1024}) {
      std::vector<TypeParam> a, b;
      std::vector<Int> b_constant, b_constant_barrett;
      std::vector<TypeParam> expected_add, expected_sub, expected_mul;
      TypeParam scalar =
          TypeParam::ImportRandom(prng.get(), modulus_params.get())
              .value();
      auto scalar_constants_tuple = scalar.GetConstant(modulus_params.get());
      auto scalar_constant = std::get<0>(scalar_constants_tuple);
      auto scalar_constant_barrett = std::get<1>(scalar_constants_tuple);
      std::vector<TypeParam> expected_add_scalar, expected_sub_scalar,
          expected_mul_scalar;
      for (size_t i = 0; i < length; i++) {
        a.push_back(TypeParam::ImportRandom(prng.get(), modulus_params.get())
                        .value());
        b.push_back(TypeParam::ImportRandom(prng.get(), modulus_params.get())
                        .value());
        auto constants_tuple = b[i].GetConstant(modulus_params.get());
        auto constant = std::get<0>(constants_tuple);
        auto constant_barrett = std::get<1>(constants_tuple); 
        b_constant.push_back(constant);
        b_constant_barrett.push_back(constant_barrett);
        expected_add.push_back(a[i].Add(b[i], modulus_params.get()));
        expected_sub.push_back(a[i].Sub(b[i], modulus_params.get()));
        expected_mul.push_back(a[i].Mul(b[i], modulus_params.get()));
        expected_add_scalar.push_back(a[i].Add(scalar, modulus_params.get()));
        expected_sub_scalar.push_back(a[i].Sub(scalar, modulus_params.get()));
        expected_mul_scalar.push_back(a[i].Mul(scalar, modulus_params.get()));
      }

      ASSERT_OK_AND_ASSIGN(std::vector<TypeParam> add,
                           TypeParam::BatchAdd(a, b, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(std::vector<TypeParam> sub,
                           TypeParam::BatchSub(a, b, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(std::vector<TypeParam> mul,
                           TypeParam::BatchMul(a, b, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(
          std::vector<TypeParam> mul_constant,
          TypeParam::BatchMulConstant(a, b_constant, b_constant_barrett,
                                      modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(
          std::vector<TypeParam> add_scalar,
          TypeParam::BatchAdd(a, scalar, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(
          std::vector<TypeParam> sub_scalar,
          TypeParam::BatchSub(a, scalar, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(
          std::vector<TypeParam> mul_scalar,
          TypeParam::BatchMul(a, scalar, modulus_params.get()));
      ASSERT_OK_AND_ASSIGN(std::vector<TypeParam> mul_scalar_constant,
                           TypeParam::BatchMulConstant(a, scalar_constant,
                                                       scalar_constant_barrett,
                                                       modulus_params.get()));

      EXPECT_EQ(add.size(), expected_add.size());
      EXPECT_EQ(sub.size(), expected_sub.size());
      EXPECT_EQ(mul.size(), expected_mul.size());
      EXPECT_EQ(mul_constant.size(), expected_mul.size());
      EXPECT_EQ(add_scalar.size(), expected_add_scalar.size());
      EXPECT_EQ(sub_scalar.size(), expected_sub_scalar.size());
      EXPECT_EQ(mul_scalar.size(), expected_mul_scalar.size());
      EXPECT_EQ(mul_scalar_constant.size(), expected_mul_scalar.size());
      for (size_t i = 0; i < length; i++) {
        EXPECT_EQ(add[i].ExportInt(modulus_params.get()),
                  expected_add[i].ExportInt(modulus_params.get()));
        EXPECT_EQ(sub[i].ExportInt(modulus_params.get()),
                  expected_sub[i].ExportInt(modulus_params.get()));
        EXPECT_EQ(mul[i].ExportInt(modulus_params.get()),
                  expected_mul[i].ExportInt(modulus_params.get()));
        EXPECT_EQ(mul_constant[i].ExportInt(modulus_params.get()),
                  expected_mul[i].ExportInt(modulus_params.get()));
        EXPECT_EQ(add_scalar[i].ExportInt(modulus_params.get()),
                  expected_add_scalar[i].ExportInt(modulus_params.get()));
        EXPECT_EQ(sub_scalar[i].ExportInt(modulus_params.get()),
                  expected_sub_scalar[i].ExportInt(modulus_params.get()));
        EXPECT_EQ(mul_scalar[i].ExportInt(modulus_params.get()),
                  expected_mul_scalar[i].ExportInt(modulus_params.get()));
        EXPECT_EQ(mul_scalar_constant[i].ExportInt(modulus_params.get()),
                  expected_mul_scalar[i].ExportInt(modulus_params.get()));
      }
    }
  }
}

TYPED_TEST(MontgomeryTest, BatchOperationsFailsWithVectorsOfDifferentSize) {
  using Int = typename TypeParam::Int;

  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    for (size_t length_a : {1, 2, 7, 32, 500, 1024}) {
      for (size_t length_b : {1, 2, 7, 32, 500, 1024}) {
        if (length_a != length_b) {
          std::vector<TypeParam> a(length_a,
                                   TypeParam::ImportZero(modulus_params.get()));
          std::vector<Int> a_constant(length_a, static_cast<Int>(0));
          std::vector<TypeParam> b(length_b,
                                   TypeParam::ImportZero(modulus_params.get()));
          std::vector<Int> b_constant(length_b, static_cast<Int>(0));

          EXPECT_THAT(
              TypeParam::BatchAdd(a, b, modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              TypeParam::BatchAddInPlace(&a, b, modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              TypeParam::BatchSub(a, b, modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              TypeParam::BatchSubInPlace(&a, b, modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              TypeParam::BatchMul(a, b, modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              TypeParam::BatchMulInPlace(&a, b, modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              TypeParam::BatchMulConstant(a, b_constant, b_constant,
                                          modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              TypeParam::BatchMulConstantInPlace(&a, b_constant, b_constant,
                                                 modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              TypeParam::BatchMulConstantInPlace(&a, a_constant, b_constant,
                                                 modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
        }
      }
    }
  }
}

// This PRNG tests templating with a Prng that does not inherit from SecurePrng.
class FakePrng {
 public:
  StatusOr<Uint8> Rand8() { return 0; }
  StatusOr<Uint64> Rand64() { return 0; }
};

TYPED_TEST(MontgomeryTest, PrngTemplateParameterizationWorks) {
  for (const auto& params :
       rlwe::testing::ContextParameters<TypeParam>::Value()) {
    ASSERT_OK_AND_ASSIGN(auto modulus_params,
                         TypeParam::Params::Create(params.modulus));
    FakePrng prng;
    ASSERT_OK(TypeParam::ImportRandom(&prng, modulus_params.get()));
  }
}

}  // namespace
}  // namespace rlwe
