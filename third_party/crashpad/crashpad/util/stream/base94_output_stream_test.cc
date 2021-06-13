// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/stream/base94_output_stream.h"

#include <string.h>

#include <algorithm>
#include <sstream>

#include "base/cxx17_backports.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "util/stream/test_output_stream.h"

namespace crashpad {
namespace test {
namespace {

constexpr size_t kLongDataLength = 4096 * 10;

std::string DumpInput(const uint8_t* input, size_t size) {
  std::stringstream s;
  size_t index = 0;
  size_t byte_count = 0;
  while (index < size) {
    s << "0x" << std::hex << static_cast<int>(*(input + index++)) << ",";
    if (byte_count++ > 1024) {
      s << "\n";
      byte_count = 0;
    }
  }
  return s.str();
}

class Base94OutputStreamTest : public testing::Test {
 public:
  Base94OutputStreamTest() {}

 protected:
  void SetUp() override {
    auto output_stream = std::make_unique<TestOutputStream>();
    encode_test_output_stream_ = output_stream.get();
    encoder_ = std::make_unique<Base94OutputStream>(
        Base94OutputStream::Mode::kEncode, std::move(output_stream));
    output_stream = std::make_unique<TestOutputStream>();
    decode_test_output_stream_ = output_stream.get();
    decoder_ = std::make_unique<Base94OutputStream>(
        Base94OutputStream::Mode::kDecode, std::move(output_stream));
    output_stream = std::make_unique<TestOutputStream>();
    round_trip_test_output_stream_ = output_stream.get();
    round_trip_ = std::make_unique<Base94OutputStream>(
        Base94OutputStream::Mode::kEncode,
        std::make_unique<Base94OutputStream>(Base94OutputStream::Mode::kDecode,
                                             std::move(output_stream)));
  }

  const uint8_t* BuildDeterministicInput(size_t size) {
    deterministic_input_ = std::make_unique<uint8_t[]>(size);
    uint8_t* deterministic_input_base = deterministic_input_.get();
    while (size-- > 0)
      deterministic_input_base[size] = static_cast<uint8_t>(size);
    return deterministic_input_base;
  }

  const uint8_t* BuildRandomInput(size_t size) {
    input_ = std::make_unique<uint8_t[]>(size);
    base::RandBytes(&input_[0], size);
    return input_.get();
  }

  Base94OutputStream* round_trip() const { return round_trip_.get(); }
  const TestOutputStream& round_trip_test_output_stream() const {
    return *round_trip_test_output_stream_;
  }

  static void VerifyEncoding(const TestOutputStream& out,
                             const std::string& expected) {
    EXPECT_EQ(out.all_data().size(), expected.size());
    EXPECT_EQ(memcmp(out.all_data().data(), expected.data(), expected.size()),
              0);
  }

  static void VerifyDecoding(const TestOutputStream& out,
                             const std::vector<uint8_t>& expected) {
    EXPECT_EQ(out.all_data().size(), expected.size());
    EXPECT_EQ(memcmp(out.all_data().data(), expected.data(), expected.size()),
              0);
  }

  void RunTest(const std::string& text, const std::vector<uint8_t>& binary) {
    EXPECT_TRUE(encoder_->Write(binary.data(), binary.size()));
    EXPECT_TRUE(encoder_->Flush());
    VerifyEncoding(*encode_test_output_stream_, text);
    EXPECT_TRUE(decoder_->Write(reinterpret_cast<const uint8_t*>(text.data()),
                                text.size()));
    EXPECT_TRUE(decoder_->Flush());
    VerifyDecoding(*decode_test_output_stream_, binary);
  }

  void VerifyRoundTrip(const std::vector<uint8_t>& expected) {
    TestOutputStream* out = round_trip_test_output_stream_;
    EXPECT_EQ(out->all_data().size(), expected.size());
    EXPECT_EQ(memcmp(out->all_data().data(), expected.data(), expected.size()),
              0);
  }

 private:
  std::unique_ptr<Base94OutputStream> encoder_;
  std::unique_ptr<Base94OutputStream> decoder_;
  std::unique_ptr<Base94OutputStream> round_trip_;
  TestOutputStream* encode_test_output_stream_;
  TestOutputStream* decode_test_output_stream_;
  TestOutputStream* round_trip_test_output_stream_;
  std::unique_ptr<uint8_t[]> input_;
  std::unique_ptr<uint8_t[]> deterministic_input_;

