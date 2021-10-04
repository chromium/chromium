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

// This tests the generic VBA code extraction API.
//
// See maldoca/ole/vba_extract.h for more details on the
// API. This standalone executable runs all the tests it defines,
// unless it is passed non empty --input_file or --input_files flags,
// in which case the flag values are interpreted as a file (or as a
// CSV list of files) to extract VBA code from.
//
// The md5 checksum, the length and the filename of all code chunks
// are always printed on stdout. When --print_code is provided, the
// extracted VBA code chunks are also printed on stdout. When
// --print_code_short is used, only the first 256 bytes of the VBA
// code chunks are reported. To get more reports on the unexpected
// situations met while parsing the input, run a vba_extract_test
// binary not build with '-c opt'.
//
// Sample invocation:
//
//  $ vba_extract_test --print_code_short --input_file \
//     maldoca/testdata/ole/a2acaccb1a8e08efe439ab2feec8aa32
//
//  md5=59ebb5ecc08ce573d7bf39efd89d16e0 length=1349 filename=/Macros/...
//  --
//  Attribute VB_Name = "ThisDocument"
//  Attribute VB_Base = "1Normal.ThisDocument"
//  Attribute VB_GlobalNameSpace = False
//  Attribute VB_Creatable = False
//  Attribute VB_PredeclaredId = True
//  Attribute VB_Exposed = True
//  Attribute VB_TemplateDerived = True
//  Attri...
//  --
//
// Vector initialization carefully laid out for readibility, do not
// clang-format.
// clang-format off

#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_format.h"

#ifndef MALDOCA_CHROME
#include "benchmark/benchmark.h"
#endif  // MALDOCA_CHROME

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/ole/proto/extract_vba_settings.pb.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/status.h"
#include "maldoca/ole/oss_utils.h"
#include "maldoca/ole/vba_extract.h"

using ::maldoca::ExtractVBAFromFile;
using ::maldoca::LikelyOLE2WithVBAContent;
using ::maldoca::OLEDirectoryMessage;
using ::maldoca::VBACodeChunks;
using ::maldoca::ExtractVBAFromStringWithSettings;

ABSL_FLAG(std::string, input_file, "",
              "A file to extract VBA code chunks from");
ABSL_FLAG(std::string, input_files, "",
              "A CSV list of files to extract VBA code chunks from");
ABSL_FLAG(std::string, input_files_list, "",
              "A file containing a list of files to extract VBA code "
              "chunks from");
ABSL_FLAG(bool, print_code, false,
            "When set to true, print the code of each VBA code chunk");
ABSL_FLAG(bool, print_code_short, false,
            "When set to true, print the first 256 bytes each VBA code chunk");
ABSL_FLAG(std::string, save_code_to, "",
              "When specified, a directory to save the extracted code chunks "
              "to. The name of the extracted chunks is their MD5 checksums.");

namespace {
std::string TestFilename(absl::string_view filename) {
  return maldoca::testing::OleTestFilename(filename, "ole/");
}

std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status = maldoca::testing::GetTestContents(TestFilename(filename), &content);
  EXPECT_TRUE(status.ok()) << status;
  return content;
}

#ifndef MALDOCA_CHROME
void ExtractAndPrintCode(const std::string& filename) {
  VBACodeChunks chunks;
  std::string error;
  ExtractVBAFromFile(filename, &chunks, &error);
  if (chunks.chunk_size() != 0) {
    std::cout << filename << ":" << std::endl;
  }
  // First report any code we might have found.
  for (const auto& chunk : chunks.chunk()) {
    std::string digest = maldoca::Md5HexString(chunk.code());
    // Print some information about the extracted code chunk.
    std::cout << "md5=" << digest
              << " length=" << chunk.code().length() << " filename="
              << maldoca::file::JoinPathRespectAbsolute(chunk.path(),
              chunk.filename())
              << std::endl;
    std::cout << "--" << std::endl;

    // If required, print the code chunk on stdout.
    if (absl::GetFlag(FLAGS_print_code) ||
        absl::GetFlag(FLAGS_print_code_short)) {
      size_t length = (absl::GetFlag(FLAGS_print_code_short) ? 256 :
                       std::string::npos);
      std::cout << chunk.code().substr(0, length);
      if (absl::GetFlag(FLAGS_print_code_short)) {
        std::cout << "...";
      }
      std::cout << std::endl << "--" << std::endl;
    }

    // If required, save the code chunk using its md5 checksum as its
    // name into the specified directory.
    if (!absl::GetFlag(FLAGS_save_code_to).empty()) {
      std::string filename = maldoca::file::JoinPathRespectAbsolute(
          absl::GetFlag(FLAGS_save_code_to), digest);
      MALDOCA_CHECK_OK(maldoca::file::SetContents(filename, chunk.code()));
    }
  }
  // And then report errors we might have encountered trying to extract
  // the code.
  if (!error.empty()) {
    LOG(ERROR) << error;
  }
  // Explicitly print out that nothing was found
  if (chunks.chunk_size() == 0 && error.empty()) {
    std::cout << filename << ": no VBA found";
  }
}
#endif  // MALDOCA_CHROME

