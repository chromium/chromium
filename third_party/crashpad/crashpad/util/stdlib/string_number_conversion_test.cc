// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/stdlib/string_number_conversion.h"

#include <sys/types.h>

#include <array>
#include <limits>
#include <type_traits>

#include "base/stl_util.h"
#include "gtest/gtest.h"

#define STRINGIFY(a) STR(a)
#define STR(a) #a

template <typename TValueType>
struct TestSpecification {
  const char* string;
  bool valid;
  TValueType value;
};

// Signed 32-bit test data
template <typename TIntType,
          typename std::enable_if<std::is_integral<TIntType>::value &&
                                  std::is_signed<TIntType>::value &&
                                  (sizeof(TIntType) == 4)>::type* = nullptr>
static constexpr std::array<TestSpecification<TIntType>, 61> kTestDataFunc() {
  return {{
      {"", false, 0},
      {"0", true, 0},
      {"1", true, 1},
      {"2147483647", true, std::numeric_limits<TIntType>::max()},
      {"2147483648", false, 0},
      {"4294967295", false, 0},
      {"4294967296", false, 0},
      {"-1", true, -1},
      {"-2147483648", true, std::numeric_limits<TIntType>::min()},
      {"-2147483649", false, 0},
      {"00", true, 0},
      {"01", true, 1},
      {"-01", true, -1},
      {"+2", true, 2},
      {"0x10", true, 16},
      {"-0x10", true, -16},
      {"+0x20", true, 32},
      {"0xf", true, 15},
      {"0xg", false, 0},
      {"0x7fffffff", true, std::numeric_limits<TIntType>::max()},
      {"0x7FfFfFfF", true, std::numeric_limits<TIntType>::max()},
      {"0x80000000", false, 0},
      {"0xFFFFFFFF", false, 0},
      {"-0x7fffffff", true, -2147483647},
      {"-0x80000000", true, std::numeric_limits<TIntType>::min()},
      {"-0x80000001", false, 0},
      {"-0xffffffff", false, 0},
      {"0x100000000", false, 0},
      {"0xabcdef", true, 11259375},
      {"010", true, 8},
      {"-010", true, -8},
      {"+020", true, 16},
      {"07", true, 7},
      {"08", false, 0},
      {" 0", false, 0},
      {"0 ", false, 0},
      {" 0 ", false, 0},
      {" 1", false, 0},
      {"1 ", false, 0},
      {" 1 ", false, 0},
      {"a2", false, 0},
      {"2a", false, 0},
      {"2a2", false, 0},
      {".0", false, 0},
      {".1", false, 0},
      {"-.2", false, 0},
      {"+.3", false, 0},
      {"1.23", false, 0},
      {"-273.15", false, 0},
      {"+98.6", false, 0},
      {"1e1", false, 0},
      {"1E1", false, 0},
      {"0x123p4", false, 0},
      {"infinity", false, 0},
      {"NaN", false, 0},
      {"-9223372036854775810", false, 0},
      {"-9223372036854775809", false, 0},
      {"9223372036854775808", false, 0},
      {"9223372036854775809", false, 0},
      {"18446744073709551615", false, 0},
      {"18446744073709551616", false, 0},
  }};
}

// Unsigned 32-bit test data
template <typename TIntType,
          typename std::enable_if<std::is_integral<TIntType>::value &&
                                  !std::is_signed<TIntType>::value &&
                                  (sizeof(TIntType) == 4)>::type* = nullptr>
static constexpr std::array<TestSpecification<TIntType>, 61> kTestDataFunc() {
  return {{
      {"", false, 0},
      {"0", true, 0},
      {"1", true, 1},
      {"2147483647", true, 2147483647},
      {"2147483648", true, 2147483648},
      {"4294967295", true, std::numeric_limits<TIntType>::max()},
      {"4294967296", false, 0},
      {"-1", false, 0},
      {"-2147483648", false, 0},
      {"-2147483649", false, 0},
      {"00", true, 0},
      {"01", true, 1},
      {"-01", false, 0},
      {"+2", true, 2},
      {"0x10", true, 16},
      {"-0x10", false, 0},
      {"+0x20", true, 32},
      {"0xf", true, 15},
      {"0xg", false, 0},
      {"0x7fffffff", true, 0x7fffffff},
      {"0x7FfFfFfF", true, 0x7fffffff},
      {"0x80000000", true, 0x80000000},
      {"0xFFFFFFFF", true, 0xffffffff},
      {"-0x7fffffff", false, 0},
      {"-0x80000000", false, 0},
      {"-0x80000001", false, 0},
      {"-0xffffffff", false, 0},
      {"0x100000000", false, 0},
      {"0xabcdef", true, 11259375},
      {"010", true, 8},
      {"-010", false, 0},
      {"+020", true, 16},
      {"07", true, 7},
      {"08", false, 0},
      {" 0", false, 0},
      {"0 ", false, 0},
      {" 0 ", false, 0},
      {" 1", false, 0},
      {"1 ", false, 0},
      {" 1 ", false, 0},
      {"a2", false, 0},
      {"2a", false, 0},
      {"2a2", false, 0},
      {".0", false, 0},
      {".1", false, 0},
      {"-.2", false, 0},
      {"+.3", false, 0},
      {"1.23", false, 0},
      {"-273.15", false, 0},
      {"+98.6", false, 0},
      {"1e1", false, 0},
      {"1E1", false, 0},
      {"0x123p4", false, 0},
      {"infinity", false, 0},
      {"NaN", false, 0},
      {"-9223372036854775810", false, 0},
      {"-9223372036854775809", false, 0},
      {"9223372036854775808", false, 0},
      {"9223372036854775809", false, 0},
      {"18446744073709551615", false, 0},
      {"18446744073709551616", false, 0},
  }};
}

