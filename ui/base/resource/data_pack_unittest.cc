// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/resource/data_pack.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/data_pack_literal.h"
#include "ui/base/ui_base_paths.h"

namespace ui {

class DataPackTest
    : public testing::TestWithParam<DataPack::TextEncodingType> {
 public:
  DataPackTest() {}
};

TEST(DataPackTest, LoadFromPath) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath data_path =
      dir.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak file.
  ASSERT_TRUE(
      base::WriteFile(data_path, {kSamplePakContentsV4, kSamplePakSizeV4}));

  // Load the file through the data pack API.
  DataPack pack(k100Percent);
  ASSERT_TRUE(pack.LoadFromPath(data_path));

  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_EQ(pack.GetStringView(4),
            std::make_optional(std::string_view{"this is id 4"}));
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_EQ(pack.GetStringView(6),
            std::make_optional(std::string_view{"this is id 6"}));

  // Try reading zero-length data blobs, just in case.
  ASSERT_EQ(pack.GetStringView(1), std::make_optional(std::string_view{}));
  ASSERT_EQ(pack.GetStringView(10), std::make_optional(std::string_view{}));

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringView(140).has_value());
}

TEST(DataPackTest, LoadFromPathCompressed) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath data_path =
      dir.GetPath().Append(FILE_PATH_LITERAL("sample.pak.gz"));

  // Dump contents into a compressed pak file.
  std::string compressed;
  ASSERT_TRUE(compression::GzipCompress(
      {kSamplePakContentsV4, kSamplePakSizeV4}, &compressed));
  ASSERT_TRUE(base::WriteFile(data_path, compressed));

  // Load the file through the data pack API.
  DataPack pack(k100Percent);
  ASSERT_TRUE(pack.LoadFromPath(data_path));

  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_EQ(pack.GetStringView(4),
            std::make_optional(std::string_view{"this is id 4"}));
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_EQ(pack.GetStringView(6),
            std::make_optional(std::string_view{"this is id 6"}));

  // Try reading zero-length data blobs, just in case.
  ASSERT_EQ(pack.GetStringView(1), std::make_optional(std::string_view{}));
  ASSERT_EQ(pack.GetStringView(10), std::make_optional(std::string_view{}));

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringView(140).has_value());
}

TEST(DataPackTest, LoadFromFile) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath data_path =
      dir.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak file.
  ASSERT_TRUE(
      base::WriteFile(data_path, {kSamplePakContentsV4, kSamplePakSizeV4}));

  base::File file(data_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  // Load the file through the data pack API.
  DataPack pack(k100Percent);
  ASSERT_TRUE(pack.LoadFromFile(std::move(file)));

  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_EQ(pack.GetStringView(4),
            std::make_optional(std::string_view{"this is id 4"}));
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_EQ(pack.GetStringView(6),
            std::make_optional(std::string_view{"this is id 6"}));

  // Try reading zero-length data blobs, just in case.
  ASSERT_EQ(pack.GetStringView(1), std::make_optional(std::string_view{}));
  ASSERT_EQ(pack.GetStringView(10), std::make_optional(std::string_view{}));

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringView(140).has_value());
}

TEST(DataPackTest, LoadFromFileRegion) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath data_path =
      dir.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));

  // Construct a file which has a non page-aligned zero-filled header followed
  // by the actual pak file content.
  const uint8_t kPadding[5678] = {0};
  ASSERT_TRUE(base::WriteFile(data_path, kPadding));
  ASSERT_TRUE(
      base::AppendToFile(data_path, {kSamplePakContentsV4, kSamplePakSizeV4}));

  base::File file(data_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  // Load the file through the data pack API.
  DataPack pack(k100Percent);
  base::MemoryMappedFile::Region region = {sizeof(kPadding), kSamplePakSizeV4};
  ASSERT_TRUE(pack.LoadFromFileRegion(std::move(file), region));

  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_EQ(pack.GetStringView(4),
            std::make_optional(std::string_view{"this is id 4"}));
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_EQ(pack.GetStringView(6),
            std::make_optional(std::string_view{"this is id 6"}));

  // Try reading zero-length data blobs, just in case.
  ASSERT_EQ(pack.GetStringView(1), std::make_optional(std::string_view{}));
  ASSERT_EQ(pack.GetStringView(10), std::make_optional(std::string_view{}));

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringView(140).has_value());
}

TEST(DataPackTest, LoadFromBufferV4) {
  DataPack pack(k100Percent);

  ASSERT_TRUE(pack.LoadFromBuffer({kSamplePakContentsV4, kSamplePakSizeV4}));

  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_EQ(pack.GetStringView(4),
            std::make_optional(std::string_view{"this is id 4"}));
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_EQ(pack.GetStringView(6),
            std::make_optional(std::string_view{"this is id 6"}));

  // Try reading zero-length data blobs, just in case.
  ASSERT_EQ(pack.GetStringView(1), std::make_optional(std::string_view{}));
  ASSERT_EQ(pack.GetStringView(10), std::make_optional(std::string_view{}));

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringView(140).has_value());
}

TEST(DataPackTest, LoadFromBufferV5) {
  DataPack pack(k100Percent);

  ASSERT_TRUE(pack.LoadFromBuffer(
      {kSampleCompressPakContentsV5, kSampleCompressPakSizeV5}));

  ASSERT_TRUE(pack.HasResource(4));
  ASSERT_EQ(pack.GetStringView(4),
            std::make_optional(std::string_view{"this is id 4"}));
  ASSERT_TRUE(pack.HasResource(6));
  ASSERT_TRUE(pack.GetStringView(6).has_value());
  ASSERT_TRUE(pack.HasResource(8));
  ASSERT_TRUE(pack.GetStringView(8).has_value());
  ASSERT_EQ(pack.GetStringView(10),
            std::make_optional(std::string_view{"this is id 4"}));

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.HasResource(140));
  ASSERT_FALSE(pack.GetStringView(140).has_value());
}