TEST(VBAPresenceDetection, VBAPresenceDetectionTest) {
  // These are certified OLE2/Docfiles.
  std::string content = GetTestContent("ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_0x42_encoded");
  EXPECT_TRUE(LikelyOLE2WithVBAContent(content));

  // OOXML, Office 2003 XML and straight MSO file content (not straight
  // OLE2/Docfile content.)
  content = GetTestContent("c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_0x42_encoded");
  EXPECT_FALSE(LikelyOLE2WithVBAContent(content));
}

TEST(BogusExtraction, BogusExtractionTest) {
  std::string error, content;
  VBACodeChunks code_chunks;

  // The file doesn't exist.
  ExtractVBAFromFile("/does/not/exist", &code_chunks, &error);
  EXPECT_EQ(code_chunks.chunk_size(), 0);
  EXPECT_EQ(error, "Can not get content for '/does/not/exist'");

  // error is reset prior to being set again. Test this assumption by
  // trying a successful extraction.
  ExtractVBAFromFile(TestFilename(
    "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_0x42_encoded"),
                     &code_chunks, &error, true);
  EXPECT_TRUE(error.empty());
  EXPECT_NE(code_chunks.chunk_size(), 0);
  code_chunks.Clear();

  // Attempt to parse something that is neither a Docfile, OOXML nor
  // Office 2003 XML.
  std::string not_a_docfile = "not a doc file";
#ifndef MALDOCA_CHROME
  std::string temp = maldoca::file::CreateTempFileAndCloseOrDie(
      maldoca::file::TestTempDir(), "not a doc file");
  ExtractVBAFromFile(temp, &code_chunks, &error);
  EXPECT_EQ(error, absl::StrCat(temp, ": Unrecognized file format"));
#else
  ExtractVBAFromString(not_a_docfile, &code_chunks, &error);
#endif  // MALDOCA_CHROME
  EXPECT_EQ(code_chunks.chunk_size(), 0);
  ExtractVBAFromString(not_a_docfile, &code_chunks, &error);
  EXPECT_EQ(error, "Unrecognized file format");

  // OLE header but bogus content
  ExtractVBAFromFile(TestFilename("bogus_ole"), &code_chunks, &error);
  EXPECT_EQ(error, absl::StrCat(TestFilename("bogus_ole"), ": FAT is empty"));
  content = GetTestContent("bogus_ole");
  ExtractVBAFromString(content, &code_chunks, &error);
  EXPECT_EQ(error, "FAT is empty");

  // Below are OOXML files.
  // Bogus OOXML content
  ExtractVBAFromFile(TestFilename("bogus_ooxml"), &code_chunks, &error);
  EXPECT_EQ(error, absl::StrCat(TestFilename("bogus_ooxml"), ": ",
                          "bogus_ole2.bin: FAT is empty, "
                          "bogus_ole.bin: FAT is empty"));
  content = GetTestContent("bogus_ooxml");
  ExtractVBAFromString(content, &code_chunks, &error);
  EXPECT_EQ(error, "bogus_ole2.bin: FAT is empty, bogus_ole.bin: FAT is empty");
}