// Signed 64-bit test data
template <typename TIntType,
          typename std::enable_if<std::is_integral<TIntType>::value &&
                                  std::is_signed<TIntType>::value &&
                                  (sizeof(TIntType) == 8)>::type* = nullptr>
static constexpr std::array<TestSpecification<TIntType>, 24> kTestDataFunc() {
  return {{
      {"", false, 0},
      {"0", true, 0},
      {"1", true, 1},
      {"2147483647", true, 2147483647},
      {"2147483648", true, 2147483648},
      {"4294967295", true, 4294967295},
      {"4294967296", true, 4294967296},
      {"9223372036854775807", true, std::numeric_limits<TIntType>::max()},
      {"9223372036854775808", false, 0},
      {"18446744073709551615", false, 0},
      {"18446744073709551616", false, 0},
      {"-1", true, -1},
      {"-2147483648", true, INT64_C(-2147483648)},
      {"-2147483649", true, INT64_C(-2147483649)},
      {"-9223372036854775808", true, std::numeric_limits<TIntType>::min()},
      {"-9223372036854775809", false, 0},
      {"0x7fffffffffffffff", true, std::numeric_limits<TIntType>::max()},
      {"0x8000000000000000", false, 0},
      {"0xffffffffffffffff", false, 0},
      {"0x10000000000000000", false, 0},
      {"-0x7fffffffffffffff", true, -9223372036854775807},
      {"-0x8000000000000000", true, std::numeric_limits<TIntType>::min()},
      {"-0x8000000000000001", false, 0},
      {"0x7Fffffffffffffff", true, std::numeric_limits<TIntType>::max()},
  }};
}

// Unsigned 64-bit test data
template <typename TIntType,
          typename std::enable_if<std::is_integral<TIntType>::value &&
                                  !std::is_signed<TIntType>::value &&
                                  (sizeof(TIntType) == 8)>::type* = nullptr>
static constexpr std::array<TestSpecification<TIntType>, 25> kTestDataFunc() {
  return {{
      {"", false, 0},
      {"0", true, 0},
      {"1", true, 1},
      {"2147483647", true, 2147483647},
      {"2147483648", true, 2147483648},
      {"4294967295", true, 4294967295},
      {"4294967296", true, 4294967296},
      {"9223372036854775807", true, 9223372036854775807},
      {"9223372036854775808", true, 9223372036854775808u},
      {"18446744073709551615", true, std::numeric_limits<TIntType>::max()},
      {"18446744073709551616", false, 0},
      {"-1", false, 0},
      {"-2147483648", false, 0},
      {"-2147483649", false, 0},
      {"-2147483648", false, 0},
      {"-9223372036854775808", false, 0},
      {"-9223372036854775809", false, 0},
      {"0x7fffffffffffffff", true, 9223372036854775807},
      {"0x8000000000000000", true, 9223372036854775808u},
      {"0xffffffffffffffff", true, std::numeric_limits<TIntType>::max()},
      {"0x10000000000000000", false, 0},
      {"-0x7fffffffffffffff", false, 0},
      {"-0x8000000000000000", false, 0},
      {"-0x8000000000000001", false, 0},
      {"0xFfffffffffffffff", true, std::numeric_limits<TIntType>::max()},
  }};
}

// This string is split to avoid MSVC warning:
//   "decimal digit terminates octal escape sequence".
static constexpr char kEmbeddedNullInputRaw[] = "6\000" "6";

