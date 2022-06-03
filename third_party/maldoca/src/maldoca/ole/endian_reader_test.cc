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

// Unit-tests for the EndianReader class.

#include "maldoca/ole/endian_reader.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "maldoca/base/endian.h"

namespace {
using ::maldoca::BigEndianReader;
using ::maldoca::LittleEndianReader;

const int kMaxStrParseLength = 1024;

// Make sure we properly handle an input that's shorter than expected.
TEST(EndianReaderTest, InputTooShort) {
  uint8_t one_byte;
  uint16_t two_bytes;
  uint32_t four_bytes;
  uint64_t eight_bytes;

  absl::string_view empty = absl::string_view("");
  absl::string_view almost_empty = absl::string_view(".");
  EXPECT_FALSE(LittleEndianReader::ConsumeUInt8(&empty, &one_byte));
  EXPECT_FALSE(LittleEndianReader::ConsumeUInt16(&almost_empty, &two_bytes));
  EXPECT_FALSE(LittleEndianReader::ConsumeUInt32(&almost_empty, &four_bytes));
  EXPECT_FALSE(LittleEndianReader::ConsumeUInt64(&almost_empty, &eight_bytes));

  EXPECT_FALSE(BigEndianReader::ConsumeUInt8(&empty, &one_byte));
  EXPECT_FALSE(BigEndianReader::ConsumeUInt16(&almost_empty, &two_bytes));
  EXPECT_FALSE(BigEndianReader::ConsumeUInt32(&almost_empty, &four_bytes));
  EXPECT_FALSE(BigEndianReader::ConsumeUInt64(&almost_empty, &eight_bytes));
}

// Consuming values from a StringPiece and interpreting them according
// to the little endian byte ordering.
TEST(EndianReaderTest, ReadLittleEndian) {
  uint8_t one_byte;
  absl::string_view piece_one_byte = absl::string_view("\x0a");
  EXPECT_TRUE(LittleEndianReader::ConsumeUInt8(&piece_one_byte, &one_byte));
  EXPECT_EQ(one_byte, 0xa);
  EXPECT_EQ(piece_one_byte.size(), 0);

  uint16_t two_bytes;
  absl::string_view piece_two_bytes = absl::string_view("\x0b\x0a");
  EXPECT_TRUE(LittleEndianReader::ConsumeUInt16(&piece_two_bytes, &two_bytes));
  EXPECT_EQ(two_bytes, (1 << 8) * 0x0a + 0x0b);
  EXPECT_EQ(piece_two_bytes.size(), 0);

  uint32_t four_bytes;
  absl::string_view piece_four_bytes = absl::string_view("\x0d\x0c\x0b\x0a");
  EXPECT_TRUE(
      LittleEndianReader::ConsumeUInt32(&piece_four_bytes, &four_bytes));
  EXPECT_EQ(four_bytes,
            (1 << 24) * 0x0a + (1 << 16) * 0x0b + (1 << 8) * 0x0c + 0x0d);
  EXPECT_EQ(piece_four_bytes.size(), 0);

  uint64_t eight_bytes;
  absl::string_view piece_eight_bytes =
      absl::string_view("\x04\x03\x02\x01\x0d\x0c\x0b\x0a");
  EXPECT_TRUE(
      LittleEndianReader::ConsumeUInt64(&piece_eight_bytes, &eight_bytes));
  EXPECT_EQ(eight_bytes,
            ((0x0aLL << 56) + (0x0bLL << 48) + (0x0cLL << 40) + (0x0dLL << 32) +
             (0x01 << 24) + (0x02 << 16) + (0x03 << 8) + 0x04));
  EXPECT_EQ(piece_eight_bytes.size(), 0);

  piece_eight_bytes = absl::string_view("\x04\x03\x02\x01\x0d\x0c\x0b\x0a");
  uint16_t two_bytes_2;
  EXPECT_TRUE(
      LittleEndianReader::ConsumeUInt16(&piece_eight_bytes, &two_bytes));
  EXPECT_TRUE(
      LittleEndianReader::ConsumeUInt16(&piece_eight_bytes, &two_bytes_2));
  EXPECT_TRUE(
      LittleEndianReader::ConsumeUInt32(&piece_eight_bytes, &four_bytes));

  EXPECT_EQ(two_bytes, (0x03 << 8) + 0x04);
  EXPECT_EQ(two_bytes_2, (0x01 << 8) + 0x02);
  EXPECT_EQ(four_bytes, (0x0a << 24) + (0x0b << 16) + (0x0c << 8) + 0x0d);
  EXPECT_EQ(piece_eight_bytes.size(), 0);
}

// Consuming values from a StringPiece and interpreting them according
// to the big endian byte ordering.
TEST(EndianReaderTest, ReadBigEndian) {
  uint16_t two_bytes;
  absl::string_view piece_two_bytes = absl::string_view("\x0b\x0a");
  EXPECT_TRUE(BigEndianReader::ConsumeUInt16(&piece_two_bytes, &two_bytes));
  EXPECT_EQ(two_bytes, (1 << 8) * 0x0b + 0x0a);
  EXPECT_EQ(piece_two_bytes.size(), 0);

  uint32_t four_bytes;
  absl::string_view piece_four_bytes = absl::string_view("\x0d\x0c\x0b\x0a");
  EXPECT_TRUE(BigEndianReader::ConsumeUInt32(&piece_four_bytes, &four_bytes));
  EXPECT_EQ(four_bytes, (0x0d << 24) + (0x0c << 16) + (0x0b << 8) + 0x0a);
  EXPECT_EQ(piece_four_bytes.size(), 0);

  uint64_t eight_bytes;
  absl::string_view piece_eight_bytes =
      absl::string_view("\x04\x03\x02\x01\x0d\x0c\x0b\x0a");
  EXPECT_TRUE(BigEndianReader::ConsumeUInt64(&piece_eight_bytes, &eight_bytes));
  EXPECT_EQ(eight_bytes,
            ((0x04LL << 56) + (0x03LL << 48) + (0x02LL << 40) + (0x01LL << 32) +
             (0x0d << 24) + (0x0c << 16) + (0x0b << 8) + 0x0a));
  EXPECT_EQ(piece_eight_bytes.size(), 0);

  piece_eight_bytes = absl::string_view("\x04\x03\x02\x01\x0d\x0c\x0b\x0a");
  uint16_t two_bytes_2;
  EXPECT_TRUE(BigEndianReader::ConsumeUInt16(&piece_eight_bytes, &two_bytes));
  EXPECT_TRUE(BigEndianReader::ConsumeUInt16(&piece_eight_bytes, &two_bytes_2));
  EXPECT_TRUE(BigEndianReader::ConsumeUInt32(&piece_eight_bytes, &four_bytes));

  EXPECT_EQ(two_bytes, (0x04 << 8) + 0x03);
  EXPECT_EQ(two_bytes_2, (0x02 << 8) + 0x01);
  EXPECT_EQ(four_bytes, (0x0d << 24) + (0x0c << 16) + (0x0b << 8) + 0x0a);
  EXPECT_EQ(piece_eight_bytes.size(), 0);
}

// Consuming strings from a StringPiece input.
TEST(EndianReaderTest, ReadString) {
  absl::string_view input =
      absl::string_view("The quick brown fox jumps over the lazy dog");
  std::string read_string;
  // Can't read more than there is.
  EXPECT_FALSE(LittleEndianReader::ConsumeString(&input, input.size() + 1,
                                                 &read_string));
  EXPECT_TRUE(LittleEndianReader::ConsumeString(&input, 9, &read_string));
  EXPECT_EQ(read_string, "The quick");
  EXPECT_EQ(input.size(), 34);

  input = absl::string_view("The quick brown fox jumps over the lazy dog");
  EXPECT_FALSE(
      BigEndianReader::ConsumeString(&input, input.size() + 1, &read_string));
  EXPECT_TRUE(
      BigEndianReader::ConsumeString(&input, input.size(), &read_string));
  EXPECT_EQ(read_string, "The quick brown fox jumps over the lazy dog");
  EXPECT_EQ(input.size(), 0);
}

// Consuming null terminated strings from a StringPiece input.
TEST(EndianReaderTest, ReadNullTerminatedString) {
  char input_chars[] = "The quick brown fox\x00 jumps over the lazy dog";
  absl::string_view input(input_chars, sizeof(input_chars) - 1);
  std::string read_string;
  EXPECT_TRUE(
      LittleEndianReader::ConsumeNullTerminatedString(&input, &read_string));
  EXPECT_EQ(read_string, "The quick brown fox");
  EXPECT_EQ(input.size(), 24);

  input = absl::string_view(input_chars, sizeof(input_chars) - 1);
  EXPECT_TRUE(
      BigEndianReader::ConsumeNullTerminatedString(&input, &read_string));
  EXPECT_EQ(read_string, "The quick brown fox");
  EXPECT_EQ(input.size(), 24);

  // no nulls in string
  input = absl::string_view(read_string);
  EXPECT_FALSE(
      LittleEndianReader::ConsumeNullTerminatedString(&input, &read_string));

  std::string large_input(kMaxStrParseLength + 2, 'a');
  large_input[kMaxStrParseLength + 1] = '\0';
  input = large_input;
  EXPECT_FALSE(
      LittleEndianReader::ConsumeNullTerminatedString(&input, &read_string));
}

// Read (but do not consume) from a StringPiece at a given position
// and interpret the retrieved value according to a given endianess.
TEST(EndianReaderTest, ReadAtTest) {
  absl::string_view input =
      absl::string_view("\x01\x02\x03\x04\x05\x06\x07\x08");

  uint8_t byte = 0;
  // Can't read past the input.
  EXPECT_FALSE(LittleEndianReader::LoadUInt8At(input, 8, &byte));
  EXPECT_EQ(byte, 0);
  EXPECT_TRUE(LittleEndianReader::LoadUInt8At(input, 1, &byte));
  EXPECT_EQ(byte, 0x2);

  uint16_t two_bytes = 0;
  // Can't read past the input.
  EXPECT_FALSE(LittleEndianReader::LoadUInt16At(input, 7, &two_bytes));
  EXPECT_EQ(two_bytes, 0);
  // Unaligned reads are supported.
  EXPECT_TRUE(LittleEndianReader::LoadUInt16At(input, 1, &two_bytes));
  EXPECT_EQ(two_bytes, (0x03 << 8) + 0x02);

  uint32_t four_bytes = 0;
  // Can't read past the input.
  EXPECT_FALSE(LittleEndianReader::LoadUInt32At(input, 6, &four_bytes));
  EXPECT_EQ(four_bytes, 0);
  // Unaligned reads are supported.
  EXPECT_TRUE(LittleEndianReader::LoadUInt32At(input, 1, &four_bytes));
  EXPECT_EQ(four_bytes, (0x05 << 24) + (0x04 << 16) + (0x03 << 8) + 0x02);

  uint64_t eight_bytes = 0;
  // Can't read past the input.
  EXPECT_FALSE(LittleEndianReader::LoadUInt64At(input, 2, &eight_bytes));
  EXPECT_EQ(eight_bytes, 0);
  EXPECT_TRUE(LittleEndianReader::LoadUInt64At(input, 0, &eight_bytes));
  EXPECT_EQ(eight_bytes,
            ((0x08LL << 56) + (0x07LL << 48) + (0x06LL << 40) + (0x05LL << 32) +
             (0x04 << 24) + (0x03 << 16) + (0x02 << 8) + 0x01));

  byte = 0;
  // Can't read past the input.
  EXPECT_FALSE(BigEndianReader::LoadUInt8At(input, 8, &byte));
  EXPECT_EQ(byte, 0);
  EXPECT_TRUE(BigEndianReader::LoadUInt8At(input, 1, &byte));
  EXPECT_EQ(byte, 0x2);

  two_bytes = 0;
  // Can't read past the input.
  EXPECT_FALSE(BigEndianReader::LoadUInt16At(input, 7, &two_bytes));
  EXPECT_EQ(two_bytes, 0);
  // Unaligned reads are supported.
  EXPECT_TRUE(BigEndianReader::LoadUInt16At(input, 1, &two_bytes));
  EXPECT_EQ(two_bytes, (0x02 << 8) + 0x03);

  four_bytes = 0;
  // Can't read past the input.
  EXPECT_FALSE(BigEndianReader::LoadUInt32At(input, 6, &four_bytes));
  EXPECT_EQ(four_bytes, 0);
  // Unaligned reads are supported.
  EXPECT_TRUE(BigEndianReader::LoadUInt32At(input, 1, &four_bytes));
  EXPECT_EQ(four_bytes, (0x02 << 24) + (0x03 << 16) + (0x04 << 8) + 0x05);

  eight_bytes = 0;
  // Can't read past the input.
  EXPECT_FALSE(BigEndianReader::LoadUInt64At(input, 2, &eight_bytes));
  EXPECT_EQ(eight_bytes, 0);
  EXPECT_TRUE(BigEndianReader::LoadUInt64At(input, 0, &eight_bytes));
  EXPECT_EQ(eight_bytes,
            ((0x01LL << 56) + (0x02LL << 48) + (0x03LL << 40) + (0x04LL << 32) +
             (0x05 << 24) + (0x06 << 16) + (0x07 << 8) + 0x08));
}

TEST(EndianReaderTest, LittleEndianWrapper) {
  absl::string_view input =
      absl::string_view("\x01\x02\x03\x04\x05\x06\x07\x08");
  const auto *ptr16 =
      reinterpret_cast<const maldoca::LittleEndianUInt16 *>(input.data());
  const auto *ptr32 =
      reinterpret_cast<const maldoca::LittleEndianUInt32 *>(input.data());
  const auto *ptr64 =
      reinterpret_cast<const maldoca::LittleEndianUInt64 *>(input.data());
  const auto *ptrdouble =
      reinterpret_cast<const maldoca::LittleEndianDouble *>(input.data());
  EXPECT_EQ(*ptr16, 0x0201);
  EXPECT_EQ(*ptr32, 0x04030201);
  EXPECT_EQ(*ptr64, 0x0807060504030201);
  EXPECT_EQ(*ptrdouble, 5.447603722011605e-270);
}

void CheckCharReplacement(const char* const input_chars, uint32_t input_size,
                          const std::string &output) {
  absl::string_view input(input_chars, input_size);
  std::string read_string;
  maldoca::DecodeUTF16(input, &read_string);
  EXPECT_EQ(read_string, output);
}

// Check if certain Unicode chars are replaced by space.
TEST(EndianReaderTest, CharReplacement) {
  // Check C0 controls replacement (CR LF HT FF are allowed).
  char chars_c0[] = "\x01\x00\x09\x00\x1c\x00\x7f\x00";
  CheckCharReplacement(chars_c0, sizeof(chars_c0), " \t  ");

  // ASCII is allowed.
  char chars_ascii[] = "a\x00";
  CheckCharReplacement(chars_ascii, sizeof(chars_ascii), "a");

  // Check C1 controls replacement.
  char chars_c1[] = "\x7f\x00\x9f\x00";
  CheckCharReplacement(chars_c1, sizeof(chars_c1), "  ");

  // Check Unicode's private use area replacement.
  char chars_priv[] = "\x00\xe0";
  CheckCharReplacement(chars_priv, sizeof(chars_priv), " ");

  // Check random Unicode char is allowed.
  char chars_uni[] = "\x11\x11";
  absl::string_view input(chars_uni, sizeof(chars_uni));
  std::string read_string;
  maldoca::DecodeUTF16(input, &read_string);
  EXPECT_NE(read_string, " ");
}
}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  return RUN_ALL_TESTS();
}
