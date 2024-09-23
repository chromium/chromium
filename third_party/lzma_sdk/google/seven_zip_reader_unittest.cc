// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test files were generated with the following commands (commands for each test
// file given independently, even though many share the same contents)
//   echo "This is not an exe" > file.exe
//   7z a compressed_exe.7z file.exe
//
//   touch empty_file
//   7z a empty_file.7z empty_file
//
//   echo "This is not an exe" > file.exe
//   mkdir folder
//   cp file.exe file2.exe
//   7z a file_folder_file.7z file.exe folder file2.exe
//
//   mkdir folder
//   7z a folder.7z folder
//
//   echo "This is a text file!" > not_a_seven_zip.7z
//
//   echo "This is not an exe" > file.exe
//   7z a bad_crc.7z file.exe
//   grep --byte-offset --only-matching --text "This is not an exe" \
//     bad_crc.7z | sed 's/:.*$//' | while read -r match; do \
//     printf 'This is not an exe' | dd conv=notrunc of=bad_crc.7z \
//     bs=1 seek=$match done
//
//   fake_crc_table.7z was created with a hex editor, replacing the three CRC
//   values with 0.
//
//   echo "This is not an exe" > file.exe
//   7z a -p encrypted.7z file.exe  # Provided 1234 as the password
//
//   echo "This is not an exe" > file.exe
//   7z a  -mhe=on -p encrypted_header.7z file.exe
//   Provided 1234 as the password

#include "third_party/lzma_sdk/google/seven_zip_reader.h"

#include <array>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

extern "C" {
#include "third_party/lzma_sdk/C/7zCrc.h"
}

namespace seven_zip {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrictMock;

base::File OpenTestFile(base::FilePath::StringPieceType file_name) {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path))
    return base::File();
  path = path.Append(FILE_PATH_LITERAL("third_party/lzma_sdk/google/test_data"))
             .Append(file_name);
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

base::File OpenTemporaryFile() {
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path))
    return base::File();
  return base::File(
      temp_path,
      (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
       base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE |
       base::File::FLAG_WIN_TEMPORARY | base::File::FLAG_DELETE_ON_CLOSE));
}

}  // namespace

class MockSevenZipDelegate : public Delegate {
 public:
  MOCK_METHOD(void, OnOpenError, (Result));
  MOCK_METHOD(base::File, OnTempFileRequest, ());
  MOCK_METHOD(bool, OnEntry, (const EntryInfo&, base::span<uint8_t>&));
  MOCK_METHOD(bool, OnDirectory, (const EntryInfo&));
  MOCK_METHOD(bool, EntryDone, (Result, const EntryInfo&));
};

TEST(SevenZipReaderTest, ReportsOpenErrorForInvalidArchive) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("not_a_seven_zip.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate, OnOpenError(Result::kMalformedArchive));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  EXPECT_FALSE(reader);
}

TEST(SevenZipReaderTest, ReportsFilePath) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("compressed_exe.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate,
              OnEntry(Field(&EntryInfo::file_path,
                            base::FilePath(FILE_PATH_LITERAL("file.exe"))),
                      _))
      .WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, ReportsFileSize) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("compressed_exe.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate, OnEntry(Field(&EntryInfo::file_size, 19), _))
      .WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, ReportsFileModifiedTime) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("compressed_exe.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(
      delegate,
      OnEntry(Field(&EntryInfo::last_modified_time,
                    base::Time::FromDeltaSinceWindowsEpoch(
                        base::Microseconds(13307479390000000))),  // 2022-09-12
              _))
      .WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, ReportsDirectoryPath) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("folder.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate,
              OnDirectory(Field(&EntryInfo::file_path,
                                base::FilePath(FILE_PATH_LITERAL("folder")))))
      .WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, ReportsDirectoryModifiedTime) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("folder.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate,
              OnDirectory(Field(
                  &EntryInfo::last_modified_time,
                  base::Time::FromDeltaSinceWindowsEpoch(
                      base::Microseconds(13307155056000000)))  // 2022-09-08
                          ))
      .WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, StopsExtractionOnDirectory) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("file_folder_file.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate, OnDirectory(_)).WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, StopsExtractionOnEntry) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("file_folder_file.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate, OnDirectory(_)).WillOnce(Return(true));
  EXPECT_CALL(delegate, OnEntry(_, _)).WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, StopsExtractionOnEntryDone) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("file_folder_file.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  std::array<uint8_t, 19> buffer;
  EXPECT_CALL(delegate, OnTempFileRequest())
      .WillOnce(Return(ByMove(OpenTemporaryFile())));
  EXPECT_CALL(delegate, OnDirectory(_)).WillOnce(Return(true));
  EXPECT_CALL(delegate, OnEntry(Field(&EntryInfo::file_size, 19), _))
      .WillOnce(DoAll(SetArgReferee<1>(base::make_span(buffer)), Return(true)));
  EXPECT_CALL(delegate, EntryDone(_, _)).WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, ExtractsInTempBuffer) {
  // This test file has two identical files stored in the same "folder" (in the
  // 7z-internal sense), so it uses the temp buffer.
  base::File file = OpenTestFile(FILE_PATH_LITERAL("file_folder_file.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  Matcher<const EntryInfo&> matches = Field(&EntryInfo::file_size, 19);
  std::array<uint8_t, 19> buffer;
  EXPECT_CALL(delegate, OnTempFileRequest())
      .WillOnce(Return(ByMove(OpenTemporaryFile())));
  EXPECT_CALL(delegate, OnDirectory(_)).WillOnce(Return(true));
  EXPECT_CALL(delegate, OnEntry(Field(&EntryInfo::file_size, 19), _))
      .WillOnce(DoAll(SetArgReferee<1>(base::make_span(buffer)), Return(true)));
  EXPECT_CALL(delegate, EntryDone(Result::kSuccess, _)).WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();

  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "This is not an exe\n");
}

TEST(SevenZipReaderTest, ExtractsNoTempBuffer) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("compressed_exe.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  std::array<uint8_t, 19> buffer;
  EXPECT_CALL(delegate, OnEntry(Field(&EntryInfo::file_size, 19), _))
      .WillOnce(DoAll(SetArgReferee<1>(base::make_span(buffer)), Return(true)));
  EXPECT_CALL(delegate, EntryDone(Result::kSuccess, _)).WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();

  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "This is not an exe\n");
}