INSTANTIATE_TEST_SUITE_P(WriteBINARY,
                         DataPackTest,
                         ::testing::Values(DataPack::BINARY));
INSTANTIATE_TEST_SUITE_P(WriteUTF8,
                         DataPackTest,
                         ::testing::Values(DataPack::UTF8));
INSTANTIATE_TEST_SUITE_P(WriteUTF16,
                         DataPackTest,
                         ::testing::Values(DataPack::UTF16));

TEST(DataPackTest, LoadFileWithTruncatedHeader) {
  base::FilePath data_path;
  ASSERT_TRUE(base::PathService::Get(UI_DIR_TEST_DATA, &data_path));
  data_path = data_path.AppendASCII("data_pack_unittest/truncated-header.pak");

  DataPack pack(k100Percent);
  ASSERT_FALSE(pack.LoadFromPath(data_path));
}

TEST_P(DataPackTest, Write) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath file = dir.GetPath().Append(FILE_PATH_LITERAL("data.pak"));

  std::string one("one");
  std::string two("two");
  std::string three("three");
  std::string four("four");
  std::string fifteen("fifteen");

  std::map<uint16_t, std::string_view> resources;
  resources.emplace(1, std::string_view(one));
  resources.emplace(2, std::string_view(two));
  resources.emplace(15, std::string_view(fifteen));
  resources.emplace(3, std::string_view(three));
  resources.emplace(4, std::string_view(four));
  ASSERT_TRUE(DataPack::WritePack(file, resources, GetParam()));

  // Now try to read the data back in.
  DataPack pack(k100Percent);
  ASSERT_TRUE(pack.LoadFromPath(file));
  EXPECT_EQ(pack.GetTextEncodingType(), GetParam());

  ASSERT_EQ(pack.GetStringView(1), std::make_optional(std::string_view{one}));
  ASSERT_EQ(pack.GetStringView(2), std::make_optional(std::string_view{two}));
  ASSERT_EQ(pack.GetStringView(3), std::make_optional(std::string_view{three}));
  ASSERT_EQ(pack.GetStringView(4), std::make_optional(std::string_view{four}));
  ASSERT_EQ(pack.GetStringView(15),
            std::make_optional(std::string_view{fifteen}));

  EXPECT_EQ(5U, pack.GetResourceTableSizeForTesting());
  EXPECT_EQ(0U, pack.GetAliasTableSize());
}

TEST_P(DataPackTest, WriteWithAliases) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath file = dir.GetPath().Append(FILE_PATH_LITERAL("data.pak"));

  std::string one("one");
  std::string two("two");
  std::string three("three");
  std::string four("four");
  std::string fifteen("fifteen");

  std::map<uint16_t, std::string_view> resources;
  resources.emplace(1, std::string_view(one));
  resources.emplace(2, std::string_view(two));
  resources.emplace(15, std::string_view(fifteen));
  resources.emplace(3, std::string_view(three));
  resources.emplace(4, std::string_view(four));
  resources.emplace(10, std::string_view(one));
  resources.emplace(11, std::string_view(three));
  ASSERT_TRUE(DataPack::WritePack(file, resources, GetParam()));

  // Now try to read the data back in.
  DataPack pack(k100Percent);
  ASSERT_TRUE(pack.LoadFromPath(file));
  EXPECT_EQ(pack.GetTextEncodingType(), GetParam());

  ASSERT_EQ(pack.GetStringView(1), std::make_optional(std::string_view{one}));
  ASSERT_EQ(pack.GetStringView(2), std::make_optional(std::string_view{two}));
  ASSERT_EQ(pack.GetStringView(3), std::make_optional(std::string_view{three}));
  ASSERT_EQ(pack.GetStringView(4), std::make_optional(std::string_view{four}));
  ASSERT_EQ(pack.GetStringView(15),
            std::make_optional(std::string_view{fifteen}));
  ASSERT_EQ(pack.GetStringView(10), std::make_optional(std::string_view{one}));
  ASSERT_EQ(pack.GetStringView(11),
            std::make_optional(std::string_view{three}));

  ASSERT_EQ(pack.GetStringView(1)->data(), pack.GetStringView(10)->data());
  ASSERT_EQ(pack.GetStringView(3)->data(), pack.GetStringView(11)->data());

  EXPECT_EQ(5U, pack.GetResourceTableSizeForTesting());
  EXPECT_EQ(2U, pack.GetAliasTableSize());
}

#if BUILDFLAG(IS_POSIX)
TEST(DataPackTest, ModifiedWhileUsed) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath data_path =
      dir.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak file.
  ASSERT_TRUE(
      base::WriteFile(data_path, {kSamplePakContentsV4, kSamplePakSizeV4}));

  base::File file(data_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  // Load the file through the data pack API.
  DataPack pack(k100Percent);
  ASSERT_TRUE(pack.LoadFromFile(std::move(file)));

  ASSERT_TRUE(pack.HasResource(10));
  ASSERT_TRUE(pack.GetStringView(10).has_value());

  ASSERT_TRUE(base::WriteFile(
      data_path, {kSampleCorruptPakContents, kSampleCorruptPakSize}));

  // Reading asset #10 should now fail as it extends past the end of the file.
  ASSERT_TRUE(pack.HasResource(10));
  ASSERT_FALSE(pack.GetStringView(10).has_value());
}
#endif

TEST(DataPackTest, Misordered) {
  DataPack pack(k100Percent);

  ASSERT_FALSE(pack.LoadFromBuffer(
      {kSampleMisorderedPakContents, kSampleMisorderedPakSize}));
}

}  // namespace ui
