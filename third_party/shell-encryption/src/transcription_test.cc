// Copyright 2020 Google LLC
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

#include "transcription.h"

#include <cstdint>
#include <random>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/numeric/int128.h"
#include "integral_types.h"
#include "status_macros.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"

namespace rlwe {
namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

template <typename Int>
class TranscribeTest : public ::testing::Test {};

const int kLength = 10;

using MyTypes = ::testing::Types<Uint8, Uint16, Uint32, absl::uint128>;
TYPED_TEST_SUITE(TranscribeTest, MyTypes);

// Generate a random integer of a specified number of bits.
template <class TypeParam>
TypeParam generate_random(int number_bits, unsigned int* seed) {
  if (number_bits == 0) return 0;
  TypeParam random_value = static_cast<TypeParam>(rand_r(seed));
  if (number_bits >= 8 * sizeof(TypeParam)) {
    return random_value;
  } else {
    TypeParam mask = (static_cast<TypeParam>(1) << number_bits) - 1;
    return random_value & mask;
  }
}
// Specialization for uint128.
template <>
absl::uint128 generate_random(int number_bits, unsigned int* seed) {
  int number_bits_hi = number_bits - std::min(64, number_bits);
  int number_bits_lo = number_bits % 64;
  uint64_t hi = generate_random<uint64_t>(number_bits_hi, seed);
  uint64_t lo = generate_random<uint64_t>(number_bits_lo, seed);
  return absl::MakeUint128(hi, lo);
}

// Verifies that the input_vector is long enough.
TYPED_TEST(TranscribeTest, InputLongEnough) {
  using InputInt = TypeParam;
  using OutputInt = TypeParam;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  for (auto input_bit_length : {1, 100, 1000, 10000}) {
    for (int i = 1; i < input_number_of_bits; i++) {
      int necessary_number_of_chunks = (input_bit_length + i - 1) / i;
      std::vector<InputInt> input(necessary_number_of_chunks - 1, 0);
      for (int j = 1; j < output_number_of_bits; j++) {
        EXPECT_THAT(
            (TranscribeBits<InputInt, OutputInt>(input, input_bit_length, i,
                                                 j)),
            StatusIs(::absl::StatusCode::kInvalidArgument,
                     HasSubstr(absl::StrCat("The input vector of size ",
                                            (necessary_number_of_chunks - 1),
                                            " is too small to contain ",
                                            input_bit_length, " bits."))));
      }
    }
  }
}

// Verifies that the input and output types are consistent.
TYPED_TEST(TranscribeTest, InconsistentInputType) {
  using InputInt = TypeParam;
  using OutputInt = TypeParam;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  // Try to extract too many bits.
  const int input_bits_per_int = input_number_of_bits + 1;

  for (int len = 1; len <= kLength; len++) {
    int input_bit_length = input_bits_per_int * len;
    int necessary_number_of_chunks =
        (input_bit_length + input_number_of_bits - 1) / input_number_of_bits;
    std::vector<InputInt> input(necessary_number_of_chunks, 0);
    for (int j = 1; j <= output_number_of_bits; j++) {
      EXPECT_THAT(
          (TranscribeBits<InputInt, OutputInt>(input, input_bit_length,
                                               input_bits_per_int, j)),
          StatusIs(::absl::StatusCode::kInvalidArgument,
                   HasSubstr(absl::StrCat(
                       "The input type only contains ", input_number_of_bits,
                       " bits, hence we cannot extract ", input_bits_per_int,
                       " bits out of each integer."))));
    }
  }
}

TYPED_TEST(TranscribeTest, InconsistentOutputType) {
  using InputInt = TypeParam;
  using OutputInt = TypeParam;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  // Try to store too many bits.
  const int output_bits_per_int = output_number_of_bits + 1;

  for (int len = 1; len <= kLength; len++) {
    std::vector<InputInt> input(len, 0);
    for (int i = 1; i <= input_number_of_bits; i++) {
      EXPECT_THAT(
          (TranscribeBits<InputInt, OutputInt>(input, i * len, i,
                                               output_bits_per_int)),
          StatusIs(::absl::StatusCode::kInvalidArgument,
                   HasSubstr(absl::StrCat(
                       "The output type only contains ", output_number_of_bits,
                       " bits, hence we cannot save ", output_bits_per_int,
                       " bits in each integer."))));
    }
  }
}

TYPED_TEST(TranscribeTest, NegativeInputLength) {
  using InputInt = TypeParam;
  using OutputInt = TypeParam;
  const int input_bit_length = -1;

  // create a zero string
  std::vector<InputInt> bits_i(kLength, 0);
  EXPECT_THAT(
      (TranscribeBits<InputInt, OutputInt>(bits_i, input_bit_length, 0, 0)),
      StatusIs(
          ::absl::StatusCode::kInvalidArgument,
          HasSubstr(absl::StrCat("The input bit length, ", input_bit_length,
                                 ", cannot be negative."))));
}

TYPED_TEST(TranscribeTest, NonEmptyInputToEmptyOutput) {
  using InputInt = TypeParam;
  using OutputInt = TypeParam;
  const int input_bit_length = 0;

  // Create a zero string
  std::vector<InputInt> bits_i(kLength, 0);
  EXPECT_THAT(
      (TranscribeBits<InputInt, OutputInt>(bits_i, input_bit_length, 1, 1)),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr("Cannot transcribe an empty output "
                         "vector with a non-empty input "
                         "vector.")));
}

