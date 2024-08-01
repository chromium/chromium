// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <utility>

#include "base/memory/raw_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/mock_source_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/i18n_source_stream.h"

namespace ui {

namespace {

// This constant is rather arbitrary, though the offsets and other sizes must
// be less than kBufferSize.
const int kBufferSize = 256;

const int kMinimumSize = 1;
const int kSmallSize = 5;  // Arbitrary small value > 1.
const int kInOneReadSize = INT_MAX;

struct I18nTest {
  constexpr I18nTest(const char* input, const char* expected_output)
      : input(input), expected_output(expected_output) {}

  const char* input;
  const char* expected_output;
};

constexpr I18nTest kTestEmpty = I18nTest("", "");

constexpr I18nTest kTestNoReplacements =
    I18nTest("This text has no i18n replacements.",
             "This text has no i18n replacements.");

constexpr I18nTest kTestTagAtEndOfLine =
    I18nTest("test with tag at end of line $",
             "test with tag at end of line $");

constexpr I18nTest kTestOneReplacement = I18nTest("$i18n{alpha}", "apple");

constexpr I18nTest kTestOneReplacementPlus =
    I18nTest("Extra text $i18n{alpha}.", "Extra text apple.");

constexpr I18nTest kTestThreeReplacements =
    I18nTest("$i18n{alpha}^$i18n{beta}_$i18n{gamma}", "apple^banana_carrot");

constexpr I18nTest kTestExtraBraces =
    I18nTest("($i18n{alpha})^_^_^_^_$i18n{beta}_beta_$i18n{gamma}}}}}}",
             "(apple)^_^_^_^_banana_beta_carrot}}}}}");

// These tests with generic names are sequences that might catch an error in the
// future, depending on how the code changes.
constexpr I18nTest kTest1 =
    I18nTest("  }    $($i18n{gamma})^_^_^_^_$i18n{alpha}_$i18n{gamma}$",
             "  }    $(carrot)^_^_^_^_apple_carrot$");

constexpr I18nTest kTest2 =
    I18nTest("$i18n{alpha} gamma}{ ^_^_^_^_$abc{beta}:$i18n{gamma}z",
             "apple gamma}{ ^_^_^_^_$abc{beta}:carrotz");

struct I18nTestParam {
  constexpr I18nTestParam(
      const I18nTest* test,
      int buf_size,
      int read_size,
      net::MockSourceStream::Mode read_mode = net::MockSourceStream::SYNC)
      : buffer_size(buf_size),
        read_size(read_size),
        mode(read_mode),
        test(test) {}

  const int buffer_size;
  const int read_size;
  const net::MockSourceStream::Mode mode;
  raw_ptr<const I18nTest> test;
};

}  // namespace

class I18nSourceStreamTest : public ::testing::TestWithParam<I18nTestParam> {
 protected:
  I18nSourceStreamTest() : output_buffer_size_(GetParam().buffer_size) {}

  // Helpful function to initialize the test fixture.
  void Init() {
    output_buffer_ =
        base::MakeRefCounted<net::IOBufferWithSize>(output_buffer_size_);
    std::unique_ptr<net::MockSourceStream> source(new net::MockSourceStream());
    source_ = source.get();

    replacements_["alpha"] = "apple";
    replacements_["beta"] = "banana";
    replacements_["gamma"] = "carrot";
    stream_ = I18nSourceStream::Create(
        std::move(source), net::SourceStream::TYPE_NONE, &replacements_);
  }

  // If MockSourceStream::Mode is ASYNC, completes 1 read from |mock_stream| and
  // wait for |callback| to complete. If Mode is not ASYNC, does nothing and
  // returns |previous_result|.
  int CompleteReadIfAsync(int previous_result,
                          net::TestCompletionCallback* callback,
                          net::MockSourceStream* mock_stream) {
    if (GetParam().mode == net::MockSourceStream::ASYNC) {
      EXPECT_EQ(net::ERR_IO_PENDING, previous_result);
      mock_stream->CompleteNextRead();
      return callback->WaitForResult();
    }
    return previous_result;
  }

  net::IOBuffer* output_buffer() { return output_buffer_.get(); }
  char* output_data() { return output_buffer_->data(); }
  size_t output_buffer_size() { return output_buffer_size_; }

  net::MockSourceStream* source() { return source_; }
  I18nSourceStream* stream() { return stream_.get(); }

  void PushReadResults(const char* input, size_t chunk_size) {
    size_t written = 0;
    size_t source_size = strlen(GetParam().test->input);
    while (written != source_size) {
      size_t write_size = std::min(chunk_size, source_size - written);
      source()->AddReadResult(input + written, write_size, net::OK,
                              GetParam().mode);
      written += write_size;
    }
    source()->AddReadResult(nullptr, 0, net::OK, GetParam().mode);
  }

  // Reads from |stream_| until an error occurs or the EOF is reached.
  // When an error occurs, returns the net error code. When an EOF is reached,
  // returns the number of bytes read and appends data read to |output|.
  int ReadStream(std::string* output) {
    int bytes_read = 0;
    while (true) {
      net::TestCompletionCallback callback;
      int rv = stream_->Read(output_buffer(), output_buffer_size(),
                             callback.callback());
      if (rv == net::ERR_IO_PENDING)
        rv = CompleteReadIfAsync(rv, &callback, source());
      if (rv == net::OK)
        break;
      if (rv < net::OK)
        return rv;
      EXPECT_GT(rv, net::OK);
      bytes_read += rv;
      output->append(output_data(), rv);
    }
    return bytes_read;
  }