namespace crashpad {
namespace test {
namespace {

TEST(StringNumberConversion, StringToInt) {
  static_assert(sizeof(int) == 4, "Test only configured for 32-bit int.");
  static constexpr auto kTestData = kTestDataFunc<int>();

  for (size_t index = 0; index < kTestData.size(); ++index) {
    int value;
    bool valid = StringToNumber(kTestData[index].string, &value);
    if (kTestData[index].valid) {
      EXPECT_TRUE(valid) << "index " << index << ", string "
                         << kTestData[index].string;
      if (valid) {
        EXPECT_EQ(value, kTestData[index].value)
            << "index " << index << ", string " << kTestData[index].string;
      }
    } else {
      EXPECT_FALSE(valid) << "index " << index << ", string "
                          << kTestData[index].string << ", value " << value;
    }
  }

  // Ensure that embedded NUL characters are treated as bad input. The string
  // is split to avoid MSVC warning:
  //   "decimal digit terminates octal escape sequence".
  int output;
  std::string kEmbeddedNullInput(kEmbeddedNullInputRaw,
                                 base::size(kEmbeddedNullInputRaw) - 1);
  EXPECT_FALSE(StringToNumber(kEmbeddedNullInput, &output));
}

TEST(StringNumberConversion, StringToUnsignedInt) {
  static_assert(sizeof(unsigned int) == 4,
                "Test only configured for 32-bit unsigned int.");
  static constexpr auto kTestData = kTestDataFunc<unsigned int>();

  for (size_t index = 0; index < kTestData.size(); ++index) {
    unsigned int value;
    bool valid = StringToNumber(kTestData[index].string, &value);
    if (kTestData[index].valid) {
      EXPECT_TRUE(valid) << "index " << index << ", string "
                         << kTestData[index].string;
      if (valid) {
        EXPECT_EQ(value, kTestData[index].value)
            << "index " << index << ", string " << kTestData[index].string;
      }
    } else {
      EXPECT_FALSE(valid) << "index " << index << ", string "
                          << kTestData[index].string << ", value " << value;
    }
  }

  // Ensure that embedded NUL characters are treated as bad input. The string
  // is split to avoid MSVC warning:
  //   "decimal digit terminates octal escape sequence".
  unsigned int output;
  std::string kEmbeddedNullInput(kEmbeddedNullInputRaw,
                                 base::size(kEmbeddedNullInputRaw) - 1);
  EXPECT_FALSE(StringToNumber(kEmbeddedNullInput, &output));
}

TEST(StringNumberConversion, StringToLong) {
  static_assert(
      sizeof(long) == 4 || sizeof(long) == 8,
      "Test not configured for " STRINGIFY(__SIZEOF_LONG__) "-byte long");
  static constexpr auto kTestData = kTestDataFunc<long>();

  for (size_t index = 0; index < kTestData.size(); ++index) {
    long value;
    bool valid = StringToNumber(kTestData[index].string, &value);
    if (kTestData[index].valid) {
      EXPECT_TRUE(valid) << "index " << index << ", string "
                         << kTestData[index].string;
      if (valid) {
        EXPECT_EQ(value, kTestData[index].value)
            << "index " << index << ", string " << kTestData[index].string;
      }
    } else {
      EXPECT_FALSE(valid) << "index " << index << ", string "
                          << kTestData[index].string << ", value " << value;
    }
  }
}

TEST(StringNumberConversion, StringToUnsignedLong) {
  static_assert(
      sizeof(long) == 4 || sizeof(long) == 8,
      "Test not configured for " STRINGIFY(__SIZEOF_LONG__) "-byte long");
  static constexpr auto kTestData = kTestDataFunc<unsigned long>();

  for (size_t index = 0; index < kTestData.size(); ++index) {
    unsigned long value;
    bool valid = StringToNumber(kTestData[index].string, &value);
    if (kTestData[index].valid) {
      EXPECT_TRUE(valid) << "index " << index << ", string "
                         << kTestData[index].string;
      if (valid) {
        EXPECT_EQ(value, kTestData[index].value)
            << "index " << index << ", string " << kTestData[index].string;
      }
    } else {
      EXPECT_FALSE(valid) << "index " << index << ", string "
                          << kTestData[index].string << ", value " << value;
    }
  }
}

TEST(StringNumberConversion, StringToLongLong) {
  static_assert(sizeof(long long) == 8,
                "Test only configured for 64-bit long long.");
  static constexpr auto kTestData = kTestDataFunc<long long>();

  for (size_t index = 0; index < kTestData.size(); ++index) {
    long long value;
    bool valid = StringToNumber(kTestData[index].string, &value);
    if (kTestData[index].valid) {
      EXPECT_TRUE(valid) << "index " << index << ", string "
                         << kTestData[index].string;
      if (valid) {
        EXPECT_EQ(value, kTestData[index].value)
            << "index " << index << ", string " << kTestData[index].string;
      }
    } else {
      EXPECT_FALSE(valid) << "index " << index << ", string "
                          << kTestData[index].string << ", value " << value;
    }
  }
}

TEST(StringNumberConversion, StringToUnsignedLongLong) {
  static_assert(sizeof(unsigned long long) == 8,
                "Test only configured for 64-bit unsigned long long.");
  static constexpr auto kTestData = kTestDataFunc<unsigned long long>();

  for (size_t index = 0; index < kTestData.size(); ++index) {
    unsigned long long value;
    bool valid = StringToNumber(kTestData[index].string, &value);
    if (kTestData[index].valid) {
      EXPECT_TRUE(valid) << "index " << index << ", string "
                         << kTestData[index].string;
      if (valid) {
        EXPECT_EQ(value, kTestData[index].value)
            << "index " << index << ", string " << kTestData[index].string;
      }
    } else {
      EXPECT_FALSE(valid) << "index " << index << ", string "
                          << kTestData[index].string << ", value " << value;
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