// Convert a sequence in chunks of i bits into a sequence in chunks of j
// bits.
TYPED_TEST(TranscribeTest, TranscribeTypeToType) {
  using InputInt = TypeParam;
  using OutputInt = TypeParam;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  unsigned int seed = 0;

  for (int i = 1; i <= input_number_of_bits; i++) {
    for (int j = 1; j <= output_number_of_bits; j++) {
      for (int len = 1; len <= kLength; len++) {
        // Create a random string of len bytes.
        std::vector<InputInt> bits_i(len, 0);
        for (InputInt& byte : bits_i) {
          byte = generate_random<InputInt>(i, &seed);
        }

        // Convert to j bits.
        ASSERT_OK_AND_ASSIGN(
            std::vector<OutputInt> bits_j,
            (TranscribeBits<InputInt, OutputInt>(bits_i, len * i, i, j)));

        // Ensure that bits_j has the right length.
        EXPECT_EQ(bits_j.size(), (len * i + (j - 1)) / j);

        // Ensure that the bits came out the same.
        for (int bit = 0; bit < i * len; bit++) {
          InputInt bit_i = bits_i[bit / i] >> (bit % i);
          OutputInt bit_j = bits_j[bit / j] >> (bit % j);

          EXPECT_EQ(bit_i & 1, bit_j & 1);
        }

        // Ensure that all other bits in bits_j are zeros.
        for (int byte = 0; byte < bits_j.size(); byte++) {
          if (j == output_number_of_bits) continue;  // no remaining bits
          EXPECT_EQ(bits_j[byte] >> j, 0);
        }
      }
    }
  }
}

TYPED_TEST(TranscribeTest, TranscribeTypeToTypeAndBack) {
  using InputInt = TypeParam;
  using OutputInt = TypeParam;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  unsigned int seed = 0;

  for (int i = 1; i <= input_number_of_bits; i++) {
    for (int j = 1; j <= output_number_of_bits; j++) {
      for (int len = 1; len <= kLength; len++) {
        // Create a random string of len bytes.
        std::vector<InputInt> bits_i(len, 0);
        for (InputInt& byte : bits_i) {
          byte = generate_random<InputInt>(i, &seed);
        }

        // Convert to j bits.
        ASSERT_OK_AND_ASSIGN(
            std::vector<OutputInt> bits_j,
            (TranscribeBits<InputInt, OutputInt>(bits_i, len * i, i, j)));

        // Convert back.
        ASSERT_OK_AND_ASSIGN(
            std::vector<InputInt> bits_i2,
            (TranscribeBits<InputInt, OutputInt>(bits_j, len * i, j, i)));

        EXPECT_EQ(bits_i, bits_i2);
      }
    }
  }
}

