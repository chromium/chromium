// Copyright 2023 The Crashpad Authors
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

#include "client/length_delimited_ring_buffer.h"

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

using testing::Eq;
using testing::IsFalse;
using testing::IsTrue;

// Buffer with magic 0xcab00d1e, version 1, read_pos 0, length 3, and 3 bytes of
// data (1 varint length, 2 bytes data)
constexpr char kValidBufferSize3[] =
    "\x1e\x0d\xb0\xca\x01\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x02\x42"
    "\x23";
constexpr size_t kValidBufferSize3Len =
    sizeof(kValidBufferSize3) - 1;  // Remove trailing NUL.

// Buffer with magic 0xcab00d1e, version 8, read_pos 0, length 3, and 3 bytes of
// data (1 varint length, 2 bytes data).
constexpr char kInvalidVersionBuffer[] =
    "\x1e\x0d\xb0\xca\x08\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x02\xab"
    "\xcd";
constexpr size_t kInvalidVersionBufferLen =
    sizeof(kInvalidVersionBuffer) - 1;  // Remove trailing NUL.

// Buffer representing process which crashed while in the middle of a Push()
// operation, with a previously-Push()ed buffer whose length was zeroed out at
// the start.
constexpr char kMidCrashBuffer[] =
    "\x1e\x0d\xb0\xca\x01\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x42"
    "\x23";
constexpr size_t kMidCrashBufferLen =
    sizeof(kMidCrashBuffer) - 1;  // Remove trailing NUL.

constexpr uint8_t kHello[] = {0x68, 0x65, 0x6c, 0x6c, 0x6f};

// Invalid buffer containing malformed varint in data payload (Base 128 varint
// with length 6, which would represent a data length > 32 bits).
constexpr char kInvalidBase128VarintBuffer[] =
    "\x1e\x0d\xb0\xca\x01\x00\x00\x00\x00\x00\x00\x00\x06\x00\x00\x00\x80\x80"
    "\x80\x80\x80\x01";
constexpr size_t kInvalidBase128VarintBufferLen =
    sizeof(kInvalidBase128VarintBuffer) - 1;  // Remove trailing NUL.

// Invalid buffer containing malformed varint in data payload (Base 128 varint
// with length 5 but bits 33 and 34 set, which would represent a data length >
// 32 bits).
constexpr char kInvalidBase128VarintBits33And34SetBuffer[] =
    "\x1e\x0d\xb0\xca\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x80\x80"
    "\x80\x80\x60";
constexpr size_t kInvalidBase128VarintBits33And34SetBufferLen =
    sizeof(kInvalidBase128VarintBits33And34SetBuffer) -
    1;  // Remove trailing NUL.

// Invalid buffer containing too-short data payload (varint length indicates
// payload length is 4 but payload only contains 3 bytes).
constexpr char kInvalidPayloadBufferTooShort[] =
    "\x1e\x0d\xb0\xca\x01\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x04"
    "\x42\x42\x42";
constexpr size_t kInvalidPayloadBufferTooShortLen =
    sizeof(kInvalidPayloadBufferTooShort) - 1;  // Remove trailing NUL.

TEST(LengthDelimitedRingBufferTest,
     RingBufferDataShouldStartWithMagicAndVersion) {
  RingBufferData ring_buffer;
  const void* ring_buffer_bytes = static_cast<const void*>(&ring_buffer);
  EXPECT_THAT(memcmp(ring_buffer_bytes, "\x1e\x0d\xb0\xca\x01\x00\x00\x00", 8),
              Eq(0));
}

TEST(LengthDelimitedRingBufferTest,
     EmptyBufferSizeShouldIncludeHeaderInRingBufferLength) {
  RingBufferData ring_buffer;
  EXPECT_THAT(ring_buffer.GetRingBufferLength(),
              Eq(16U));  // 4 for uint32 magic, 4 for uint32 version, 4 for
                         // uint32 read_pos, 4 for uint32 length
}

TEST(LengthDelimitedRingBufferTest,
     NonEmptyBufferSizeShouldIncludeHeaderAndData) {
  RingBufferData ring_buffer;
  LengthDelimitedRingBufferWriter writer(ring_buffer);
  ASSERT_THAT(writer.Push(kHello, sizeof(kHello)), IsTrue());
  EXPECT_THAT(ring_buffer.GetRingBufferLength(),
              Eq(22U));  // 16 for header, 1 for varint length, 5 for data
}

TEST(LengthDelimitedRingBufferTest, PopOnEmptyBufferShouldFail) {
  RingBufferData ring_buffer;
  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> result;
  EXPECT_THAT(reader.Pop(result), IsFalse());
}

TEST(LengthDelimitedRingBufferTest, PushZeroLengthShouldFail) {
  RingBufferData ring_buffer;
  LengthDelimitedRingBufferWriter writer(ring_buffer);
  ASSERT_THAT(writer.Push(nullptr, 0), IsFalse());
}

