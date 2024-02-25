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

#include "dpf/int_mod_n.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/base/config.h"
#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "dpf/internal/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace distributed_point_functions {
namespace {

constexpr double kFeasibleSecurityParameter = 40;
constexpr double kUnfeasibleSecurityParameter = 95;
constexpr int kNumSamples = 5;

template <typename T>
class IntModNTest : public testing::Test {};
using IntModNTypes = ::testing::Types<
    IntModN<uint32_t, 4294967291u>,             //  2**32-5
    IntModN<uint64_t, 18446744073709551557ull>  //  2**64-59
#ifdef ABSL_HAVE_INTRINSIC_INT128
    ,
    IntModN<absl::uint128, (unsigned __int128)(absl::MakeUint128(
                               65535u, 18446744073709551551ull))>  // 2**80-65
#endif
    >;
TYPED_TEST_SUITE(IntModNTest, IntModNTypes);

TYPED_TEST(IntModNTest, DefaultValueIsZero) {
  TypeParam a;
  EXPECT_EQ(a.value(), 0);
}

TYPED_TEST(IntModNTest, SetValueWorks) {
  TypeParam a;
  EXPECT_EQ(a.value(), 0);
  a = 23;
  EXPECT_EQ(a.value(), 23);
}

TYPED_TEST(IntModNTest, AdditionWithoutWrapAroundWorks) {
  TypeParam a;
  TypeParam b;
  a += b;
  EXPECT_EQ(a.value(), 0);
  b = 23;
  a += b;
  EXPECT_EQ(a.value(), 23);
  b = 4294967200;
  a += b;
  EXPECT_EQ(a.value(), 4294967223);
}

TYPED_TEST(IntModNTest, AdditionWithWrapAroundWorks) {
  TypeParam a;
  TypeParam b;
  a += b;
  EXPECT_EQ(a.value(), 0);
  b = 23;
  a += b;
  EXPECT_EQ(a.value(), 23);
  b = TypeParam::modulus() - 10;
  a += b;
  EXPECT_EQ(a.value(), 13);
}

TYPED_TEST(IntModNTest, NegationWorks) {
  TypeParam a(10);
  TypeParam b = -a;
  EXPECT_EQ(a + b, TypeParam(0));
}

TYPED_TEST(IntModNTest, GetNumBytesRequiredFailsIfUnfeasible) {
  absl::StatusOr<int> result =
      TypeParam::GetNumBytesRequired(kNumSamples, kUnfeasibleSecurityParameter);
  EXPECT_THAT(result, dpf_internal::StatusIs(
                          absl::StatusCode::kInvalidArgument,
                          testing::StartsWith(absl::StrFormat(
                              "For num_samples = 5 and kModulus = %d",
                              absl::uint128(TypeParam::modulus())))));
}

TYPED_TEST(IntModNTest, GetNumBytesRequiredSucceedsIfFeasible) {
  absl::StatusOr<int> result =
      TypeParam::GetNumBytesRequired(5, kFeasibleSecurityParameter);
  EXPECT_EQ(result.ok(), true);
}

TYPED_TEST(IntModNTest, SampleFailsIfUnfeasible) {
  absl::StatusOr<int> r_getnum =
      TypeParam::GetNumBytesRequired(5, kFeasibleSecurityParameter);
  EXPECT_EQ(r_getnum.ok(), true);

  std::string bytes = std::string(16, '#');
  EXPECT_GT(r_getnum.value(), bytes.size());
  std::vector<TypeParam> samples(5);
  absl::Status r_sample = TypeParam::SampleFromBytes(
      bytes, kFeasibleSecurityParameter, absl::MakeSpan(samples));
  EXPECT_EQ(r_sample.ok(), false);
  EXPECT_THAT(
      r_sample,
      dpf_internal::StatusIs(
          absl::StatusCode::kInvalidArgument,
          "The number of bytes provided (16) is insufficient for the required "
          "statistical security and number of samples."));
}

TYPED_TEST(IntModNTest, SampleSucceedsIfFeasible) {
  absl::StatusOr<int> r_getnum =
      TypeParam::GetNumBytesRequired(5, kFeasibleSecurityParameter);
  EXPECT_EQ(r_getnum.ok(), true);

  std::string bytes = std::string(r_getnum.value(), '#');
  std::vector<TypeParam> samples(5);
  absl::Status r_sample = TypeParam::SampleFromBytes(
      bytes, kFeasibleSecurityParameter, absl::MakeSpan(samples));
  EXPECT_EQ(r_sample.ok(), true);
}

TYPED_TEST(IntModNTest, FirstEntryOfSamplesIsAsExpected) {
  absl::StatusOr<int> r_getnum =
      TypeParam::GetNumBytesRequired(5, kFeasibleSecurityParameter);
  EXPECT_EQ(r_getnum.ok(), true);

  std::string bytes = std::string(r_getnum.value(), '#');
  std::vector<TypeParam> samples(5);
  absl::Status r_sample = TypeParam::SampleFromBytes(
      bytes, kFeasibleSecurityParameter, absl::MakeSpan(samples));
  EXPECT_EQ(r_sample.ok(), true);
  EXPECT_EQ(
      samples[0].value(),
      TypeParam::template ConvertBytesTo<absl::uint128>(bytes.substr(0, 16)) %
          TypeParam::modulus());
}

using BaseInteger = uint32_t;
constexpr BaseInteger kModulus32 = 4294967291u;  // 2**32 - 5
using MyIntModN = IntModN<BaseInteger, kModulus32>;

TEST(IntModNTest, SampleFromBytesWorksInConcreteExample) {
  absl::StatusOr<int> r_getnum =
      MyIntModN::GetNumBytesRequired(5, kFeasibleSecurityParameter);
  EXPECT_EQ(r_getnum.ok(), true);
  EXPECT_EQ(*r_getnum, 32);
  std::string bytes = "this is a length 32 test string.";
  EXPECT_EQ(bytes.size(), 32);

  std::vector<MyIntModN> samples(5);
  absl::Status r_sample = MyIntModN::SampleFromBytes(
      bytes, kFeasibleSecurityParameter, absl::MakeSpan(samples));
  EXPECT_EQ(r_sample.ok(), true);
  absl::uint128 r =
      MyIntModN::ConvertBytesTo<absl::uint128>("this is a length");
  EXPECT_EQ(samples[0].value(), r % MyIntModN::modulus());
  r /= MyIntModN::modulus();
  r <<= (sizeof(MyIntModN::Base) * 8);
  r |= MyIntModN::ConvertBytesTo<MyIntModN::Base>(" 32 ");
  EXPECT_EQ(samples[1].value(), r % MyIntModN::modulus());
  r /= MyIntModN::modulus();
  r <<= (sizeof(MyIntModN::Base) * 8);
  r |= MyIntModN::ConvertBytesTo<MyIntModN::Base>("test");
  EXPECT_EQ(samples[2].value(), r % MyIntModN::modulus());
  r /= MyIntModN::modulus();
  r <<= (sizeof(MyIntModN::Base) * 8);
  r |= MyIntModN::ConvertBytesTo<MyIntModN::Base>(" str");
  EXPECT_EQ(samples[3].value(), r % MyIntModN::modulus());
  r /= MyIntModN::modulus();
  r <<= (sizeof(MyIntModN::Base) * 8);
  r |= MyIntModN::ConvertBytesTo<MyIntModN::Base>("ing.");
  EXPECT_EQ(samples[4].value(), r % MyIntModN::modulus());
}

TEST(IntModNTest, SampleFromBytesFailsAsExpectedInConcreteExample) {
  absl::StatusOr<int> r_getnum =
      MyIntModN::GetNumBytesRequired(5, kFeasibleSecurityParameter);
  EXPECT_EQ(r_getnum.ok(), true);
  EXPECT_EQ(*r_getnum, 32);
  std::string bytes = "this is a length 32 test string.";
  EXPECT_EQ(bytes.size(), 32);

  std::vector<MyIntModN> samples(5);
  absl::Status r_sample = MyIntModN::SampleFromBytes(
      bytes, kFeasibleSecurityParameter, absl::MakeSpan(samples));
  EXPECT_EQ(r_sample.ok(), true);
  absl::uint128 r =
      MyIntModN::ConvertBytesTo<absl::uint128>("this is a length");
  EXPECT_EQ(samples[0].value(), r % MyIntModN::modulus());
  r /= MyIntModN::modulus();
  r <<= (sizeof(MyIntModN::Base) * 8);
  r |= MyIntModN::ConvertBytesTo<MyIntModN::Base>(" 32 ");
  EXPECT_EQ(samples[1].value(), r % MyIntModN::modulus());
  r /= MyIntModN::modulus();
  r <<= (sizeof(MyIntModN::Base) * 8);
  r |= MyIntModN::ConvertBytesTo<MyIntModN::Base>("test");
  EXPECT_EQ(samples[2].value(), r % MyIntModN::modulus());
  r /= MyIntModN::modulus();
  r <<= (sizeof(MyIntModN::Base) * 8);
  r |= MyIntModN::ConvertBytesTo<MyIntModN::Base>(" str");
  EXPECT_EQ(samples[3].value(), r % MyIntModN::modulus());
  r /= MyIntModN::modulus();
  r <<= (sizeof(MyIntModN::Base) * 8);
  r |= MyIntModN::ConvertBytesTo<MyIntModN::Base>("ing#");  // # instead of .
  EXPECT_NE(samples[4].value(), r % MyIntModN::modulus());
}

// Test if IntModN operators are in fact constexpr. This will fail to compile
// otherwise.
constexpr MyIntModN TestAddition() { return MyIntModN(2) + MyIntModN(5); }
static_assert(TestAddition().value() == 7,
              "constexpr addition of IntModNs incorrect");

constexpr MyIntModN TestSubtraction() { return MyIntModN(5) - MyIntModN(2); }
static_assert(TestSubtraction().value() == 3,
              "constexpr subtraction of IntModNs incorrect");

constexpr MyIntModN TestAssignment() {
  MyIntModN x(0);
  x = 5;
  return x;
}
static_assert(TestAssignment().value() == 5,
              "constexpr assignment to IntModN incorrect");

#ifdef ABSL_HAVE_INTRINSIC_INT128
constexpr unsigned __int128 kModulus128 =
    (unsigned __int128)(-1);  // 2**128 - 159
using MyIntModN128 = IntModN<unsigned __int128, kModulus128>;
constexpr MyIntModN128 TestAddition128() {
  return MyIntModN128(2) + MyIntModN128(5);
}
static_assert(TestAddition128().value() == 7,
              "constexpr addition of IntModNs incorrect");
#endif

}  // namespace
}  // namespace distributed_point_functions