// Test when the input bit length is not a multiple of the number of bits per
// int.
TYPED_TEST(TranscribeTest, InputBitLengthNotMultipleBitsPerInt) {
  using InputInt = TypeParam;
  using OutputInt = TypeParam;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  unsigned int seed = 0;

  // Reduce the number of elements to speed up the test.
  for (auto i : {2, 7, input_number_of_bits / 2, input_number_of_bits / 2 + 1,
                 input_number_of_bits}) {
    for (int j = 1; j <= output_number_of_bits; j++) {
      // Reduce the number of elements to speed up the test.
      for (int len = 1; len <= kLength / 4 + 1; len++) {
        for (int input_bit_length = len * i + 1;
             input_bit_length < (len + 1) * i; input_bit_length++) {
          int necessary_number_of_chunks = (input_bit_length + i - 1) / i;
          // Create a random string of necessary_number_of_chunks bytes.
          std::vector<InputInt> bits_i(necessary_number_of_chunks, 0);
          for (InputInt& byte : bits_i) {
            byte = generate_random<InputInt>(i, &seed);
          }
          // Convert to j bits.
          ASSERT_OK_AND_ASSIGN(std::vector<OutputInt> bits_j,
                               (TranscribeBits<InputInt, OutputInt>(
                                   bits_i, input_bit_length, i, j)));
          // Ensure that the bits came out the same.
          for (int bit = 0; bit < input_bit_length; bit++) {
            InputInt bit_i = bits_i[bit / i] >> (bit % i);
            OutputInt bit_j = bits_j[bit / j] >> (bit % j);

            EXPECT_EQ(bit_i & 1, bit_j & 1);
          }
          // Ensure that all other bits in bits_j are zeros.
          for (int byte = 0; byte < bits_j.size(); byte++) {
            if (j == output_number_of_bits) continue;  // no remaining bits
            EXPECT_EQ(bits_j[byte] >> j, 0);
          }
          // The last element will only have input_bit_length % j bits sets.
          // Check that all the other bits are 0. The test is only meaningful
          // when input_bit_length % j != 0.
          if (input_bit_length % j != 0) {
            EXPECT_EQ(bits_j[bits_j.size() - 1] >> (input_bit_length % j), 0);
          }
        }
      }
    }
  }
}

TYPED_TEST(TranscribeTest, TranscribeTypeToUint64) {
  using InputInt = TypeParam;
  using OutputInt = uint64_t;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  unsigned int seed = 0;

  for (int i = 1; i <= input_number_of_bits; i++) {
    for (int j = 1; j <= output_number_of_bits; j++) {
      for (int len = 1; len <= kLength; len++) {
        // Create a random string of len bytes.
        std::vector<InputInt> bits_i(len, 0);
        for (InputInt& byte : bits_i) {
          byte = generate_random<InputInt>(i, &seed);
        }

        // Convert to j bits.
        ASSERT_OK_AND_ASSIGN(
            std::vector<OutputInt> bits_j,
            (TranscribeBits<InputInt, OutputInt>(bits_i, len * i, i, j)));

        // Ensure that bits_j has the right length.
        EXPECT_EQ(bits_j.size(), (len * i + (j - 1)) / j);

        // Ensure that the bits came out the same.
        for (int bit = 0; bit < i * len; bit++) {
          Uint8 bit_i = static_cast<Uint8>(bits_i[bit / i] >> (bit % i));
          Uint8 bit_j = static_cast<Uint8>(bits_j[bit / j] >> (bit % j));

          EXPECT_EQ(bit_i & 0x01, bit_j & 0x01);
        }

        // Ensure that all other bits in bits_j are zeros.
        for (int byte = 0; byte < bits_j.size(); byte++) {
          if (j == output_number_of_bits) continue;  // no remaining bits
          EXPECT_EQ(bits_j[byte] >> j, 0);
        }
      }
    }
  }
}

TYPED_TEST(TranscribeTest, TranscribeTypeToUint128) {
  using InputInt = TypeParam;
  using OutputInt = absl::uint128;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  unsigned int seed = 0;

  for (int i = 1; i <= input_number_of_bits; i++) {
    // Reduce the number of elements to speed up the test.
    for (auto j : {2, 7, output_number_of_bits / 2,
                   output_number_of_bits / 2 + 1, output_number_of_bits}) {
      for (int len = 1; len <= kLength; len++) {
        // Create a random string of len bytes.
        std::vector<InputInt> bits_i(len, 0);
        for (InputInt& byte : bits_i) {
          byte = generate_random<InputInt>(i, &seed);
        }

        // Convert to j bits.
        ASSERT_OK_AND_ASSIGN(
            std::vector<OutputInt> bits_j,
            (TranscribeBits<InputInt, OutputInt>(bits_i, len * i, i, j)));

        // Ensure that bits_j has the right length.
        EXPECT_EQ(bits_j.size(), (len * i + (j - 1)) / j);

        // Ensure that the bits came out the same.
        for (int bit = 0; bit < i * len; bit++) {
          Uint8 bit_i = static_cast<Uint8>(bits_i[bit / i] >> (bit % i));
          Uint8 bit_j = static_cast<Uint8>(bits_j[bit / j] >> (bit % j));

          EXPECT_EQ(bit_i & 0x01, bit_j & 0x01);
        }

        // Ensure that all other bits in bits_j are zeros.
        for (int byte = 0; byte < bits_j.size(); byte++) {
          if (j == output_number_of_bits) continue;  // no remaining bits
          EXPECT_EQ(bits_j[byte] >> j, 0);
        }
      }
    }
  }
}