TEST(LengthDelimitedRingBufferTest, PushExactlyBufferSizeThenPopShouldSucceed) {
  RingBufferData ring_buffer;
  LengthDelimitedRingBufferWriter writer(ring_buffer);
  ASSERT_THAT(writer.Push(kHello, sizeof(kHello)), IsTrue());

  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> result;
  EXPECT_THAT(reader.Pop(result), IsTrue());
  const std::vector<uint8_t> expected_first = {0x68, 0x65, 0x6c, 0x6c, 0x6f};
  EXPECT_THAT(result, Eq(expected_first));
}

TEST(LengthDelimitedRingBufferTest, PushLargerThanBufferSizeShouldFail) {
  RingBufferData<4> ring_buffer;
  LengthDelimitedRingBufferWriter writer(ring_buffer);
  EXPECT_THAT(writer.Push(kHello, sizeof(kHello)), IsFalse());
}

TEST(LengthDelimitedRingBufferTest,
     PushUntilFullThenPopUntilEmptyShouldReturnInFIFOOrder) {
  RingBufferData<4> ring_buffer;
  LengthDelimitedRingBufferWriter writer(ring_buffer);
  constexpr uint8_t a = 0x41;
  EXPECT_THAT(writer.Push(&a, sizeof(a)),
              IsTrue());  // Writes 2 bytes (1 for length)
  constexpr uint8_t b = 0x42;
  EXPECT_THAT(writer.Push(&b, sizeof(b)),
              IsTrue());  // Writes 2 bytes (1 for length)

  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> first;
  EXPECT_THAT(reader.Pop(first), IsTrue());
  const std::vector<uint8_t> expected_first = {0x41};
  EXPECT_THAT(first, Eq(expected_first));

  std::vector<uint8_t> second;
  EXPECT_THAT(reader.Pop(second), IsTrue());
  const std::vector<uint8_t> expected_second = {0x42};
  EXPECT_THAT(second, Eq(expected_second));

  std::vector<uint8_t> empty;
  EXPECT_THAT(reader.Pop(empty), IsFalse());
}

TEST(LengthDelimitedRingBufferTest,
     PushThenPopBuffersOfDifferingLengthsShouldReturnBuffers) {
  RingBufferData<5> ring_buffer;
  LengthDelimitedRingBufferWriter writer(ring_buffer);
  constexpr uint8_t ab[2] = {0x41, 0x42};
  EXPECT_THAT(writer.Push(ab, sizeof(ab)),
              IsTrue());  // Writes 3 bytes (1 for length)
  constexpr uint8_t c = 0x43;
  EXPECT_THAT(writer.Push(&c, sizeof(c)),
              IsTrue());  // Writes 2 bytes (1 for length)

  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> first;
  EXPECT_THAT(reader.Pop(first), IsTrue());
  const std::vector<uint8_t> expected_first = {0x41, 0x42};
  EXPECT_THAT(first, Eq(expected_first));

  std::vector<uint8_t> second;
  EXPECT_THAT(reader.Pop(second), IsTrue());
  const std::vector<uint8_t> expected_second = {0x43};
  EXPECT_THAT(second, Eq(expected_second));

  std::vector<uint8_t> empty;
  EXPECT_THAT(reader.Pop(empty), IsFalse());
}

TEST(LengthDelimitedRingBufferDataTest, PushOnFullBufferShouldOverwriteOldest) {
  RingBufferData<4> ring_buffer;
  LengthDelimitedRingBufferWriter writer(ring_buffer);
  constexpr uint8_t a = 0x41;
  EXPECT_THAT(writer.Push(&a, sizeof(a)),
              IsTrue());  // Writes 2 bytes (1 for length)
  constexpr uint8_t b = 0x42;
  EXPECT_THAT(writer.Push(&b, sizeof(b)),
              IsTrue());  // Writes 2 bytes (1 for length)
  constexpr uint8_t c = 0x43;
  EXPECT_THAT(writer.Push(&c, sizeof(c)), IsTrue());  // Should overwrite "A"

  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> first;
  EXPECT_THAT(reader.Pop(first), IsTrue());
  const std::vector<uint8_t> expected_first = {uint8_t{0x42}};
  EXPECT_THAT(first, Eq(expected_first));

  std::vector<uint8_t> second;
  EXPECT_THAT(reader.Pop(second), IsTrue());
  const std::vector<uint8_t> expected_second = {uint8_t{0x43}};
  EXPECT_THAT(second, Eq(expected_second));
}

TEST(LengthDelimitedRingBufferDataTest,
     PushOnFullBufferShouldOverwriteMultipleOldest) {
  RingBufferData<4> ring_buffer;
  LengthDelimitedRingBufferWriter writer(ring_buffer);
  constexpr uint8_t a = 0x41;
  EXPECT_THAT(writer.Push(&a, sizeof(a)),
              IsTrue());  // Writes 2 bytes (1 for length)
  constexpr uint8_t b = 0x42;
  EXPECT_THAT(writer.Push(&b, sizeof(b)),
              IsTrue());  // Writes 2 bytes (1 for length)
  constexpr uint8_t cd[] = {0x43, 0x44};
  EXPECT_THAT(writer.Push(cd, sizeof(cd)),
              IsTrue());  // Needs 3 bytes; should overwrite "A" and "B"

  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> first;
  EXPECT_THAT(reader.Pop(first), IsTrue());
  const std::vector<uint8_t> expected_first = {0x43, 0x44};
  EXPECT_THAT(first, Eq(expected_first));

  std::vector<uint8_t> empty;
  EXPECT_THAT(reader.Pop(empty), IsFalse());
}