  DISALLOW_COPY_AND_ASSIGN(Base94OutputStreamTest);
};

TEST_F(Base94OutputStreamTest, Encoding) {
  std::vector<uint8_t> binary = {0x0};
  std::string text("!");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Encoding1) {
  std::vector<uint8_t> binary = {0x0, 0x0};
  std::string text("!!!");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Encoding2) {
  std::vector<uint8_t> binary = {0x0, 0x0, 0x0};
  std::string text("!!!!");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Encoding3) {
  std::vector<uint8_t> binary = {0x0, 0x0, 0x0, 0x0};
  std::string text("!!!!!");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Encoding4) {
  std::vector<uint8_t> binary = {0x0, 0x0, 0x0, 0x0, 0x0};
  std::string text("!!!!!!");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Encoding10) {
  std::vector<uint8_t> binary = {0xFF};
  std::string text("d#");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Encoding11) {
  std::vector<uint8_t> binary = {0xFF, 0xFF};
  std::string text(".x(");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Encoding12) {
  std::vector<uint8_t> binary = {0xFF, 0xFF, 0xFF};
  std::string text(".xj6");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Encoding13) {
  std::vector<uint8_t> binary = {0xFF, 0xFF, 0xFF, 0xFF};
  std::string text(".x.x`");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Encoding14) {
  std::vector<uint8_t> binary = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  std::string text(".x.x.x\"");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, Printable94) {
  std::vector<uint8_t> binary = {
      0x5e, 0x0,  0x47, 0xa0, 0x1d, 0x60, 0xa,  0xab, 0x41, 0x41, 0xa4, 0x9,
      0x64, 0x71, 0x32, 0xc,  0x47, 0xf9, 0x20, 0x22, 0xa3, 0x44, 0xa0, 0x84,
      0x15, 0xe0, 0xf2, 0x61, 0xfc, 0x4c, 0xb7, 0xe1, 0x39, 0x9b, 0x47, 0xff,
      0x64, 0x21, 0x5c, 0x74, 0x91, 0xec, 0x52, 0x75, 0xa2, 0x51, 0x93, 0x4a,
      0x5e, 0x45, 0x2d, 0xd8, 0xf5, 0xc0, 0xdc, 0x58, 0x33, 0x63, 0x69, 0x8b,
      0x4d, 0xbd, 0x25, 0x39, 0x54, 0x77, 0xf0, 0xcc, 0x5e, 0xf1, 0x23, 0x81,
      0x6,  0x21, 0x71, 0x28, 0x28, 0x2};
  std::string text(
      "!\"#$%&'()*+,-./"
      "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
      "abcdefghijklmnopqrstuvwxyz{|}~!");
  RunTest(text, binary);
}

TEST_F(Base94OutputStreamTest, WriteLongDataMultipleTimes) {
  const uint8_t* input = BuildRandomInput(kLongDataLength);
  SCOPED_TRACE(base::StringPrintf("Input: %s",
                                  DumpInput(input, kLongDataLength).c_str()));
  // Call Write() a random number of times.
  size_t index = 0;
  while (index < kLongDataLength) {
    size_t write_length =
        std::min(static_cast<size_t>(base::RandInt(0, 4096 * 2)),
                 kLongDataLength - index);
    SCOPED_TRACE(
        base::StringPrintf("index %zu, write_length %zu", index, write_length));
    EXPECT_TRUE(round_trip()->Write(input + index, write_length));
    index += write_length;
  }
  EXPECT_TRUE(round_trip()->Flush());
  EXPECT_EQ(round_trip_test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(memcmp(round_trip_test_output_stream().all_data().data(),
                   input,
                   kLongDataLength),
            0);
}

TEST_F(Base94OutputStreamTest, WriteDeterministicLongDataMultipleTimes) {
  const uint8_t* input = BuildDeterministicInput(kLongDataLength);

  static constexpr size_t kWriteLengths[] = {
      4, 96, 40, kLongDataLength - 4 - 96 - 40};

  size_t offset = 0;
  for (size_t index = 0; index < base::size(kWriteLengths); ++index) {
    const size_t write_length = kWriteLengths[index];
    SCOPED_TRACE(base::StringPrintf(
        "offset %zu, write_length %zu", offset, write_length));
    EXPECT_TRUE(round_trip()->Write(input + offset, write_length));
    offset += write_length;
  }
  EXPECT_TRUE(round_trip()->Flush());
  EXPECT_EQ(round_trip_test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(memcmp(round_trip_test_output_stream().all_data().data(),
                   input,
                   kLongDataLength),
            0);
}

TEST_F(Base94OutputStreamTest, NoWriteOrFlush) {
  EXPECT_EQ(round_trip_test_output_stream().write_count(), 0u);
  EXPECT_EQ(round_trip_test_output_stream().flush_count(), 0u);
  EXPECT_TRUE(round_trip_test_output_stream().all_data().empty());
}

TEST_F(Base94OutputStreamTest, FlushWithoutWrite) {
  EXPECT_TRUE(round_trip()->Flush());
  EXPECT_EQ(round_trip_test_output_stream().write_count(), 0u);
  EXPECT_EQ(round_trip_test_output_stream().flush_count(), 1u);
  EXPECT_TRUE(round_trip_test_output_stream().all_data().empty());
}

TEST_F(Base94OutputStreamTest, WriteEmptyData) {
  std::vector<uint8_t> empty_data;
  EXPECT_TRUE(round_trip()->Write(
      static_cast<const uint8_t*>(empty_data.data()), empty_data.size()));
  EXPECT_TRUE(round_trip()->Flush());
  EXPECT_TRUE(round_trip()->Flush());
  EXPECT_EQ(round_trip_test_output_stream().write_count(), 0u);
  EXPECT_EQ(round_trip_test_output_stream().flush_count(), 2u);
  EXPECT_TRUE(round_trip_test_output_stream().all_data().empty());
}

TEST_F(Base94OutputStreamTest, Process7bitsInFinishDecoding) {
  std::vector<uint8_t> input = {
      0xff, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  EXPECT_TRUE(round_trip()->Write(static_cast<const uint8_t*>(input.data()),
                                  input.size()));
  EXPECT_TRUE(round_trip()->Flush());
  VerifyRoundTrip(input);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