TEST(MultipleExtraction, MultipleExtractionTest) {
  // This maps a input filename (identified by its md5 checksum) to
  // the md5 checksum of the VBA code chunks it contains. This ground
  // truth data was obtained independently.
  std::map<std::string, std::vector<std::string>> input_to_results_map = {
    // These are plain Docfiles.
    {"ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_0x42_encoded",
                                         {"c598c45b0d9d3090599ff1df77c5d612",
                                          "46d8fa3e4d042579f074a60553029ff5"}},
    // Word97 file with very large VBA
    {"0d21ac394df6857ff203e456ed2385ee", {"794334f8dbc7d478108d32b7c2fa520d"}},
    // These are OOXML (.docx) files
    {"c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_0x42_encoded",
                                        {"f2677ec038ddbb21b2dd64bce3f9bf62",
                                        "e68c04ceca0173bf179ab351e8878936",
                                        "c068cedb2dc37e55f6f3dc5b5add4ccf",
                                        "96cb36a5e07b5f4598711afb78f47840",
                                        "bd25f32182eee73a0740de5796c1580e",
                                        "678229ad796b67b6e97e2048e756ae6c",
                                        "7d3469499fe4d1180581a17c503eb371",
                                        "ecbd879b95cb9b4139e031554f701e7d",
                                        "0410ff67b2a2d132686ea44e91718c7f",
                                        "19f832785dd4edf766b9356b42e430e9",
                                        "bff914c5373d3cc7c06666daaf86736c",
                                        "47b9b103285db9d77524e5937bb9679b"}},
    // PowerPoint 97
    {"95dc7b31c9ba45a066f580e6e2c5c914", {"ec2c9b2abab04ede4199963e6a38bb71"}},
  };
  for (auto const& item : input_to_results_map) {
    VBACodeChunks chunks, chunks_string;
    std::string error, error_string, content;
    LOG(INFO) << "Processing testdata/ole/" << item.first;
    std::string filename = TestFilename(item.first);
    content = GetTestContent(item.first);
    // Both extraction methods, when successful, should return the
    // exact same thing.
    bool xor_decode_file = absl::StrContains(item.first, kFileNameXorEncodingIndicator) && !absl::StrContains(item.first, kFileNameTextprotoIndicator);
    ExtractVBAFromFile(filename, &chunks, &error, xor_decode_file);
    ExtractVBAFromString(content, &chunks_string, &error_string);
    EXPECT_THAT(error, testing::StrEq(""));
    EXPECT_THAT(error_string, testing::StrEq(""));
    EXPECT_NE(chunks.chunk_size(), 0);
    EXPECT_NE(chunks_string.chunk_size(), 0);
    EXPECT_EQ(chunks.chunk_size(), item.second.size());
    EXPECT_EQ(chunks_string.chunk_size(), item.second.size());
    for (int i = 0; i < chunks.chunk_size(); ++i) {
      EXPECT_EQ(maldoca::Md5HexString(chunks.chunk(i).code()),
        item.second[i]);
      EXPECT_EQ(maldoca::Md5HexString(chunks_string.chunk(i).code()),
        item.second[i]);
    }
  }
}

// When a small enough input is used and when we consider it might be
// a MSO file, we detect that we can't read past the end of the
// content and bail. Make sure that when we do that, we free some of
// the memory that creating the z_stream has allocated.
TEST(BugFixesVerificationTest, NoMemoryLeak) {
  std::string error;
  VBACodeChunks code_chunks;

  // The input needs to be larger than 32 bytes but smaller than the
  // largest constant used in MSOContent:GetOLE2Data or smaller than
  // what you read at byte 30 + 0x46, here 0xaa + 0x46 = 240.
  ExtractVBAFromString(std::string(100, 'a'), &code_chunks, &error);
  EXPECT_EQ(code_chunks.chunk_size(), 0);
  EXPECT_EQ(error, "Unrecognized file format");
}

// Test that with the lightweight API, we fail extracting VBA content
// from OOXML input but don't use the regular API.
TEST(LightweightAPI, LightweightAPITest) {
  std::string content = GetTestContent(
      "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_0x42_encoded");

  std::string error;
  VBACodeChunks code_chunks;
  ExtractVBAFromString(content, &code_chunks, &error);
  EXPECT_EQ(code_chunks.chunk_size(), 12);
  EXPECT_TRUE(error.empty());

  code_chunks.Clear();
  error.clear();
  ExtractVBAFromStringLightweight(content, &code_chunks, &error);
  EXPECT_EQ(code_chunks.chunk_size(), 0);
  EXPECT_EQ(error, "Unrecognized file format");
}

// Test that for ExtractVBAWithSettings, we fail sucessfully extract content
// from OOXML input using the default extraction type.
TEST(ExtractVBAWithSettings, ExtractVBAWithSettingsDefaultTest) {
  std::string content = GetTestContent(
      "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_0x42_encoded");
  maldoca::vba::ExtractVBASettings settings;
  settings.set_extraction_method(maldoca::vba::EXTRACTION_TYPE_DEFAULT);

  auto status_or_vba = ExtractVBAFromStringWithSettings(content, settings);
  EXPECT_EQ(status_or_vba.value().chunk_size(), 12);
}

