// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/data_pack_with_resource_sharing_lacros.h"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace ui {

class DataPackWithResourceSharingTest
    : public testing::TestWithParam<DataPack::TextEncodingType> {
 public:
  DataPackWithResourceSharingTest() {}
};

INSTANTIATE_TEST_SUITE_P(LoadFromPathWithAshResourceBINARY,
                         DataPackWithResourceSharingTest,
                         ::testing::Values(DataPack::BINARY));
INSTANTIATE_TEST_SUITE_P(LoadFromPathWithAshResourceUTF8,
                         DataPackWithResourceSharingTest,
                         ::testing::Values(DataPack::UTF8));
INSTANTIATE_TEST_SUITE_P(LoadFromPathWithAshResourceUTF16,
                         DataPackWithResourceSharingTest,
                         ::testing::Values(DataPack::UTF16));

TEST_P(DataPackWithResourceSharingTest, LoadFromPathWithAshResource) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath lacros_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("lacros_data.pak"));
  base::FilePath ash_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("ash_data.pak"));
  base::FilePath shared_resource_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("shared_resource_data.pak"));

  std::string one("one");
  std::string two("two");
  std::string three("three");
  std::string zero("zero");

  std::map<uint16_t, std::string_view> lacros_resources;
  lacros_resources.emplace(1, std::string_view(one));
  lacros_resources.emplace(2, std::string_view(two));
  lacros_resources.emplace(3, std::string_view(three));
  ASSERT_TRUE(DataPack::WritePack(lacros_file, lacros_resources, GetParam()));

  std::map<uint16_t, std::string_view> ash_resources;
  ash_resources.emplace(1, std::string_view(one));
  ash_resources.emplace(2, std::string_view(zero));
  ash_resources.emplace(4, std::string_view(three));
  ASSERT_TRUE(DataPack::WritePack(ash_file, ash_resources, GetParam()));

  ASSERT_TRUE(DataPackWithResourceSharing::MaybeGenerateFallbackAndMapping(
      ash_file, lacros_file, shared_resource_file, k100Percent));

  DataPackWithResourceSharing pack(k100Percent);
  ASSERT_TRUE(pack.LoadFromPathWithAshResource(shared_resource_file, ash_file));

  ASSERT_EQ(pack.GetStringPiece(1), std::optional<std::string_view>(one));
  ASSERT_EQ(pack.GetStringPiece(2), std::optional<std::string_view>(two));
  ASSERT_EQ(pack.GetStringPiece(3), std::optional<std::string_view>(three));

  EXPECT_EQ(2U, pack.GetMappingTableSizeForTesting());
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(0)->lacros_resource_id,
            1);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(0)->ash_resource_id,
            1);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(1)->lacros_resource_id,
            3);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(1)->ash_resource_id,
            4);
}

TEST_P(DataPackWithResourceSharingTest, LoadFromPathWithAshResourceWithAlias) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath lacros_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("lacros_data.pak"));
  base::FilePath ash_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("ash_data.pak"));
  base::FilePath shared_resource_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("shared_resource_data.pak"));

  std::string one("one");
  std::string two("two");
  std::string three("three");
  std::string zero("zero");

  std::map<uint16_t, std::string_view> lacros_resources;
  lacros_resources.emplace(1, std::string_view(one));
  lacros_resources.emplace(2, std::string_view(two));
  lacros_resources.emplace(3, std::string_view(three));
  // Add resources registered as Alias.
  lacros_resources.emplace(11, std::string_view(one));
  lacros_resources.emplace(12, std::string_view(two));
  lacros_resources.emplace(13, std::string_view(three));
  ASSERT_TRUE(DataPack::WritePack(lacros_file, lacros_resources, GetParam()));

  std::map<uint16_t, std::string_view> ash_resources;
  ash_resources.emplace(1, std::string_view(one));
  ash_resources.emplace(2, std::string_view(zero));
  ash_resources.emplace(4, std::string_view(three));
  ASSERT_TRUE(DataPack::WritePack(ash_file, ash_resources, GetParam()));

  ASSERT_TRUE(DataPackWithResourceSharing::MaybeGenerateFallbackAndMapping(
      ash_file, lacros_file, shared_resource_file, k100Percent));

  DataPackWithResourceSharing pack(k100Percent);
  ASSERT_TRUE(pack.LoadFromPathWithAshResource(shared_resource_file, ash_file));

  ASSERT_EQ(pack.GetStringPiece(1), std::optional<std::string_view>(one));
  ASSERT_EQ(pack.GetStringPiece(2), std::optional<std::string_view>(two));
  ASSERT_EQ(pack.GetStringPiece(3), std::optional<std::string_view>(three));
  ASSERT_EQ(pack.GetStringPiece(11), std::optional<std::string_view>(one));
  ASSERT_EQ(pack.GetStringPiece(12), std::optional<std::string_view>(two));
  ASSERT_EQ(pack.GetStringPiece(13), std::optional<std::string_view>(three));

  EXPECT_EQ(4U, pack.GetMappingTableSizeForTesting());
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(0)->lacros_resource_id,
            1);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(0)->ash_resource_id,
            1);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(1)->lacros_resource_id,
            3);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(1)->ash_resource_id,
            4);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(2)->lacros_resource_id,
            11);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(2)->ash_resource_id,
            1);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(3)->lacros_resource_id,
            13);
  EXPECT_EQ(pack.GetMappingByMappingTableIndexForTesting(3)->ash_resource_id,
            4);
}

