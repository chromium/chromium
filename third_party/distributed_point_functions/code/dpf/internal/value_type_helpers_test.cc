// Copyright 2021 Google LLC
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

#include "dpf/internal/value_type_helpers.h"

#include <stdint.h>

#include <array>
#include <string>
#include <tuple>

#include "absl/base/config.h"
#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "dpf/distributed_point_function.pb.h"
#include "dpf/int_mod_n.h"
#include "dpf/internal/status_matchers.h"
#include "dpf/tuple.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace distributed_point_functions {
namespace dpf_internal {
namespace {

constexpr int kDefaultSecurityParameter = 40;

TEST(ValueTypeHelperTest, ValueTypesAreEqualFailsOnInvalidValueTypes) {
  ValueType type1, type2;

  EXPECT_THAT(ValueTypesAreEqual(type1, type2),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Both arguments must be valid ValueTypes"));
}

TEST(ValueTypeHelperTest, BitsNeededFailsOnInvalidValueType) {
  EXPECT_THAT(
      BitsNeeded(ValueType{}, kDefaultSecurityParameter),
      StatusIs(absl::StatusCode::kInvalidArgument,
               testing::StartsWith("BitsNeeded: Unsupported ValueType")));
}

template <typename T>
class ValueTypeIntegerTest : public testing::Test {};
using IntegerTypes =
    ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t, absl::uint128>;
TYPED_TEST_SUITE(ValueTypeIntegerTest, IntegerTypes);

TYPED_TEST(ValueTypeIntegerTest, ToValueTypeIntegers) {
  ValueType value_type = ValueTypeHelper<TypeParam>::ToValueType();

  EXPECT_TRUE(value_type.has_integer());
  EXPECT_EQ(value_type.integer().bitsize(), sizeof(TypeParam) * 8);
}

TYPED_TEST(ValueTypeIntegerTest, TestValueTypesAreEqual) {
  ValueType value_type_1 = ValueTypeHelper<TypeParam>::ToValueType(),
            value_type_2;
  value_type_2.mutable_integer()->set_bitsize(sizeof(TypeParam) * 8);

  DPF_ASSERT_OK_AND_ASSIGN(bool equal,
                           ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_TRUE(equal);
  DPF_ASSERT_OK_AND_ASSIGN(equal,
                           ValueTypesAreEqual(value_type_2, value_type_1));
  EXPECT_TRUE(equal);
}

TYPED_TEST(ValueTypeIntegerTest, TestValueTypesAreNotEqual) {
  ValueType value_type_1 = ValueTypeHelper<TypeParam>::ToValueType(),
            value_type_2;
  value_type_2.mutable_integer()->set_bitsize(sizeof(TypeParam) * 8 * 2);

  DPF_ASSERT_OK_AND_ASSIGN(bool equal,
                           ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_FALSE(equal);
  DPF_ASSERT_OK_AND_ASSIGN(equal,
                           ValueTypesAreEqual(value_type_2, value_type_1));
  EXPECT_FALSE(equal);
}

TYPED_TEST(ValueTypeIntegerTest, ValueConversionFailsIfNotInteger) {
  Value value;
  value.mutable_tuple();

  EXPECT_THAT(ValueTypeHelper<TypeParam>::FromValue(value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "The given Value is not an integer"));
}

TYPED_TEST(ValueTypeIntegerTest, ValueConversionFailsIfInvalidIntegerCase) {
  Value value;
  value.mutable_integer();

  EXPECT_THAT(ValueTypeHelper<TypeParam>::FromValue(value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Unknown value case for the given integer Value"));
}

TYPED_TEST(ValueTypeIntegerTest, ValueConversionFailsIfValueOutOfRange) {
  Value value;
  auto value_64 = uint64_t{1} << 32;
  value.mutable_integer()->set_value_uint64(value_64);

  if (sizeof(TypeParam) >= sizeof(uint64_t)) {
    DPF_EXPECT_OK(ValueTypeHelper<TypeParam>::FromValue(value));
  } else {
    EXPECT_THAT(ValueTypeHelper<TypeParam>::FromValue(value),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         absl::StrCat("Value (= ", value_64,
                                      ") too large for the given type T (size ",
                                      sizeof(TypeParam), ")")));
  }
}

template <typename T>
class ValueTypeTupleTest : public testing::Test {};

template <typename T, int... bits>
struct TupleTestParam {
  using Tuple = T;
  static constexpr int ExpectedNumElements() { return sizeof...(bits); };
  static constexpr std::array<int, ExpectedNumElements()> ExpectedBitSizes() {
    return {bits...};
  }
};

// We only test tuples consisting of integers here.
using TupleTypes = ::testing::Types<
    TupleTestParam<Tuple<uint64_t>, 64>,
    TupleTestParam<Tuple<uint64_t, uint64_t>, 64, 64>,
    TupleTestParam<Tuple<uint32_t, absl::uint128, uint8_t>, 32, 128, 8>,
    TupleTestParam<Tuple<uint8_t, uint8_t, uint8_t, uint8_t>, 8, 8, 8, 8>>;
TYPED_TEST_SUITE(ValueTypeTupleTest, TupleTypes);

TYPED_TEST(ValueTypeTupleTest, ToValueTypeTuples) {
  ValueType value_type =
      ValueTypeHelper<typename TypeParam::Tuple>::ToValueType();

  constexpr int expected_num_elements = TypeParam::ExpectedNumElements();
  EXPECT_TRUE(value_type.has_tuple());
  ASSERT_EQ(std::tuple_size<typename TypeParam::Tuple::Base>(),
            expected_num_elements);  // Sanity check for test parameters.
  EXPECT_EQ(value_type.tuple().elements_size(), expected_num_elements);

  std::array<int, expected_num_elements> expected_bit_sizes =
      TypeParam::ExpectedBitSizes();
  for (int i = 0; i < expected_num_elements; ++i) {
    EXPECT_TRUE(value_type.tuple().elements(i).has_integer());
    EXPECT_EQ(value_type.tuple().elements(i).integer().bitsize(),
              expected_bit_sizes[i]);
  }
}

TYPED_TEST(ValueTypeTupleTest, BitsNeededEqualsCompileTimeTypeSize) {
  ValueType value_type =
      ValueTypeHelper<typename TypeParam::Tuple>::ToValueType();

  DPF_ASSERT_OK_AND_ASSIGN(int bitsize,
                           BitsNeeded(value_type, kDefaultSecurityParameter));

  EXPECT_EQ(bitsize, TotalBitSize<typename TypeParam::Tuple>());
}

TYPED_TEST(ValueTypeTupleTest, ValueConversionFailsIfValueIsNotATuple) {
  Value value;
  value.mutable_integer();

  EXPECT_THAT(ValueTypeHelper<Tuple<uint32_t>>::FromValue(value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "The given Value is not a tuple"));
}

TEST(ValueTypeTupleTest, ValueConversionFailsIfValueSizeDoesntMatchTupleSize) {
  Value value;
  value.mutable_tuple()->add_elements()->mutable_integer()->set_value_uint64(
      1234);

  using TupleType = Tuple<uint32_t, uint32_t>;
  EXPECT_THAT(
      ValueTypeHelper<TupleType>::FromValue(value),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          "The tuple in the given Value has the wrong number of elements"));
}

TEST(ValueTypeTupleTest, TestValueTypesAreEqual) {
  using T1 = Tuple<uint32_t, absl::uint128, uint8_t>;
  using T2 = Tuple<uint32_t, absl::uint128, uint8_t>;

  ValueType value_type_1 = ValueTypeHelper<T1>::ToValueType();
  ValueType value_type_2 = ValueTypeHelper<T2>::ToValueType();

  DPF_ASSERT_OK_AND_ASSIGN(bool equal,
                           ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_TRUE(equal);
  DPF_ASSERT_OK_AND_ASSIGN(equal,
                           ValueTypesAreEqual(value_type_2, value_type_1));
  EXPECT_TRUE(equal);
}

TEST(ValueTypeTupleTest, TestValueTypesAreNotEqual) {
  using T1 = Tuple<uint32_t, absl::uint128, uint8_t>;
  using T2 = Tuple<uint32_t, absl::uint128, uint16_t>;

  ValueType value_type_1 = ValueTypeHelper<T1>::ToValueType();
  ValueType value_type_2 = ValueTypeHelper<T2>::ToValueType();

  DPF_ASSERT_OK_AND_ASSIGN(bool equal,
                           ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_FALSE(equal);
  DPF_ASSERT_OK_AND_ASSIGN(equal,
                           ValueTypesAreEqual(value_type_2, value_type_1));
  EXPECT_FALSE(equal);
}

TEST(ValueTypeTupleTest, TestFromBytesWithConcreteExample) {
  std::string bytes = "A 128 bit string";

  auto tuple = FromBytes<Tuple<uint64_t, uint64_t>>(bytes);
  EXPECT_EQ(std::get<0>(tuple.value()), FromBytes<uint64_t>("A 128 bi"));
  EXPECT_EQ(std::get<1>(tuple.value()), FromBytes<uint64_t>("t string"));
}

TEST(ValueTypeTupleTest, TestFromBytesWithConcreteExampleForIntModN) {
  constexpr uint32_t kModulus = 4294967291u;
  using MyIntModN = IntModN<uint32_t, kModulus>;
  std::string bytes = "A 128+32 bit string.";

  absl::uint128 block = FromBytes<absl::uint128>("A 128+32 bit str");
  MyIntModN expected_0(static_cast<uint32_t>(block % kModulus));
  block /= kModulus;
  block <<= (8 * sizeof(uint32_t));
  block |= FromBytes<uint32_t>("ing.");
  MyIntModN expected_1(static_cast<uint32_t>(block % kModulus));

  auto tuple = FromBytes<Tuple<MyIntModN, MyIntModN>>(bytes).value();
  EXPECT_EQ(std::get<0>(tuple), expected_0);
  EXPECT_EQ(std::get<1>(tuple), expected_1);
}

template <typename T>
class ValueTypeIntModNTest : public testing::Test {};
using IntModNTypes = ::testing::Types<
    IntModN<uint32_t, 4>, IntModN<uint32_t, 4294967291u>,
    IntModN<uint64_t, 4294967291ull>, IntModN<uint64_t, 1000000000000ull>
#ifdef ABSL_HAVE_INTRINSIC_INT128
    ,
    IntModN<absl::uint128, (unsigned __int128)(absl::MakeUint128(
                               65535u, 18446744073709551551ull))>  // 2**80-65
#endif
    >;
TYPED_TEST_SUITE(ValueTypeIntModNTest, IntModNTypes);

TYPED_TEST(ValueTypeIntModNTest, ToValueType) {
  ValueType value_type = ValueTypeHelper<TypeParam>::ToValueType();

  EXPECT_TRUE(value_type.type_case() == ValueType::kIntModN);
  EXPECT_EQ(value_type.int_mod_n().base_integer().bitsize(),
            sizeof(typename TypeParam::Base) * 8);
  DPF_ASSERT_OK_AND_ASSIGN(
      absl::uint128 modulus,
      ValueIntegerToUint128(value_type.int_mod_n().modulus()));
  EXPECT_EQ(modulus, absl::uint128{TypeParam::modulus()});
}

TYPED_TEST(ValueTypeIntModNTest, TestValueTypesAreEqual) {
  ValueType value_type_1 = ValueTypeHelper<TypeParam>::ToValueType(),
            value_type_2;

  value_type_2.mutable_int_mod_n()->mutable_base_integer()->set_bitsize(
      sizeof(TypeParam) * 8);
  *(value_type_2.mutable_int_mod_n()->mutable_modulus()) =
      Uint128ToValueInteger(TypeParam::modulus());

  DPF_ASSERT_OK_AND_ASSIGN(bool equal,
                           ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_TRUE(equal);
  DPF_ASSERT_OK_AND_ASSIGN(equal,
                           ValueTypesAreEqual(value_type_2, value_type_1));
  EXPECT_TRUE(equal);
}

TYPED_TEST(ValueTypeIntModNTest, TestValueTypesAreDifferentBase) {
  ValueType value_type_1 = ValueTypeHelper<TypeParam>::ToValueType(),
            value_type_2 = value_type_1;

  value_type_2.mutable_int_mod_n()->mutable_base_integer()->set_bitsize(
      sizeof(TypeParam) * 8 * 2);

  DPF_ASSERT_OK_AND_ASSIGN(bool equal,
                           ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_FALSE(equal);
  DPF_ASSERT_OK_AND_ASSIGN(equal,
                           ValueTypesAreEqual(value_type_2, value_type_1));
  EXPECT_FALSE(equal);
};

TYPED_TEST(ValueTypeIntModNTest, TestValueTypesAreDifferentModulus) {
  ValueType value_type_1 = ValueTypeHelper<TypeParam>::ToValueType(),
            value_type_2 = value_type_1;

  *(value_type_2.mutable_int_mod_n()->mutable_modulus()) =
      Uint128ToValueInteger(TypeParam::modulus() - 1);

  DPF_ASSERT_OK_AND_ASSIGN(bool equal,
                           ValueTypesAreEqual(value_type_1, value_type_2));
  EXPECT_FALSE(equal);
  DPF_ASSERT_OK_AND_ASSIGN(equal,
                           ValueTypesAreEqual(value_type_2, value_type_1));
  EXPECT_FALSE(equal);
}

TYPED_TEST(ValueTypeIntModNTest, ValueTypesAreEqualFailsWhenModulusInvalid) {
  ValueType value_type_1 = ValueTypeHelper<TypeParam>::ToValueType(),
            value_type_2 = value_type_1;

  value_type_2.mutable_int_mod_n()->clear_modulus();

  EXPECT_THAT(ValueTypesAreEqual(value_type_1, value_type_2),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Unknown value case for the given integer Value"));
}

TYPED_TEST(ValueTypeIntModNTest, ValueConversionFailsIfNotInteger) {
  Value value;
  value.mutable_tuple();

  EXPECT_THAT(ValueTypeHelper<TypeParam>::FromValue(value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "The given Value is not an IntModN"));
}

TYPED_TEST(ValueTypeIntModNTest, ValueConversionFailsIfTooLargeForModulus) {
  Value value;
  *(value.mutable_int_mod_n()) = Uint128ToValueInteger(TypeParam::modulus());

  EXPECT_THAT(ValueTypeHelper<TypeParam>::FromValue(value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       testing::HasSubstr("is larger than kModulus")));
}

}  // namespace

}  // namespace dpf_internal
}  // namespace distributed_point_functions