 private:
  scoped_refptr<net::IOBuffer> output_buffer_;
  const int output_buffer_size_;

  std::unique_ptr<I18nSourceStream> stream_;  // Must outlive `source_`.
  raw_ptr<net::MockSourceStream> source_;

  TemplateReplacements replacements_;
};

INSTANTIATE_TEST_SUITE_P(
    I18nSourceStreamTests,
    I18nSourceStreamTest,
    ::testing::Values(
        I18nTestParam(&kTest1, kBufferSize, kInOneReadSize),
        I18nTestParam(&kTest1, kBufferSize, kSmallSize),
        I18nTestParam(&kTest1, kMinimumSize, kMinimumSize),
        I18nTestParam(&kTest1, kMinimumSize, kSmallSize),

        I18nTestParam(&kTest2, kBufferSize, kInOneReadSize),
        I18nTestParam(&kTest2, kBufferSize, kSmallSize),
        I18nTestParam(&kTest2, kMinimumSize, kMinimumSize),
        I18nTestParam(&kTest2, kMinimumSize, kSmallSize),

        I18nTestParam(&kTestEmpty, kBufferSize, kInOneReadSize),
        I18nTestParam(&kTestEmpty, kBufferSize, kSmallSize),
        I18nTestParam(&kTestEmpty, kMinimumSize, kMinimumSize),
        I18nTestParam(&kTestEmpty, kMinimumSize, kSmallSize),

        I18nTestParam(&kTestExtraBraces, kBufferSize, kInOneReadSize),
        I18nTestParam(&kTestExtraBraces, kBufferSize, kSmallSize),
        I18nTestParam(&kTestExtraBraces, kMinimumSize, kMinimumSize),
        I18nTestParam(&kTestExtraBraces, kMinimumSize, kSmallSize),

        I18nTestParam(&kTestNoReplacements, kBufferSize, kInOneReadSize),
        I18nTestParam(&kTestNoReplacements, kBufferSize, kSmallSize),
        I18nTestParam(&kTestNoReplacements, kMinimumSize, kMinimumSize),
        I18nTestParam(&kTestNoReplacements, kMinimumSize, kSmallSize),

        I18nTestParam(&kTestOneReplacement, kBufferSize, kInOneReadSize),
        I18nTestParam(&kTestOneReplacement, kBufferSize, kSmallSize),
        I18nTestParam(&kTestOneReplacement, kMinimumSize, kMinimumSize),
        I18nTestParam(&kTestOneReplacement, kMinimumSize, kSmallSize),

        I18nTestParam(&kTestOneReplacementPlus, kBufferSize, kInOneReadSize),
        I18nTestParam(&kTestOneReplacementPlus, kBufferSize, kSmallSize),
        I18nTestParam(&kTestOneReplacementPlus, kMinimumSize, kMinimumSize),
        I18nTestParam(&kTestOneReplacementPlus, kMinimumSize, kSmallSize),

        I18nTestParam(&kTestTagAtEndOfLine, kBufferSize, kInOneReadSize),
        I18nTestParam(&kTestTagAtEndOfLine, kBufferSize, kSmallSize),
        I18nTestParam(&kTestTagAtEndOfLine, kMinimumSize, kMinimumSize),
        I18nTestParam(&kTestTagAtEndOfLine, kMinimumSize, kSmallSize),

        I18nTestParam(&kTestThreeReplacements, kBufferSize, kInOneReadSize),
        I18nTestParam(&kTestThreeReplacements, kBufferSize, kSmallSize),
        I18nTestParam(&kTestThreeReplacements, kMinimumSize, kMinimumSize),
        I18nTestParam(&kTestThreeReplacements, kMinimumSize, kSmallSize)));

TEST_P(I18nSourceStreamTest, FilterTests) {
  Init();
  // Create the chain of read buffers.
  PushReadResults(GetParam().test->input, GetParam().read_size);

  // Process the buffers.
  std::string actual_output;
  int rv = ReadStream(&actual_output);

  // Check the results.
  std::string expected_output(GetParam().test->expected_output);
  EXPECT_EQ(expected_output.size(), static_cast<size_t>(rv));
  EXPECT_EQ(expected_output, actual_output);
  EXPECT_EQ("i18n", stream()->Description());
}

TEST_P(I18nSourceStreamTest, LargeFilterTests) {
  Init();
  std::string padding;
  // 251 and 599 are prime and avoid power-of-two repetition.
  int padding_modulus = 251;
  int pad_size = 599;
  padding.resize(pad_size);
  for (int i = 0; i < pad_size; ++i)
    padding[i] = i % padding_modulus;

  // Create the chain of read buffers.
  const int kPadCount = 128;  // Arbitrary number of pads to add.
  for (int i = 0; i < kPadCount; ++i) {
    source()->AddReadResult(padding.c_str(), padding.size(), net::OK,
                            GetParam().mode);
  }
  PushReadResults(GetParam().test->input, GetParam().read_size);

  // Process the buffers.
  std::string actual_output;
  int rv = ReadStream(&actual_output);

  // Check the results.
  size_t total_padding = kPadCount * padding.size();
  std::string expected_output(GetParam().test->expected_output);
  ASSERT_EQ(expected_output.size() + total_padding, static_cast<size_t>(rv));
  for (int i = 0; i < kPadCount; ++i) {
    EXPECT_EQ(actual_output.substr(i * padding.size(), padding.size()),
              padding);
  }
  EXPECT_EQ(expected_output, &actual_output[total_padding]);
  EXPECT_EQ("i18n", stream()->Description());
}

}  // namespace ui