TEST_P(DataPackWithResourceSharingTest, LoadMappingTableWithAshIDNotExisting) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath ash_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("ash_data.pak"));
  base::FilePath shared_resource_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("shared_resource_data.pak"));

  std::string one("one");
  std::string zero("zero");

  std::map<uint16_t, std::string_view> ash_resources;
  ash_resources.emplace(1, std::string_view(one));
  ash_resources.emplace(2, std::string_view(zero));
  ASSERT_TRUE(DataPack::WritePack(ash_file, ash_resources, GetParam()));

  using Mapping = DataPackWithResourceSharing::Mapping;
  std::vector<Mapping> mapping_table;
  mapping_table.push_back(Mapping(1, 1));
  mapping_table.push_back(Mapping(2, 3));

  DataPackWithResourceSharing::WriteSharedResourceFileForTesting(
      shared_resource_file, std::move(mapping_table));

  // If mapping_table points to Ash ID which doesn't exist, load should fail.
  DataPackWithResourceSharing pack(k100Percent);
  ASSERT_FALSE(
      pack.LoadFromPathWithAshResource(shared_resource_file, ash_file));
}

TEST_P(DataPackWithResourceSharingTest,
       LoadMappingTableWithDuplicatedLacrosID) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath ash_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("ash_data.pak"));
  base::FilePath shared_resource_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("shared_resource_data.pak"));

  std::string one("one");
  std::string zero("zero");

  std::map<uint16_t, std::string_view> ash_resources;
  ash_resources.emplace(1, std::string_view(one));
  ash_resources.emplace(2, std::string_view(zero));
  ASSERT_TRUE(DataPack::WritePack(ash_file, ash_resources, GetParam()));

  using Mapping = DataPackWithResourceSharing::Mapping;
  std::vector<Mapping> mapping_table;
  mapping_table.push_back(Mapping(1, 1));
  mapping_table.push_back(Mapping(1, 2));

  DataPackWithResourceSharing::WriteSharedResourceFileForTesting(
      shared_resource_file, std::move(mapping_table));

  // If a lacros resource id is mapped to more than 1 ash resource id, it should
  // fail.
  DataPackWithResourceSharing pack(k100Percent);
  ASSERT_FALSE(
      pack.LoadFromPathWithAshResource(shared_resource_file, ash_file));
}

TEST_P(DataPackWithResourceSharingTest, OnFailedToGenerateFile) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath empty_ash_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("ash_data.pak"));
  base::FilePath lacros_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("lacros_data.pak"));
  base::FilePath shared_resource_file =
      dir.GetPath().Append(FILE_PATH_LITERAL("shared_resource_data.pak"));

  std::string one("one");
  std::string two("two");

  std::map<uint16_t, std::string_view> lacros_resources;
  lacros_resources.emplace(1, std::string_view(one));
  lacros_resources.emplace(2, std::string_view(two));
  ASSERT_TRUE(DataPack::WritePack(lacros_file, lacros_resources, GetParam()));

  ASSERT_FALSE(DataPackWithResourceSharing::MaybeGenerateFallbackAndMapping(
      empty_ash_file, lacros_file, shared_resource_file, k100Percent));

  DataPackWithResourceSharing pack(k100Percent);
  ASSERT_FALSE(
      pack.LoadFromPathWithAshResource(shared_resource_file, empty_ash_file));

  EXPECT_FALSE(base::PathExists(shared_resource_file));
}

}  // namespace ui
