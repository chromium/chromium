// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Tests for the VBACodeChunk class methods.

#include "maldoca/ole/vba.h"

#include <memory>

#include "benchmark/benchmark.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/node_hash_map.h"
#include "maldoca/base/file.h"
#include "maldoca/base/status.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/fat.h"
#include "maldoca/ole/header.h"
#include "maldoca/ole/stream.h"

using ::maldoca::DirectoryStorageType;
using ::maldoca::FAT;
using ::maldoca::OLEDirectoryEntry;
using ::maldoca::OLEHeader;
using ::maldoca::OLEStream;
using ::maldoca::VBACodeChunks;
using ::maldoca::vba_code::ExtractVBA2;
using ::maldoca::vba_code::InsertPathPrefix;
using ::maldoca::vba_code::ParseCodeModules;

namespace {
std::string TestFilename(absl::string_view filename) {
  return maldoca::testing::OleTestFilename(filename);
}

std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status =
      maldoca::testing::GetTestContents(TestFilename(filename), &content);
  MALDOCA_CHECK_OK(status) << status;
  return content;
}

class VBATest : public testing::Test {
 protected:
  void SetUp() override {
    content = GetTestContent("vba1_xor_0x42_encoded.bin");
    EXPECT_TRUE(OLEHeader::ParseHeader(content, &header));
    EXPECT_TRUE(header.IsInitialized());
    EXPECT_TRUE(FAT::Read(content, header, &fat));
    std::vector<OLEDirectoryEntry *> dir_entries;
    std::string directory_stream;
    EXPECT_TRUE(OLEDirectoryEntry::ReadDirectory(
        content, header, fat, &root, &dir_entries, &directory_stream));
    EXPECT_TRUE(root.IsInitialized());
    vba_root = root.FindVBAContentRoot();
    EXPECT_NE(vba_root, nullptr);
    continue_extraction_ = false;
  }

  std::string content;
  OLEHeader header;
  std::vector<uint32_t> fat;
  OLEDirectoryEntry root;
  OLEDirectoryEntry *vba_root;
  bool continue_extraction_;
};

// Test catching errors parsing code modules input.
TEST(SimpleVBATest, CodeModulesParsingTestErrors) {
  absl::node_hash_map<std::string, std::string> code_modules;

  // Some input is needed for parsing to happen.
  std::string empty;
  ASSERT_DEATH(ParseCodeModules(empty, &code_modules),
               "Check failed: !project_stream\\.empty\\(\\)");

  // Various parsing attempts with bogus input. The method declares success
  // when it has found at least one entry.
  EXPECT_FALSE(ParseCodeModules("\n\n", &code_modules));
  EXPECT_FALSE(ParseCodeModules("foo=bar", &code_modules));
  EXPECT_FALSE(ParseCodeModules("abc\n123\n", &code_modules));
  EXPECT_FALSE(ParseCodeModules("abc=\n123=\n", &code_modules));
  EXPECT_FALSE(ParseCodeModules("=abc\n=123\n", &code_modules));
  EXPECT_FALSE(ParseCodeModules("=abc\n123=\n", &code_modules));

  // We're expecting Document=xxx/yyy and retain only xxx. Test that not having
  // xxx is properly handled.
  EXPECT_FALSE(ParseCodeModules("Document=/abc", &code_modules));

  // You can't initialize code_modules twice.
  code_modules.insert(std::make_pair("key", "value"));
  ASSERT_DEATH(ParseCodeModules("foo=bar", &code_modules),
               "Check failed: code_modules->empty\\(\\)");
}

TEST(SimpleVBATest, CodeModulesParsingTestOK) {
  std::string input =
      ("Document=abc/def=foobar\n"
       "# Nothing here\n"
       "Module=module1\n"
       "# Nothing here either=derail\n"
       "Class=class1\n"
       "BaseClass=baseclass1\n");
  absl::node_hash_map<std::string, std::string> code_modules;
  EXPECT_TRUE(ParseCodeModules(input, &code_modules));
  EXPECT_EQ(code_modules.find("abc")->second, "cls");
  EXPECT_EQ(code_modules.find("module1")->second, "bas");
  EXPECT_EQ(code_modules.find("class1")->second, "cls");
  EXPECT_EQ(code_modules.find("baseclass1")->second, "frm");
  EXPECT_EQ(code_modules.size(), 4);
}

