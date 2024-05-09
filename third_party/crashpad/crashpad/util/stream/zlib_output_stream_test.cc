// Copyright 2019 The Crashpad Authors
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

#include "util/stream/zlib_output_stream.h"

#include <string.h>

#include <algorithm>
#include <iterator>

#include "base/containers/heap_array.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "util/stream/test_output_stream.h"

namespace crashpad {
namespace test {
namespace {

constexpr size_t kShortDataLength = 10;
constexpr size_t kLongDataLength = 4096 * 10;

class ZlibOutputStreamTest : public testing::Test {
 public:
  ZlibOutputStreamTest() : input_(), deterministic_input_() {
    auto test_output_stream = std::make_unique<TestOutputStream>();
    test_output_stream_ = test_output_stream.get();
    zlib_output_stream_ = std::make_unique<ZlibOutputStream>(
        ZlibOutputStream::Mode::kCompress,
        std::make_unique<ZlibOutputStream>(ZlibOutputStream::Mode::kDecompress,
                                           std::move(test_output_stream)));
  }

  ZlibOutputStreamTest(const ZlibOutputStreamTest&) = delete;
  ZlibOutputStreamTest& operator=(const ZlibOutputStreamTest&) = delete;

  const uint8_t* BuildDeterministicInput(size_t size) {
    deterministic_input_ = base::HeapArray<uint8_t>::WithSize(size);
    while (size-- > 0)
      deterministic_input_[size] = static_cast<uint8_t>(size);
    return deterministic_input_.data();
  }

  const uint8_t* BuildRandomInput(size_t size) {
    input_ = base::HeapArray<uint8_t>::Uninit(size);
    base::RandBytes(input_);
    return input_.data();
  }

  const TestOutputStream& test_output_stream() const {
    return *test_output_stream_;
  }

  ZlibOutputStream* zlib_output_stream() const {
    return zlib_output_stream_.get();
  }

 private:
  std::unique_ptr<ZlibOutputStream> zlib_output_stream_;
  base::HeapArray<uint8_t> input_;
  base::HeapArray<uint8_t> deterministic_input_;
  TestOutputStream* test_output_stream_;  // weak, owned by zlib_output_stream_
};

TEST_F(ZlibOutputStreamTest, WriteDeterministicShortData) {
  const uint8_t* input = BuildDeterministicInput(kShortDataLength);
  EXPECT_TRUE(zlib_output_stream()->Write(input, kShortDataLength));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().last_written_data().size(), kShortDataLength);
  EXPECT_EQ(memcmp(test_output_stream().last_written_data().data(),
                   input,
                   kShortDataLength),
            0);
}

TEST_F(ZlibOutputStreamTest, WriteDeterministicLongDataOneTime) {
  const uint8_t* input = BuildDeterministicInput(kLongDataLength);
  EXPECT_TRUE(zlib_output_stream()->Write(input, kLongDataLength));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(
      memcmp(test_output_stream().all_data().data(), input, kLongDataLength),
      0);
}

TEST_F(ZlibOutputStreamTest, WriteDeterministicLongDataMultipleTimes) {
  const uint8_t* input = BuildDeterministicInput(kLongDataLength);

  static constexpr size_t kWriteLengths[] = {
      4, 96, 40, kLongDataLength - 4 - 96 - 40};

  size_t offset = 0;
  for (size_t index = 0; index < std::size(kWriteLengths); ++index) {
    const size_t write_length = kWriteLengths[index];
    SCOPED_TRACE(base::StringPrintf(
        "offset %zu, write_length %zu", offset, write_length));
    EXPECT_TRUE(zlib_output_stream()->Write(input + offset, write_length));
    offset += write_length;
  }
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(
      memcmp(test_output_stream().all_data().data(), input, kLongDataLength),
      0);
}

TEST_F(ZlibOutputStreamTest, WriteShortData) {
  const uint8_t* input = BuildRandomInput(kShortDataLength);
  EXPECT_TRUE(zlib_output_stream()->Write(input, kShortDataLength));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(memcmp(test_output_stream().last_written_data().data(),
                   input,
                   kShortDataLength),
            0);
  EXPECT_EQ(test_output_stream().last_written_data().size(), kShortDataLength);
}

TEST_F(ZlibOutputStreamTest, WriteLongDataOneTime) {
  const uint8_t* input = BuildRandomInput(kLongDataLength);
  EXPECT_TRUE(zlib_output_stream()->Write(input, kLongDataLength));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(
      memcmp(test_output_stream().all_data().data(), input, kLongDataLength),
      0);
}

TEST_F(ZlibOutputStreamTest, WriteLongDataMultipleTimes) {
  const uint8_t* input = BuildRandomInput(kLongDataLength);

  // Call Write() a random number of times.
  size_t index = 0;
  while (index < kLongDataLength) {
    size_t write_length =
        std::min(static_cast<size_t>(base::RandInt(0, 4096 * 2)),
                 kLongDataLength - index);
    SCOPED_TRACE(
        base::StringPrintf("index %zu, write_length %zu", index, write_length));
    EXPECT_TRUE(zlib_output_stream()->Write(input + index, write_length));
    index += write_length;
  }
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(
      memcmp(test_output_stream().all_data().data(), input, kLongDataLength),
      0);
}

TEST_F(ZlibOutputStreamTest, NoWriteOrFlush) {
  EXPECT_EQ(test_output_stream().write_count(), 0u);
  EXPECT_EQ(test_output_stream().flush_count(), 0u);
  EXPECT_TRUE(test_output_stream().all_data().empty());
}

TEST_F(ZlibOutputStreamTest, FlushWithoutWrite) {
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().write_count(), 0u);
  EXPECT_EQ(test_output_stream().flush_count(), 1u);
  EXPECT_TRUE(test_output_stream().all_data().empty());
}

TEST_F(ZlibOutputStreamTest, WriteEmptyData) {
  std::vector<uint8_t> empty_data;
  EXPECT_TRUE(zlib_output_stream()->Write(
      static_cast<const uint8_t*>(empty_data.data()), empty_data.size()));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().write_count(), 0u);
  EXPECT_EQ(test_output_stream().flush_count(), 2u);
  EXPECT_TRUE(test_output_stream().all_data().empty());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
