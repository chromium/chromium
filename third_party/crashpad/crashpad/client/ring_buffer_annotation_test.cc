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

#include "client/ring_buffer_annotation.h"
#include "client/length_delimited_ring_buffer.h"

#include <array>
#include <string>

#include "client/annotation_list.h"
#include "client/crashpad_info.h"
#include "gtest/gtest.h"
#include "test/gtest_death.h"

namespace crashpad {
namespace test {
namespace {

constexpr uint32_t kRingBufferHeaderSize = 16;
constexpr uint32_t kLengthDelimiter1ByteSize = 1;

class RingBufferAnnotationTest : public testing::Test {
 public:
  void SetUp() override {
    CrashpadInfo::GetCrashpadInfo()->set_annotations_list(&annotations_);
  }

  void TearDown() override {
    CrashpadInfo::GetCrashpadInfo()->set_annotations_list(nullptr);
  }

  size_t AnnotationsCount() {
    size_t result = 0;
    for (auto* annotation : annotations_) {
      if (annotation->is_set())
        ++result;
    }
    return result;
  }

 protected:
  AnnotationList annotations_;
};

TEST_F(RingBufferAnnotationTest, Basics) {
  constexpr Annotation::Type kType = Annotation::UserDefinedType(1);

  constexpr char kName[] = "annotation 1";
  RingBufferAnnotation annotation(kType, kName);

  EXPECT_FALSE(annotation.is_set());
  EXPECT_EQ(0u, AnnotationsCount());

  EXPECT_EQ(kType, annotation.type());
  EXPECT_EQ(0u, annotation.size());
  EXPECT_EQ(std::string(kName), annotation.name());

  EXPECT_TRUE(
      annotation.Push(reinterpret_cast<const uint8_t*>("0123456789"), 10));

  EXPECT_TRUE(annotation.is_set());
  EXPECT_EQ(1u, AnnotationsCount());

  constexpr Annotation::ValueSizeType kExpectedSize =
      kRingBufferHeaderSize + kLengthDelimiter1ByteSize + 10u;
  EXPECT_EQ(kExpectedSize, annotation.size());
  EXPECT_EQ(&annotation, *annotations_.begin());

  RingBufferData data;
  EXPECT_TRUE(
      data.DeserializeFromBuffer(annotation.value(), annotation.size()));
  EXPECT_EQ(kExpectedSize, data.GetRingBufferLength());

  std::vector<uint8_t> popped_value;
  LengthDelimitedRingBufferReader reader(data);
  EXPECT_TRUE(reader.Pop(popped_value));

  const std::vector<uint8_t> expected = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
  EXPECT_EQ(expected, popped_value);

  annotation.Clear();

  EXPECT_FALSE(annotation.is_set());
  EXPECT_EQ(0u, AnnotationsCount());

  EXPECT_EQ(0u, annotation.size());
}

TEST_F(RingBufferAnnotationTest, MultiplePushesWithoutWrapping) {
  constexpr Annotation::Type kType = Annotation::UserDefinedType(1);

  constexpr char kName[] = "annotation 1";
  RingBufferAnnotation annotation(kType, kName);

  EXPECT_TRUE(
      annotation.Push(reinterpret_cast<const uint8_t*>("0123456789"), 10));
  EXPECT_TRUE(annotation.Push(reinterpret_cast<const uint8_t*>("ABCDEF"), 6));

  EXPECT_TRUE(annotation.is_set());
  EXPECT_EQ(1u, AnnotationsCount());

  constexpr Annotation::ValueSizeType kExpectedSize =
      kRingBufferHeaderSize + kLengthDelimiter1ByteSize + 10u +
      kLengthDelimiter1ByteSize + 6u;
  EXPECT_EQ(kExpectedSize, annotation.size());
  EXPECT_EQ(&annotation, *annotations_.begin());

  RingBufferData data;
  EXPECT_TRUE(
      data.DeserializeFromBuffer(annotation.value(), annotation.size()));
  EXPECT_EQ(kExpectedSize, data.GetRingBufferLength());

  std::vector<uint8_t> popped_value;
  LengthDelimitedRingBufferReader reader(data);
  EXPECT_TRUE(reader.Pop(popped_value));

  const std::vector<uint8_t> expected1 = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
  EXPECT_EQ(expected1, popped_value);

  popped_value.clear();
  EXPECT_TRUE(reader.Pop(popped_value));

  const std::vector<uint8_t> expected2 = {'A', 'B', 'C', 'D', 'E', 'F'};
  EXPECT_EQ(expected2, popped_value);
}

TEST_F(RingBufferAnnotationTest,
       MultiplePushCallsWithWrappingShouldOverwriteInFIFOOrder) {
  constexpr Annotation::Type kType = Annotation::UserDefinedType(1);

  constexpr char kName[] = "annotation 1";
  RingBufferAnnotation<10> annotation(kType, kName);

  // Each Push() call will push 1 byte for the varint 128-encoded length,
  // then the number of bytes specified.
  constexpr char kFirst[] = "AAA";
  constexpr char kSecond[] = "BBB";
  constexpr char kThird[] = "CCC";

  // This takes up bytes 0-3 of the 10-byte RingBufferAnnotation.
  ASSERT_TRUE(annotation.Push(reinterpret_cast<const uint8_t*>(kFirst), 3));

  // This takes up bytes 4-7 of the 10-byte RingBufferAnnotation.
  ASSERT_TRUE(annotation.Push(reinterpret_cast<const uint8_t*>(kSecond), 3));

  // This should wrap around the end of the array and overwrite kFirst since it
  // needs 4 bytes but there are only 2 left.
  ASSERT_TRUE(annotation.Push(reinterpret_cast<const uint8_t*>(kThird), 3));

  // The size of the annotation should include the header and the full 10 bytes
  // of the ring buffer, since the third write wrapped around the end.
  ASSERT_EQ(kRingBufferHeaderSize + 10u, annotation.size());

  // This data size needs to match the size in the RingBufferAnnotation above.
  RingBufferData<10> data;
  ASSERT_TRUE(
      data.DeserializeFromBuffer(annotation.value(), annotation.size()));

  std::vector<uint8_t> popped_value;
  LengthDelimitedRingBufferReader reader(data);
  ASSERT_TRUE(reader.Pop(popped_value));

  // "AAA" has been overwritten, so the first thing popped should be "BBB".
  const std::vector<uint8_t> expected_b = {'B', 'B', 'B'};
  EXPECT_EQ(expected_b, popped_value);

  popped_value.clear();
  ASSERT_TRUE(reader.Pop(popped_value));
  const std::vector<uint8_t> expected_c = {'C', 'C', 'C'};
  EXPECT_EQ(expected_c, popped_value);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