TEST(LengthDelimitedRingBufferDataTest, PushThenPopWithLengthVarintTwoBytes) {
  RingBufferData ring_buffer;
  decltype(ring_buffer)::SizeType size = 150;
  std::string s(size, 'X');
  LengthDelimitedRingBufferWriter writer(ring_buffer);
  ASSERT_THAT(writer.Push(reinterpret_cast<const uint8_t*>(s.c_str()), size),
              IsTrue());

  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> first;
  EXPECT_THAT(reader.Pop(first), IsTrue());
  std::string result(reinterpret_cast<const char*>(first.data()), first.size());
  EXPECT_THAT(result, Eq(s));
}

TEST(LengthDelimitedRingBufferDataTest, DeserializeFromTooShortShouldFail) {
  RingBufferData<1> ring_buffer;
  EXPECT_THAT(ring_buffer.DeserializeFromBuffer(nullptr, 0), IsFalse());
}

TEST(LengthDelimitedRingBufferDataTest, DeserializeFromTooLongShouldFail) {
  RingBufferData<1> ring_buffer;
  // This buffer is size 3; it won't fit in the template arg (size 1).
  EXPECT_THAT(ring_buffer.DeserializeFromBuffer(
                  reinterpret_cast<const uint8_t*>(kValidBufferSize3),
                  kValidBufferSize3Len),
              IsFalse());
}

TEST(LengthDelimitedRingBufferDataTest,
     DeserializeFromInvalidVersionShouldFail) {
  RingBufferData<3> ring_buffer;
  EXPECT_THAT(ring_buffer.DeserializeFromBuffer(
                  reinterpret_cast<const uint8_t*>(kInvalidVersionBuffer),
                  kInvalidVersionBufferLen),
              IsFalse());
}

TEST(LengthDelimitedRingBufferDataTest,
     DeserializeFromInvalidVarintLengthShouldSucceedButPopShouldFail) {
  RingBufferData ring_buffer;
  EXPECT_THAT(ring_buffer.DeserializeFromBuffer(
                  reinterpret_cast<const uint8_t*>(kInvalidBase128VarintBuffer),
                  kInvalidBase128VarintBufferLen),
              IsTrue());
  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> data;
  EXPECT_THAT(reader.Pop(data), IsFalse());
}

TEST(LengthDelimitedRingBufferDataTest,
     DeserializeFromInvalidVarintBitsShouldSucceedButPopShouldFail) {
  RingBufferData ring_buffer;
  EXPECT_THAT(ring_buffer.DeserializeFromBuffer(
                  reinterpret_cast<const uint8_t*>(
                      kInvalidBase128VarintBits33And34SetBuffer),
                  kInvalidBase128VarintBits33And34SetBufferLen),
              IsTrue());
  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> data;
  EXPECT_THAT(reader.Pop(data), IsFalse());
}

TEST(LengthDelimitedRingBufferDataTest,
     DeserializeFromInvalidPayloadBufferTooShortShouldSucceedButPopShouldFail) {
  RingBufferData ring_buffer;
  EXPECT_THAT(
      ring_buffer.DeserializeFromBuffer(
          reinterpret_cast<const uint8_t*>(kInvalidPayloadBufferTooShort),
          kInvalidPayloadBufferTooShortLen),
      IsTrue());
  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> data;
  EXPECT_THAT(reader.Pop(data), IsFalse());
}

TEST(LengthDelimitedRingBufferDataTest,
     DeserializeFromFullBufferShouldSucceed) {
  RingBufferData<3> ring_buffer;
  EXPECT_THAT(ring_buffer.DeserializeFromBuffer(
                  reinterpret_cast<const uint8_t*>(kValidBufferSize3),
                  kValidBufferSize3Len),
              IsTrue());
  LengthDelimitedRingBufferReader reader(ring_buffer);
  std::vector<uint8_t> data;
  EXPECT_THAT(reader.Pop(data), IsTrue());
  const std::vector<uint8_t> expected = {0x42, 0x23};
  EXPECT_THAT(data, Eq(expected));
}

TEST(LengthDelimitedRingBufferDataTest,
     DeserializeFromMidCrashBufferShouldSucceedButSubsequentPopShouldFail) {
  RingBufferData ring_buffer;
  EXPECT_THAT(ring_buffer.DeserializeFromBuffer(
                  reinterpret_cast<const uint8_t*>(kMidCrashBuffer),
                  kMidCrashBufferLen),
              IsTrue());
  LengthDelimitedRingBufferReader reader(ring_buffer);
  // Pop should fail since the length was written to be 0.
  std::vector<uint8_t> data;
  EXPECT_THAT(reader.Pop(data), IsFalse());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
