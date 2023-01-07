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

#include "util/stream/log_output_stream.h"

#include <algorithm>
#include <memory>
#include <string>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

constexpr size_t kOutputCap = 128 * 1024;
constexpr size_t kLineBufferSize = 512;
const char* kBeginGuard = "-----BEGIN CRASHPAD MINIDUMP-----";
const char* kEndGuard = "-----END CRASHPAD MINIDUMP-----";
const char* kAbortGuard = "-----ABORT CRASHPAD MINIDUMP-----";

class LogOutputStreamTestDelegate final : public LogOutputStream::Delegate {
 public:
  explicit LogOutputStreamTestDelegate(std::string* logging_destination)
      : logging_destination_(logging_destination) {}
  ~LogOutputStreamTestDelegate() override = default;

  int Log(const char* buf) override {
    size_t len = strnlen(buf, kLineBufferSize + 1);
    EXPECT_LE(len, kLineBufferSize);
    logging_destination_->append(buf, len);
    return static_cast<int>(len);
  }

  size_t OutputCap() override { return kOutputCap; }
  size_t LineWidth() override { return kLineBufferSize; }

 private:
  std::string* logging_destination_;
};

class LogOutputStreamTest : public testing::Test {
 public:
  LogOutputStreamTest() {}

  LogOutputStreamTest(const LogOutputStreamTest&) = delete;
  LogOutputStreamTest& operator=(const LogOutputStreamTest&) = delete;

 protected:
  void SetUp() override {
    log_stream_ = std::make_unique<LogOutputStream>(
        std::make_unique<LogOutputStreamTestDelegate>(&test_log_output_));
  }

  const uint8_t* BuildDeterministicInput(size_t size) {
    deterministic_input_ = std::make_unique<uint8_t[]>(size);
    uint8_t* deterministic_input_base = deterministic_input_.get();
    while (size-- > 0)
      deterministic_input_base[size] = static_cast<uint8_t>('a');
    return deterministic_input_base;
  }

  const std::string& test_log_output() const { return test_log_output_; }

  LogOutputStream* log_stream() const { return log_stream_.get(); }

 private:
  std::unique_ptr<LogOutputStream> log_stream_;
  std::string test_log_output_;
  std::unique_ptr<uint8_t[]> deterministic_input_;
};

TEST_F(LogOutputStreamTest, WriteShortLog) {
  const uint8_t* input = BuildDeterministicInput(2);
  EXPECT_TRUE(log_stream()->Write(input, 2));
  EXPECT_TRUE(log_stream()->Flush());
  // Verify OutputStream wrote 2 guards and data.
  EXPECT_FALSE(test_log_output().empty());
  EXPECT_EQ(test_log_output(),
            std::string(kBeginGuard).append("aa").append(kEndGuard));
}

TEST_F(LogOutputStreamTest, WriteLongLog) {
  size_t input_length = kLineBufferSize + kLineBufferSize / 2;
  const uint8_t* input = BuildDeterministicInput(input_length);
  // Verify OutputStream wrote 2 guards and data.
  EXPECT_TRUE(log_stream()->Write(input, input_length));
  EXPECT_TRUE(log_stream()->Flush());
  EXPECT_EQ(test_log_output().size(),
            strlen(kBeginGuard) + strlen(kEndGuard) + input_length);
}

TEST_F(LogOutputStreamTest, WriteAbort) {
  size_t input_length = kOutputCap + kLineBufferSize;
  const uint8_t* input = BuildDeterministicInput(input_length);
  EXPECT_FALSE(log_stream()->Write(input, input_length));
  EXPECT_EQ(
      test_log_output().substr(test_log_output().size() - strlen(kAbortGuard)),
      kAbortGuard);
}

TEST_F(LogOutputStreamTest, FlushAbort) {
  size_t input_length = kOutputCap - strlen(kBeginGuard) + kLineBufferSize / 2;
  const uint8_t* input = BuildDeterministicInput(input_length);
  EXPECT_TRUE(log_stream()->Write(input, input_length));
  EXPECT_FALSE(log_stream()->Flush());
  EXPECT_EQ(
      test_log_output().substr(test_log_output().size() - strlen(kAbortGuard)),
      kAbortGuard);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