TEST(SevenZipReaderTest, BadCrc) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("bad_crc.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  std::array<uint8_t, 19> buffer;
  EXPECT_CALL(delegate, OnEntry(Field(&EntryInfo::file_size, 19), _))
      .WillOnce(DoAll(SetArgReferee<1>(base::make_span(buffer)), Return(true)));
  EXPECT_CALL(delegate, EntryDone(Result::kBadCrc, _)).WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, EmptyFile) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("empty_file.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  std::array<uint8_t, 0> buffer;
  EXPECT_CALL(delegate, OnEntry(Field(&EntryInfo::file_size, 0), _))
      .WillOnce(DoAll(SetArgReferee<1>(base::make_span(buffer)), Return(true)));
  EXPECT_CALL(delegate, EntryDone(Result::kSuccess, _)).WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

class SevenZipReaderFakeCrcTableTest : public testing::Test {
 public:
  SevenZipReaderFakeCrcTableTest() = default;

  void SetUp() override {
    seven_zip::EnsureLzmaSdkInitialized();
    for (size_t i = 0; i < 2048; i++) {
      crc_table_[i] = g_CrcTable[i];
    }

    // Create a fake CRC table that always returns 0 for the target value. This
    // significantly increases the fuzzer's ability to safely mutate the
    // headers. The values here were chosen to keep the CRC internal state as
    // 0xffffffff in CrcUpdateT8. Other processors may choose a different CRC
    // update function, and would need different values here. See the
    // CrcGenerateTable function in //third_party/lzma_sdk/C/7zCrc.c.
    for (size_t i = 0; i < 256; i++) {
      g_CrcTable[i] = 0xff000000;
      g_CrcTable[i + 0x100] = 0xff000000;
      g_CrcTable[i + 0x200] = 0;
      g_CrcTable[i + 0x300] = 0;
    }

    for (size_t i = 0x0; i < 256; i++) {
      g_CrcTable[i + 0x400] = 0xffffffff;
      g_CrcTable[i + 0x500] = 0;
      g_CrcTable[i + 0x600] = 0;
      g_CrcTable[i + 0x700] = 0;
    }
  }

  void TearDown() override {
    for (size_t i = 0; i < 2048; i++) {
      g_CrcTable[i] = crc_table_[i];
    }
  }

 private:
  std::array<uint32_t, 2048> crc_table_;
};

// This is useful functionality for the fuzzer, so we test it here.
TEST_F(SevenZipReaderFakeCrcTableTest, EmptyCrcWithFakeTable) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("fake_crc_table.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  std::array<uint8_t, 19> buffer;
  EXPECT_CALL(delegate, OnEntry(Field(&EntryInfo::file_size, 19), _))
      .WillOnce(DoAll(SetArgReferee<1>(base::make_span(buffer)), Return(true)));
  EXPECT_CALL(delegate, EntryDone(Result::kSuccess, _)).WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();

  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "This is not an exe\n");
}

TEST(SevenZipReaderTest, EncryptedFile) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("encrypted.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate, OnEntry(Field(&EntryInfo::is_encrypted, true), _))
      .WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, UnencryptedFile) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("compressed_exe.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate, OnEntry(Field(&EntryInfo::is_encrypted, false), _))
      .WillOnce(Return(false));

  std::unique_ptr<SevenZipReader> reader =
      SevenZipReader::Create(std::move(file), delegate);
  ASSERT_TRUE(reader);
  reader->Extract();
}

TEST(SevenZipReaderTest, EncryptedHeaders) {
  base::File file = OpenTestFile(FILE_PATH_LITERAL("encrypted_header.7z"));
  ASSERT_TRUE(file.IsValid());

  StrictMock<MockSevenZipDelegate> delegate;
  EXPECT_CALL(delegate, OnOpenError(Result::kEncryptedHeaders));
  EXPECT_CALL(delegate, OnTempFileRequest())
      .WillOnce(Return(ByMove(OpenTemporaryFile())));

  EXPECT_EQ(SevenZipReader::Create(std::move(file), delegate), nullptr);
}

}  // namespace seven_zip