#ifndef MALDOCA_CHROME
// Test that with the lightweight API, we fail extracting VBA content
// from OOXML input but don't use the regular API.
TEST(ExtractVBAWithSettings, ExtractVBAWithSettingsLightweightTest) {
  std::string content = GetTestContent(
      "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_0x42_encoded");
  maldoca::vba::ExtractVBASettings settings;
  settings.set_extraction_method(maldoca::vba::EXTRACTION_TYPE_LIGHTWEIGHT);
  auto status_or_vba  = ExtractVBAFromStringWithSettings(content, settings);
  EXPECT_THAT(status_or_vba, testing::status::StatusIs(
                                   testing::Eq(absl::StatusCode::kInternal),
                                   testing::Eq("Unrecognized file format")));
}
#endif  // MALDOCA_CHROME

// Test that we can properly extract the directory from OLE content.
TEST(DirectoryExtraction, DirectoryExtractionTest) {
  std::string content = GetTestContent(
      "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_0x42_encoded");
  OLEDirectoryMessage directory;
  VBACodeChunks chunks;
  std::string error;
  ExtractDirectoryAndVBAFromString(content, &directory, &chunks, &error);
  for (int i = 0; i < chunks.chunk_size(); i++) {
    EXPECT_FALSE(chunks.chunk(i).extracted_from_malformed_entry());
  }
  EXPECT_TRUE(error.empty());
  std::string expected_directory_input = R"(
      entries {
        name: "Root Entry"
        children {
          name: "1Table"
        }
        children {
          name: " CompObj"
        }
        children {
          name: "Macros"
          children {
            name: "VBA"
            children {
              name: "dir"
            }
            children {
              name: "NewMacros"
            }
            children {
              name: "_VBA_PROJECT"
            }
            children {
              name: "ThisDocument"
            }
          }
          children {
            name: "PROJECTwm"
          }
          children {
            name: "PROJECT"
          }
        }
        children {
          name: "WordDocument"
        }
        children {
          name: " DocumentSummaryInformation"
        }
        children {
          name: " SummaryInformation"
        }
      })";
  OLEDirectoryMessage directory_expected;
  CHECK(google::protobuf::TextFormat::ParseFromString(expected_directory_input,
                                            &directory_expected));
  EXPECT_EQ(directory.DebugString(), directory_expected.DebugString());

  // Checking that the merge operation works the way we want it to work...
  std::string added_directory_input =
      "entries {\n"
      "  name: \"Root Entry\"\n"
      "  children {\n"
      "    name: \"Data\"\n"
      "  }\n"
      "}\n";
  OLEDirectoryMessage dir1, dir2;
  CHECK(google::protobuf::TextFormat::ParseFromString(added_directory_input, &dir1));
  CHECK(google::protobuf::TextFormat::ParseFromString(added_directory_input, &dir2));
  dir1.MergeFrom(dir2);
  CHECK_EQ(dir1.DebugString(),
           absl::StrCat(added_directory_input, added_directory_input));
}
}  // namespace

#ifndef MALDOCA_CHROME
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  maldoca::InitLogging();
  if (!absl::GetFlag(FLAGS_input_files_list).empty() ||
      !absl::GetFlag(FLAGS_input_file).empty() ||
      !absl::GetFlag(FLAGS_input_files).empty()) {
    // When either --print_code or --print_code_short is set, the value
    // of the flags are mutually exclusive.
    if (absl::GetFlag(FLAGS_print_code) ||
        absl::GetFlag(FLAGS_print_code_short)) {
      CHECK(!absl::GetFlag(FLAGS_print_code) !=
            !absl::GetFlag(FLAGS_print_code_short))
          << ": --print_code and --print_code_short are "
          << "mutually exclusive when set";
    }
    if (!absl::GetFlag(FLAGS_input_file).empty()) {
      ExtractAndPrintCode(absl::GetFlag(FLAGS_input_file));
    }

    std::vector<std::string> files;
    if (!absl::GetFlag(FLAGS_input_files).empty()) {
      files = absl::StrSplit(absl::GetFlag(FLAGS_input_files), ',');
    }

    if (!absl::GetFlag(FLAGS_input_files_list).empty()) {
      std::string content;
      CHECK(maldoca::testing::GetTestContents(absl::GetFlag(FLAGS_input_files_list),
                                 &content).ok());
      files = absl::StrSplit(content, '\n');
    }

    for (const auto& file : files) {
      ExtractAndPrintCode(file);
    }
  } else {
    ::benchmark::RunSpecifiedBenchmarks();
    return RUN_ALL_TESTS();
  }
}
#else
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
#endif  // MALDOCA_CHROME
