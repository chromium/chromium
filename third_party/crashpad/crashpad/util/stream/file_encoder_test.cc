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

#include "util/stream/file_encoder.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"
#include "util/stream/file_output_stream.h"

namespace crashpad {
namespace test {
namespace {

constexpr size_t kBufferSize = 4096;

class FileEncoderTest : public testing::Test {
 public:
  FileEncoderTest() {}

  void Verify(size_t size) {
    std::string contents;
    ASSERT_TRUE(LoggingReadEntireFile(decoded_, &contents));
    ASSERT_EQ(contents.size(), size);
    EXPECT_EQ(memcmp(deterministic_input_.get(), contents.data(), size), 0);
  }

  const uint8_t* BuildDeterministicInput(size_t size) {
    deterministic_input_ = std::make_unique<uint8_t[]>(size);
    uint8_t* deterministic_input_base = deterministic_input_.get();
    while (size-- > 0)
      deterministic_input_base[size] = static_cast<uint8_t>(size);
    return deterministic_input_base;
  }

  void GenerateOrigFile(size_t size) {
    ScopedFileHandle write_handle(OpenFileForWrite(
        orig_, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
    ASSERT_TRUE(write_handle.is_valid());
    FileOutputStream out(write_handle.get());
    const uint8_t* buf = BuildDeterministicInput(size);
    while (size > 0) {
      size_t m = std::min(kBufferSize, size);
      ASSERT_TRUE(out.Write(buf, m));
      size -= m;
      buf += m;
    }
    ASSERT_TRUE(out.Flush());
  }

  FileEncoder* encoder() const { return encoder_.get(); }

  FileEncoder* decoder() const { return decoder_.get(); }

 protected:
  void SetUp() override {
    temp_dir_ = std::make_unique<ScopedTempDir>();
    orig_ = base::FilePath(temp_dir_->path().Append(FILE_PATH_LITERAL("orig")));
    encoded_ =
        base::FilePath(temp_dir_->path().Append(FILE_PATH_LITERAL("encoded")));
    decoded_ =
        base::FilePath(temp_dir_->path().Append(FILE_PATH_LITERAL("decoded")));
    encoder_ = std::make_unique<FileEncoder>(
        FileEncoder::Mode::kEncode, orig_, encoded_);
    decoder_ = std::make_unique<FileEncoder>(
        FileEncoder::Mode::kDecode, encoded_, decoded_);
  }

 private:
  std::unique_ptr<ScopedTempDir> temp_dir_;
  base::FilePath orig_;
  base::FilePath encoded_;
  base::FilePath decoded_;
  std::unique_ptr<FileEncoder> encoder_;
  std::unique_ptr<FileEncoder> decoder_;
  std::unique_ptr<uint8_t[]> deterministic_input_;
};

TEST_F(FileEncoderTest, ProcessShortFile) {
  GenerateOrigFile(kBufferSize - 512);
  EXPECT_TRUE(encoder()->Process());
  EXPECT_TRUE(decoder()->Process());
  Verify(kBufferSize - 512);
}

TEST_F(FileEncoderTest, ProcessLongFile) {
  GenerateOrigFile(kBufferSize + 512);
  EXPECT_TRUE(encoder()->Process());
  EXPECT_TRUE(decoder()->Process());
  Verify(kBufferSize + 512);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