TYPED_TEST(TranscribeTest, TranscribeTypeToUint8) {
  using InputInt = TypeParam;
  using OutputInt = Uint8;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  unsigned int seed = 0;

  for (int i = 1; i <= input_number_of_bits; i++) {
    for (int j = 1; j <= output_number_of_bits; j++) {
      for (int len = 1; len <= kLength; len++) {
        // Create a random string of len bytes.
        std::vector<InputInt> bits_i(len, 0);
        for (InputInt& byte : bits_i) {
          byte = generate_random<InputInt>(i, &seed);
        }

        // Convert to j bits.
        ASSERT_OK_AND_ASSIGN(
            std::vector<OutputInt> bits_j,
            (TranscribeBits<InputInt, OutputInt>(bits_i, len * i, i, j)));

        // Ensure that bits_j has the right length.
        EXPECT_EQ(bits_j.size(), (len * i + (j - 1)) / j);

        // Ensure that the bits came out the same.
        for (int bit = 0; bit < i * len; bit++) {
          Uint8 bit_i = static_cast<Uint8>(bits_i[bit / i] >> (bit % i));
          Uint8 bit_j = static_cast<Uint8>(bits_j[bit / j] >> (bit % j));

          EXPECT_EQ(bit_i & 0x01, bit_j & 0x01);
        }

        // Ensure that all other bits in bits_j are zeros.
        for (int byte = 0; byte < bits_j.size(); byte++) {
          if (j == output_number_of_bits) continue;  // no remaining bits
          EXPECT_EQ(bits_j[byte] >> j, 0);
        }
      }
    }
  }
}

TYPED_TEST(TranscribeTest, InputLengthSmallerThanNumberOfBitsPerInput) {
  using InputInt = TypeParam;
  using OutputInt = TypeParam;
  const int input_number_of_bits = 8 * sizeof(InputInt);
  const int output_number_of_bits = 8 * sizeof(OutputInt);

  unsigned int seed = 0;

  for (int input_bit_length = 1; input_bit_length < input_number_of_bits;
       input_bit_length++) {
    // Create a random string of 1 byte.
    std::vector<InputInt> bits_i(
        {generate_random<InputInt>(input_bit_length, &seed)});
    for (int j = 1; j <= output_number_of_bits; j++) {
      // Convert to j bits.
      ASSERT_OK_AND_ASSIGN(
          std::vector<OutputInt> bits_j,
          (TranscribeBits<InputInt, OutputInt>(bits_i, input_bit_length,
                                               input_number_of_bits, j)));
      // Ensure that bits_j has the right length.
      EXPECT_EQ(bits_j.size(), (input_bit_length + (j - 1)) / j);

      // Ensure that the bits came out the same.
      for (int bit = 0; bit < input_bit_length; bit++) {
        Uint8 bit_i = static_cast<Uint8>(bits_i[0] >> bit);
        Uint8 bit_j = static_cast<Uint8>(bits_j[bit / j] >> (bit % j));

        EXPECT_EQ(bit_i & 0x01, bit_j & 0x01);
      }

      // Ensure that all other bits in bits_j are zeros.
      for (int byte = 0; byte < bits_j.size(); byte++) {
        if (j == output_number_of_bits) continue;  // no remaining bits
        EXPECT_EQ(bits_j[byte] >> j, 0);
      }
      // The last element will only have input_bit_length % j bits sets.
      // Check that all the other bits are 0. The test is only meaningful
      // when input_bit_length % j != 0.
      if (input_bit_length % j != 0) {
        EXPECT_EQ(bits_j[bits_j.size() - 1] >> (input_bit_length % j), 0);
      }
    }
  }
}  // namespace

}  // namespace
}  // namespace rlwe