// Check expectations on vba_code::ExtractVBA input.
TEST_F(VBATest, CodeExtractionInitialFailures) {
  absl::node_hash_map<std::string, std::string> code_modules = {{"foo", "bar"}};
  VBACodeChunks chunks;

  // No empty input
  absl::flat_hash_set<uint32_t> extracted_indices;
  std::vector<OLEDirectoryEntry *> dir_entries;
  ASSERT_DEATH(ExtractVBA2("", header, code_modules, *vba_root, fat, "abcdef",
                           &extracted_indices, &dir_entries, &chunks,
                           continue_extraction_),
               "Check failed: !main_input_string\\.empty\\(\\)");
  extracted_indices.clear();
  dir_entries.clear();
  ASSERT_DEATH(ExtractVBA2("abcdef", header, code_modules, *vba_root, fat, "",
                           &extracted_indices, &dir_entries, &chunks,
                           continue_extraction_),
               "Check failed: !dir_input_string\\.empty\\(\\)");

  // No non-empty code chunk.
  chunks.add_chunk();
  extracted_indices.clear();
  dir_entries.clear();
  ASSERT_DEATH(ExtractVBA2("abcdef", header, code_modules, *vba_root, fat,
                           "abcdef", &extracted_indices, &dir_entries, &chunks,
                           continue_extraction_),
               "Check failed: code_chunks->chunk_size\\(\\) == 0");
}

// Just testing failures to produce expected values reading the
// PROJECTSYSKIND record - provides basic testing coverage for the
// READ_* macros only defined in vba.cc
TEST_F(VBATest, CodeExtractionErrors) {
  absl::node_hash_map<std::string, std::string> code_modules = {{"foo", "bar"}};
  VBACodeChunks chunks;
  absl::flat_hash_set<uint32_t> extracted_indices;
  std::vector<OLEDirectoryEntry *> dir_entries;
  // Failing syskind_id
  EXPECT_FALSE(ExtractVBA2("aaaaaaaaaaaaaaaaaaaaaaaa", header, code_modules,
                           *vba_root, fat, std::string("\x00\x00\x00", 3),
                           &extracted_indices, &dir_entries, &chunks,
                           continue_extraction_));

  extracted_indices.clear();
  dir_entries.clear();
  // Failing syskind_size expected value.
  EXPECT_FALSE(ExtractVBA2(
      "aaaaaaaaaaaaaaaaaaaaaaaa", header, code_modules, *vba_root, fat,
      std::string("\x01\x00\xff\xff\xff\xff", 6), &extracted_indices,
      &dir_entries, &chunks, continue_extraction_));

  extracted_indices.clear();
  dir_entries.clear();
  // Failing syskind_kind expected value.
  EXPECT_FALSE(ExtractVBA2(
      "aaaaaaaaaaaaaaaaaaaaaaaa", header, code_modules, *vba_root, fat,
      std::string("\x01\x00\x04\x00\x00\x00\xff\xff\xff\xff", 10),
      &extracted_indices, &dir_entries, &chunks, continue_extraction_));
}

// Complete extraction of some VBA code.
TEST_F(VBATest, CodeExtractionSuccess) {
  // First build the code modules map.
  OLEDirectoryEntry *project_dir =
      vba_root->FindChildByName("project", DirectoryStorageType::Stream);
  EXPECT_NE(project_dir, nullptr);
  std::string project_content;
  EXPECT_TRUE(OLEStream::ReadDirectoryContent(content, header, *project_dir,
                                              fat, &project_content));
  absl::node_hash_map<std::string, std::string> code_modules;
  EXPECT_TRUE(ParseCodeModules(project_content, &code_modules));

  // Find the VBA/dir entry, read its content and decompress it.
  OLEDirectoryEntry *vba_dir =
      vba_root->FindChildByName("vba", DirectoryStorageType::Storage)
          ->FindChildByName("dir", DirectoryStorageType::Stream);
  EXPECT_NE(vba_dir, nullptr);
  std::string compressed_vba_dir_content;
  EXPECT_TRUE(OLEStream::ReadDirectoryContent(content, header, *vba_dir, fat,
                                              &compressed_vba_dir_content));
  std::string expanded_vba_dir_content;
  EXPECT_TRUE(OLEStream::DecompressStream(compressed_vba_dir_content,
                                          &expanded_vba_dir_content));

  // Extract the code chunks from the expanded VBA/dir entry.
  VBACodeChunks code_chunks;
  absl::flat_hash_set<uint32_t> extracted_indices;
  std::vector<OLEDirectoryEntry *> dir_entries;
  EXPECT_TRUE(ExtractVBA2(content, header, code_modules, *vba_root, fat,
                          expanded_vba_dir_content, &extracted_indices,
                          &dir_entries, &code_chunks, continue_extraction_));

  // Test our expectations against an independently verified checksum value.
  EXPECT_EQ(code_chunks.chunk_size(), 1);
  EXPECT_EQ(code_chunks.chunk(0).sha256_hex_string(),
            "93641fdf83cd2ffb8a3612d39afc6709cd854006f1e09b10bedc91c80a312fe1");

  // Test prepending the path with an other.
  std::string current_path = code_chunks.chunk(0).path();
  InsertPathPrefix("foobar", code_chunks.mutable_chunk(0));
  EXPECT_EQ(code_chunks.chunk(0).path(), "foobar:" + current_path);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MALDOCA_CHROME
  // mini_chromium needs InitLogging
  maldoca::InitLogging();
#else
  ::benchmark::RunSpecifiedBenchmarks();
#endif
  return RUN_ALL_TESTS();
}
