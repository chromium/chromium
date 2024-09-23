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

#include "int256.h"

#include <algorithm>
#include <random>
#include <sstream>
#include <type_traits>
#include <utility>

#include <glog/logging.h>
#include <gtest/gtest.h>
#include "absl/container/fixed_array.h"
#include "absl/numeric/int128.h"

namespace rlwe {

TEST(Int256, AllTests) {
  uint256 zero(0);
  uint256 one(1);
  uint256 minus_one(-1);
  uint256 minus_one_2arg(-1, -1);
  uint256 one_2arg(0, 1);
  uint256 two(0, 2);
  uint256 three(0, 3);
  uint256 big(2000, 2);
  uint256 big_minus_one(2000, 1);
  uint256 bigger(2001, 1);
  uint256 biggest(kuint256max);
  uint256 high_low(1, 0);
  uint256 low_high(0, absl::Uint128Max());
  EXPECT_LT(one, two);
  EXPECT_GT(two, one);
  EXPECT_LT(one, big);
  EXPECT_LT(one, big);
  EXPECT_EQ(one, one_2arg);
  EXPECT_NE(one, two);
  EXPECT_GT(big, one);
  EXPECT_GE(big, two);
  EXPECT_GE(big, big_minus_one);
  EXPECT_GT(big, big_minus_one);
  EXPECT_LT(big_minus_one, big);
  EXPECT_LE(big_minus_one, big);
  EXPECT_NE(big_minus_one, big);
  EXPECT_LT(big, biggest);
  EXPECT_LE(big, biggest);
  EXPECT_GT(biggest, big);
  EXPECT_GE(biggest, big);
  EXPECT_EQ(big, ~~big);
  EXPECT_EQ(one, one | one);
  EXPECT_EQ(big, big | big);
  EXPECT_EQ(one, one | zero);
  EXPECT_EQ(one, one & one);
  EXPECT_EQ(big, big & big);
  EXPECT_EQ(zero, one & zero);
  EXPECT_EQ(zero, big & ~big);
  EXPECT_EQ(zero, one ^ one);
  EXPECT_EQ(zero, big ^ big);
  EXPECT_EQ(one, one ^ zero);
  EXPECT_LE(minus_one_2arg, biggest);
  EXPECT_LE(minus_one, biggest);
  EXPECT_EQ(minus_one, minus_one_2arg);

  // Shift operators.
  EXPECT_EQ(big, big << 0);
  EXPECT_EQ(big, big >> 0);
  EXPECT_GT(big << 1, big);
  EXPECT_LT(big >> 1, big);
  EXPECT_EQ(big, (big << 10) >> 10);
  EXPECT_EQ(big, (big >> 1) << 1);
  EXPECT_EQ(one, (one << 80) >> 80);
  EXPECT_EQ(zero, (one >> 80) << 80);
  EXPECT_EQ(zero, big >> 256);
  EXPECT_EQ(zero, big << 256);

  // Shift assignments.
  uint256 big_copy = big;
  EXPECT_EQ(big << 0, big_copy <<= 0);
  big_copy = big;
  EXPECT_EQ(big >> 0, big_copy >>= 0);
  big_copy = big;
  EXPECT_EQ(big << 1, big_copy <<= 1);
  big_copy = big;
  EXPECT_EQ(big >> 1, big_copy >>= 1);
  big_copy = big;
  EXPECT_EQ(big << 10, big_copy <<= 10);
  big_copy = big;
  EXPECT_EQ(big >> 10, big_copy >>= 10);
  big_copy = big;
  EXPECT_EQ(big << 64, big_copy <<= 64);
  big_copy = big;
  EXPECT_EQ(big >> 64, big_copy >>= 64);
  big_copy = big;
  EXPECT_EQ(big << 73, big_copy <<= 73);
  big_copy = big;
  EXPECT_EQ(big >> 73, big_copy >>= 73);
  big_copy = big;
  EXPECT_EQ(big << 128, big_copy <<= 128);
  big_copy = big;
  EXPECT_EQ(big >> 128, big_copy >>= 128);
  big_copy = big;
  EXPECT_EQ(big << 192, big_copy <<= 192);
  big_copy = big;
  EXPECT_EQ(big >> 192, big_copy >>= 192);
  big_copy = big;
  EXPECT_EQ(big << 256, big_copy <<= 256);
  big_copy = big;
  EXPECT_EQ(big >> 256, big_copy >>= 256);

  EXPECT_EQ(Uint256High128(biggest), absl::Uint128Max());
  EXPECT_EQ(Uint256Low128(biggest), absl::Uint128Max());
  EXPECT_EQ(zero + one, one);
  EXPECT_EQ(zero + minus_one, minus_one);
  EXPECT_EQ(one + minus_one, zero);
  EXPECT_EQ(one - minus_one, two);
  EXPECT_EQ(one + one, two);
  EXPECT_EQ(big_minus_one + one, big);
  EXPECT_EQ(one - one, zero);
  EXPECT_EQ(one - zero, one);
  EXPECT_EQ(zero - one, biggest);
  EXPECT_EQ(big - big, zero);
  EXPECT_EQ(big - one, big_minus_one);
  EXPECT_EQ(big + low_high, bigger);
  EXPECT_EQ(biggest + 1, zero);
  EXPECT_EQ(minus_one + 1, zero);
  EXPECT_EQ(zero - 1, biggest);
  EXPECT_EQ(zero - 1, minus_one);
  EXPECT_EQ(high_low - one, low_high);
  EXPECT_EQ(low_high + one, high_low);
  EXPECT_EQ(Uint256High128((uint256(1) << 128) - 1), 0);
  EXPECT_EQ(Uint256Low128((uint256(1) << 128) - 1), absl::Uint128Max());
  EXPECT_TRUE(!!one);
  EXPECT_TRUE(!!high_low);
  EXPECT_FALSE(!!zero);
  EXPECT_FALSE(!one);
  EXPECT_FALSE(!high_low);
  EXPECT_TRUE(!zero);
  // These 4 checks are explicitly for the comparison operators.
  EXPECT_TRUE(zero == 0);
  EXPECT_FALSE(zero != 0);
  EXPECT_FALSE(one == 0);
  EXPECT_TRUE(one != 0);

  uint256 test = zero;
  EXPECT_EQ(++test, one);
  EXPECT_EQ(test, one);
  EXPECT_EQ(test++, one);
  EXPECT_EQ(test, two);
  EXPECT_EQ(test -= 2, zero);
  EXPECT_EQ(test, zero);
  EXPECT_EQ(test += 2, two);
  EXPECT_EQ(test, two);
  EXPECT_EQ(--test, one);
  EXPECT_EQ(test, one);
  EXPECT_EQ(test--, one);
  EXPECT_EQ(test, zero);
  EXPECT_EQ(test |= three, three);
  EXPECT_EQ(test &= one, one);
  EXPECT_EQ(test ^= three, two);
  EXPECT_EQ(test >>= 1, one);
  EXPECT_EQ(test <<= 1, two);

  EXPECT_EQ(big, -(-big));
  EXPECT_EQ(two, -((-one) - 1));
  EXPECT_EQ(kuint256max, -one);
  EXPECT_EQ(zero, -zero);
  EXPECT_EQ(one, -minus_one);
  EXPECT_EQ(-one, minus_one);

  // Test ++ and -- when hi and lo are both modified.
  test = low_high;
  EXPECT_EQ(++test, high_low);
  EXPECT_EQ(test, high_low);
  EXPECT_EQ(--test, low_high);
  EXPECT_EQ(test, low_high);
  EXPECT_EQ(test++, low_high);
  EXPECT_EQ(test, high_low);
  EXPECT_EQ(test--, high_low);
  EXPECT_EQ(test, low_high);
  test = minus_one;
  EXPECT_EQ(++test, zero);
  EXPECT_EQ(test, zero);
  EXPECT_EQ(--test, minus_one);
  EXPECT_EQ(test, minus_one);
  EXPECT_EQ(test++, minus_one);
  EXPECT_EQ(test, zero);
  EXPECT_EQ(test--, zero);
  EXPECT_EQ(test, minus_one);

  LOG(INFO) << one;
  LOG(INFO) << big_minus_one;
}

TEST(Int256, PodTests) {
  uint256_pod pod = {12345, 67890};
  uint256 from_pod(pod);
  EXPECT_EQ(12345, Uint256High128(from_pod));
  EXPECT_EQ(67890, Uint256Low128(from_pod));

  uint256 zero(0);
  uint256_pod zero_pod = {0, 0};
  uint256 one(1);
  uint256_pod one_pod = {0, 1};
  uint256 two(2);
  uint256_pod two_pod = {0, 2};
  uint256 three(3);
  uint256_pod three_pod = {0, 3};
  uint256 big(1, 0);
  uint256_pod big_pod = {1, 0};

  EXPECT_EQ(zero, zero_pod);
  EXPECT_EQ(zero_pod, zero);
  EXPECT_EQ(zero_pod, zero_pod);
  EXPECT_EQ(one, one_pod);
  EXPECT_EQ(one_pod, one);
  EXPECT_EQ(one_pod, one_pod);
  EXPECT_EQ(two, two_pod);
  EXPECT_EQ(two_pod, two);
  EXPECT_EQ(two_pod, two_pod);

  EXPECT_NE(one, two_pod);
  EXPECT_NE(one_pod, two);
  EXPECT_NE(one_pod, two_pod);

  EXPECT_LT(one, two_pod);
  EXPECT_LT(one_pod, two);
  EXPECT_LT(one_pod, two_pod);
  EXPECT_LE(one, one_pod);
  EXPECT_LE(one_pod, one);
  EXPECT_LE(one_pod, one_pod);
  EXPECT_LE(one, two_pod);
  EXPECT_LE(one_pod, two);
  EXPECT_LE(one_pod, two_pod);

  EXPECT_GT(two, one_pod);
  EXPECT_GT(two_pod, one);
  EXPECT_GT(two_pod, one_pod);
  EXPECT_GE(two, two_pod);
  EXPECT_GE(two_pod, two);
  EXPECT_GE(two_pod, two_pod);
  EXPECT_GE(two, one_pod);
  EXPECT_GE(two_pod, one);
  EXPECT_GE(two_pod, one_pod);

  EXPECT_EQ(three, one | two_pod);
  EXPECT_EQ(three, one_pod | two);
  EXPECT_EQ(three, one_pod | two_pod);
  EXPECT_EQ(one, three & one_pod);
  EXPECT_EQ(one, three_pod & one);
  EXPECT_EQ(one, three_pod & one_pod);
  EXPECT_EQ(two, three ^ one_pod);
  EXPECT_EQ(two, three_pod ^ one);
  EXPECT_EQ(two, three_pod ^ one_pod);
  EXPECT_EQ(two, three & (~one));
  EXPECT_EQ(three, ~~three);

  EXPECT_EQ(two, two_pod << 0);
  EXPECT_EQ(two, one_pod << 1);
  EXPECT_EQ(big, one_pod << 128);
  EXPECT_EQ(zero, one_pod << 256);
  EXPECT_EQ(two, two_pod >> 0);
  EXPECT_EQ(one, two_pod >> 1);
  EXPECT_EQ(one, big_pod >> 128);

  EXPECT_EQ(one, zero + one_pod);
  EXPECT_EQ(one, zero_pod + one);
  EXPECT_EQ(one, zero_pod + one_pod);
  EXPECT_EQ(one, two - one_pod);
  EXPECT_EQ(one, two_pod - one);
  EXPECT_EQ(one, two_pod - one_pod);
}

TEST(Int256, OperatorAssignReturnRef) {
  uint256 v(1);
  (v += 4) -= 3;
  EXPECT_EQ(2, v);
}

TEST(Int256, Multiply) {
  uint256 a, b, c;

  // Zero test.
  a = 0;
  b = 0;
  c = a * b;
  EXPECT_EQ(0, c);

  // Max carries.
  a = uint256(0) - 1;
  b = uint256(0) - 1;
  c = a * b;
  EXPECT_EQ(1, c);

  // Self-operation with max carries.
  c = uint256(0) - 1;
  c *= c;
  EXPECT_EQ(1, c);

  // 1-bit x 1-bit.
  for (int i = 0; i < 128; ++i) {
    for (int j = 0; j < 128; ++j) {
      a = uint256(1) << i;
      b = uint256(1) << j;
      c = a * b;
      EXPECT_EQ(uint256(1) << (i + j), c);
    }
  }

  // Verified with dc.
  a = uint256(absl::MakeUint128(static_cast<Uint64>(0xffffeeeeddddcccc),
                                static_cast<Uint64>(0xffffeeeeddddcccc)),
              absl::MakeUint128(static_cast<Uint64>(0xbbbbaaaa99998888),
                                static_cast<Uint64>(0xbbbbaaaa99998888)));
  b = uint256(absl::MakeUint128(static_cast<Uint64>(0x7777666655554444),
                                static_cast<Uint64>(0x7777666655554444)),
              absl::MakeUint128(static_cast<Uint64>(0x3333222211110000),
                                static_cast<Uint64>(0x3333222211110000)));
  c = a * b;
  EXPECT_EQ(uint256(absl::MakeUint128(static_cast<Uint64>(0x0B60BCDF06D3A4FA),
                                      static_cast<Uint64>(0x37C054321A2B4567)),
                    absl::MakeUint128(static_cast<Uint64>(0xA3D7111116C170A3),
                                      static_cast<Uint64>(0xBF25975319080000))),
            c);
  EXPECT_EQ(0, c - b * a);
  EXPECT_EQ(a * a - b * b, (a + b) * (a - b));

  // Verified with dc.
  a = uint256(absl::MakeUint128(static_cast<Uint64>(0x0123456789abcdef),
                                static_cast<Uint64>(0x0123456789abcdef)),
              absl::MakeUint128(static_cast<Uint64>(0xfedcba9876543210),
                                static_cast<Uint64>(0xfedcba9876543210)));
  b = uint256(absl::MakeUint128(static_cast<Uint64>(0x02468ace13579bdf),
                                static_cast<Uint64>(0x02468ace13579bdf)),
              absl::MakeUint128(static_cast<Uint64>(0xfdb97531eca86420),
                                static_cast<Uint64>(0xfdb97531eca86420)));
  c = a * b;
  EXPECT_EQ(uint256(absl::MakeUint128(static_cast<Uint64>(0x361CDAA0607023AD),
                                      static_cast<Uint64>(0xC86E51A688F16415)),
                    absl::MakeUint128(static_cast<Uint64>(0x64F2DE16AB6A4222),
                                      static_cast<Uint64>(0x342D0BBF48948200))),
            c);
  EXPECT_EQ(0, c - b * a);
  EXPECT_EQ(a * a - b * b, (a + b) * (a - b));
}

TEST(Int256, AliasTests) {
  uint256 x1(1, 2);
  uint256 x2(2, 4);
  x1 += x1;
  EXPECT_EQ(x2, x1);

  uint256 x3(1, absl::uint128(1) << 127);
  uint256 x4(3, 0);
  x3 += x3;
  EXPECT_EQ(x4, x3);
}

TEST(Int256, DivideAndMod) {
  // a := q * b + r
  uint256 a, b, q, r;

  // a == b
  a = 123;
  b = a;
  q = a / b;
  r = a % b;
  EXPECT_EQ(1, q);
  EXPECT_EQ(0, r);

  // Zero test.
  a = 0;
  b = 123;
  q = a / b;
  r = a % b;
  EXPECT_EQ(0, q);
  EXPECT_EQ(0, r);

  a = uint256(0, absl::MakeUint128(static_cast<Uint64>(0x530eda741c71d4c3),
                                   static_cast<Uint64>(0xbf25975319080000)));
  q = uint256(0, absl::MakeUint128(static_cast<Uint64>(0x4de2cab081),
                                   static_cast<Uint64>(0x14c34ab4676e4bab)));
  b = uint256(0x1110001);
  r = uint256(0x3eb455);
  ASSERT_EQ(a, q * b + r);  // Sanity-check.

  uint256 result_q, result_r;
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(q, result_q);
  EXPECT_EQ(r, result_r);

  // Try the other way around.
  using std::swap;
  swap(q, b);
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(q, result_q);
  EXPECT_EQ(r, result_r);
  // Restore.
  swap(b, q);

  // Dividend < divisor; result should be q:0 r:<dividend>.
  swap(a, b);
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(0, result_q);
  EXPECT_EQ(a, result_r);
  // Try the other way around.
  swap(a, q);
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(0, result_q);
  EXPECT_EQ(a, result_r);
  // Restore.
  swap(q, a);
  swap(b, a);

  // Try a large remainder.
  b = a / 2 + 1;
  uint256 expected_r(
      0, absl::MakeUint128(static_cast<Uint64>(0x29876d3a0e38ea61),
                           static_cast<Uint64>(0xdf92cba98c83ffff)));
  // Sanity checks.
  ASSERT_EQ(a / 2 - 1, expected_r);
  ASSERT_EQ(a, b + expected_r);
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(1, result_q);
  EXPECT_EQ(expected_r, result_r);
}

TEST(Int256, DivideAndModRandomInputs) {
  const int kNumIters = 1 << 18;
  std::minstd_rand random(testing::UnitTest::GetInstance()->random_seed());
  std::uniform_int_distribution<Uint64> uniform_uint64;
  for (int i = 0; i < kNumIters; ++i) {
    const uint256 a(uniform_uint64(random), uniform_uint64(random));
    const uint256 b(uniform_uint64(random), uniform_uint64(random));
    if (b == 0) {
      continue;  // Avoid a div-by-zero.
    }
    const uint256 q = a / b;
    const uint256 r = a % b;
    ASSERT_EQ(a, b * q + r);
  }
}

TEST(Int256, ConstexprTest) {
  constexpr uint256 one = 1;
  constexpr uint256_pod pod = {2, 3};
  constexpr uint256 from_pod = pod;
  constexpr uint256 minus_two = -2;

  EXPECT_EQ(one, uint256(1));
  EXPECT_EQ(from_pod, uint256(2, 3));
  EXPECT_EQ(minus_two, uint256(absl::uint128(-1), absl::uint128(-2)));
}

TEST(Int256, Traits) {
  EXPECT_TRUE(std::is_trivially_copy_constructible<uint256>::value);
  EXPECT_TRUE(std::is_trivially_copy_assignable<uint256>::value);
  EXPECT_TRUE(std::is_trivially_destructible<uint256>::value);
}

TEST(Int256, OStream) {
  struct {
    uint256 val;
    std::ios_base::fmtflags flags;
    std::streamsize width;
    char fill;
    const char* rep;
  } cases[] = {
      // zero with different bases
      {uint256(0), std::ios::dec, 0, '_', "0"},
      {uint256(0), std::ios::oct, 0, '_', "0"},
      {uint256(0), std::ios::hex, 0, '_', "0"},
      // crossover between lo_ and hi_
      {uint256(0, -1), std::ios::dec, 0, '_',
       "340282366920938463463374607431768211455"},
      {uint256(0, -1), std::ios::oct, 0, '_',
       "3777777777777777777777777777777777777777777"},
      {uint256(0, -1), std::ios::hex, 0, '_',
       "ffffffffffffffffffffffffffffffff"},
      {uint256(1, 0), std::ios::dec, 0, '_',
       "340282366920938463463374607431768211456"},
      {uint256(1, 0), std::ios::oct, 0, '_',
       "4000000000000000000000000000000000000000000"},
      {uint256(1, 0), std::ios::hex, 0, '_',
       "100000000000000000000000000000000"},
      // just the top bit
      {uint256(static_cast<Uint64>(0x8000000000000000), 0), std::ios::dec, 0,
       '_', "3138550867693340381917894711603833208051177722232017256448"},
      {uint256(static_cast<Uint64>(0x8000000000000000), 0), std::ios::oct, 0,
       '_', "4000000000000000000000000000000000000000000000000000000000000000"},
      {uint256(static_cast<Uint64>(0x8000000000000000), 0), std::ios::hex, 0,
       '_', "800000000000000000000000000000000000000000000000"},
      // maximum uint256 value
      {uint256(-1, -1), std::ios::dec, 0, '_',
       "115792089237316195423570985008687907853"
       "269984665640564039457584007913129639935"},
      {uint256(-1, -1), std::ios::oct, 0, '_',
       "1777777777777777777777777777777777777777777"
       "7777777777777777777777777777777777777777777"},
      {uint256(-1, -1), std::ios::hex, 0, '_',
       "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"},
      // uppercase
      {uint256(-1, -1), std::ios::hex | std::ios::uppercase, 0, '_',
       "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"},
      // showbase
      {uint256(1), std::ios::dec | std::ios::showbase, 0, '_', "1"},
      {uint256(1), std::ios::oct | std::ios::showbase, 0, '_', "01"},
      {uint256(1), std::ios::hex | std::ios::showbase, 0, '_', "0x1"},
      // showbase does nothing on zero
      {uint256(0), std::ios::dec | std::ios::showbase, 0, '_', "0"},
      {uint256(0), std::ios::oct | std::ios::showbase, 0, '_', "0"},
      {uint256(0), std::ios::hex | std::ios::showbase, 0, '_', "0"},
      // showpos does nothing on unsigned types
      {uint256(1), std::ios::dec | std::ios::showpos, 0, '_', "1"},
      // padding
      {uint256(9), std::ios::dec, 6, '_', "_____9"},
      {uint256(12345), std::ios::dec, 6, '_', "_12345"},
      // left adjustment
      {uint256(9), std::ios::dec | std::ios::left, 6, '_', "9_____"},
      {uint256(12345), std::ios::dec | std::ios::left, 6, '_', "12345_"},
  };
  for (size_t i = 0; i < ABSL_ARRAYSIZE(cases); ++i) {
    std::ostringstream os;
    os.flags(cases[i].flags);
    os.width(cases[i].width);
    os.fill(cases[i].fill);
    os << cases[i].val;
    EXPECT_EQ(cases[i].rep, os.str());
  }
}

TEST(Int256, SizeOfTest) { EXPECT_EQ(sizeof(uint256), 32); }

}  // namespace rlwe
