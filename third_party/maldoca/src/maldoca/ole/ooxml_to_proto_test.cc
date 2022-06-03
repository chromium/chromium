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

#include "maldoca/ole/ooxml_to_proto.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "libxml/parser.h"
#include "maldoca/base/file.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/ole/proto/ooxml_to_proto_settings.pb.h"
#include "maldoca/service/common/utils.h"

namespace maldoca {
namespace {

using ::maldoca::ooxml::OoxmlToProtoSettings;

std::string TestFilename(absl::string_view filename) {
  return maldoca::testing::OleTestFilename(filename, "ooxml/");
}

std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status =
      maldoca::testing::GetTestContents(TestFilename(filename), &content);
  MALDOCA_EXPECT_OK(status) << status;
  return content;
}

StatusOr<maldoca::ooxml::OOXMLFile> TryParseOOXMLFile(
    std::string filename, const OoxmlToProtoSettings& settings) {
  return GetOOXMLFileProto(GetTestContent(filename), settings);
}

class OOXMLToProtoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::testing::Test::SetUp();
    status_or_proto_ =
        TryParseOOXMLFile("sample_document.docm",
                          maldoca::utils::GetDefaultOoxmlToProtoSettings());
  }

  StatusOr<maldoca::ooxml::OOXMLFile> status_or_proto_;
};

// Tests if the status_or_proto_ object has the status ok, meaning
// it successfully extracted the proto message.
TEST_F(OOXMLToProtoTest, HasOkStatus) { ASSERT_TRUE(status_or_proto_.ok()); }

// Tests if the extracted proto has at least one file entry (meaning it
// found at least one entry in the archive).
TEST_F(OOXMLToProtoTest, ContainsFiles) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  ASSERT_GT(ooxml.entries_size(), 0);
}

// Tests if some specific files are extracted. Those files are mandatory
// in any valid ooxml file, so if they aren't extracted then the extraction
// isn't successful.
TEST_F(OOXMLToProtoTest, ContainsSpecificFiles) {
  absl::flat_hash_set<std::string> files = {
      "[Content_Types].xml",
      "_rels/.rels",
      "word/theme/theme1.xml",
  };
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  for (const auto& file : ooxml.entries()) {
    files.erase(file.file_name());
  }

  EXPECT_TRUE(files.empty());
}

// Tests if the extracted proto has at least one OLE entry (meaning it
// found at least one OLE entry in the archive).
TEST_F(OOXMLToProtoTest, ContainsOleFiles) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  ASSERT_GT(ooxml.ole_entries_size(), 0);
}

// Tests if OLE entries have all fields.
TEST_F(OOXMLToProtoTest, ContainsValidOleFiles) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  for (auto& ole_entry : ooxml.ole_entries()) {
    EXPECT_TRUE(ole_entry.has_ole_content());
    EXPECT_TRUE(ole_entry.has_filename());
    EXPECT_TRUE(ole_entry.has_hash());
    EXPECT_TRUE(ole_entry.has_filesize());
  }
}

// Tests if at least one VBA entry is found.
TEST_F(OOXMLToProtoTest, ExtractsVBACode) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  bool found_vba = false;
  for (const auto& ole_entry : ooxml.ole_entries()) {
    ASSERT_TRUE(ole_entry.has_ole_content());
    const auto& ole_file = ole_entry.ole_content();
    const auto& vba_code_chunks = ole_file.vba_code();

    found_vba |= (vba_code_chunks.chunk_size() > 0);
  }

  EXPECT_TRUE(found_vba);
}

// Tests if at least one relationship is found.
TEST_F(OOXMLToProtoTest, FindsRelationships) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  EXPECT_GT(ooxml.relationships_size(), 0);
}

// Tests if the relationships have all attributes.
TEST_F(OOXMLToProtoTest, RelationshipsAreValid) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  for (const auto& rel : ooxml.relationships()) {
    EXPECT_TRUE(rel.has_id());
    EXPECT_TRUE(rel.has_type());
    EXPECT_TRUE(rel.has_target());
  }
}

// Tests if some mandatory relationships exist.
TEST_F(OOXMLToProtoTest, SpecificRelationshipsExist) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  absl::flat_hash_set<std::string> files = {
      "docProps/core.xml",
      "docProps/app.xml",
  };
  for (const auto& rel : ooxml.relationships()) {
    if (rel.has_target()) files.erase(rel.target());
  }

  EXPECT_TRUE(files.empty());
}

// Tests if `PropertySetStream protos` are correctly initialized.
TEST_F(OOXMLToProtoTest, PropertySetStreamIsInitialized) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  // The `PropertySetStream` protos have exactly one `PropertySet`.
  ASSERT_EQ(ooxml.summary_information().property_set_size(), 1);
  ASSERT_EQ(ooxml.document_summary_information().property_set_size(), 1);
  // Each `PropertySet` has exactly one `Dictionary`.
  ASSERT_EQ(ooxml.summary_information().property_set(0).dictionary_size(), 1);
  ASSERT_EQ(
      ooxml.document_summary_information().property_set(0).dictionary_size(),
      1);
}

// Tests if the properties have an id, a type and a value.
TEST_F(OOXMLToProtoTest, PropertiesAreValid) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  auto Test = [&](const ole::PropertySetStream& prop_set_stream) {
    for (const auto& property : prop_set_stream.property_set(0).property()) {
      EXPECT_TRUE(property.has_type());
      ASSERT_TRUE(property.has_value());
      ASSERT_TRUE(property.has_property_id());
    }
  };
  Test(ooxml.summary_information());
  Test(ooxml.document_summary_information());
}

// Tests if at least some properties are extracted.
TEST_F(OOXMLToProtoTest, PropertiesAreExtracted) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  EXPECT_GT(ooxml.summary_information().property_set(0).property_size(), 0);
  EXPECT_GT(
      ooxml.document_summary_information().property_set(0).property_size(), 0);
}

// Tests if each property has a unique property_id, and it matches exactly one
// dictionary entry.
TEST_F(OOXMLToProtoTest, PropertiesHaveValidIDs) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();
  absl::flat_hash_set<int> properties_id;

  // Add all the property_ids.
  for (const auto& property :
       ooxml.summary_information().property_set(0).property()) {
    LOG(ERROR) << "property id: " << property.property_id();
    EXPECT_TRUE(properties_id.find(property.property_id()) ==
                properties_id.end());
    properties_id.insert(property.property_id());
  }
  for (const auto& property :
       ooxml.document_summary_information().property_set(0).property()) {
    EXPECT_TRUE(properties_id.find(property.property_id()) ==
                properties_id.end());
    properties_id.insert(property.property_id());
  }

  // Remove all the ids in the dictionaries.
  for (const auto& dict_entry :
       ooxml.summary_information().property_set(0).dictionary(0).entry()) {
    EXPECT_TRUE(properties_id.find(dict_entry.property_id()) !=
                properties_id.end());
    properties_id.erase(dict_entry.property_id());
  }
  for (const auto& dict_entry : ooxml.document_summary_information()
                                    .property_set(0)
                                    .dictionary(0)
                                    .entry()) {
    EXPECT_TRUE(properties_id.find(dict_entry.property_id()) !=
                properties_id.end());
    properties_id.erase(dict_entry.property_id());
  }
  EXPECT_TRUE(properties_id.empty());
}

// Tests if each property's VtValue has exactly one entry.
TEST_F(OOXMLToProtoTest, PropertiesHaveOneValue) {
  const ooxml::OOXMLFile& ooxml = status_or_proto_.value();

  auto Test = [&](const ole::PropertySetStream& prop_set_stream) {
    for (const auto& property : prop_set_stream.property_set(0).property()) {
      ASSERT_TRUE(property.has_value());
      const auto& value = property.value();
      int attributes_count = 0;
      attributes_count += value.has_uint();
      attributes_count += value.has_int_();
      attributes_count += value.has_boolean();
      attributes_count += value.has_str();
      attributes_count += value.has_blob();
      attributes_count += value.has_vector();
      EXPECT_EQ(attributes_count, 1);
      if (value.has_blob()) {
        EXPECT_TRUE(value.has_blob_hash());
      }
    }
  };

  Test(ooxml.summary_information());
  Test(ooxml.document_summary_information());
}

TEST(OOXMLDocumentParsing, InvalidFileIsNotParsed) {
  auto status_or_proto =
      TryParseOOXMLFile("sample_document_invalid_archive.docx",
                        maldoca::utils::GetDefaultOoxmlToProtoSettings());
  ASSERT_FALSE(status_or_proto.ok());
}

// Tests whether OOXML content was correctly extracted.
TEST(ExtractOoxml, ParserExtractsAllFeatures) {
  const StatusOr<office::ParserOutput> status_or = GetOoxmlParserOutputProto(
      GetTestContent("sample_document.docm"),
      maldoca::utils::GetDefaultOoxmlToProtoSettings());
  const office::ParserOutput parser_output = status_or.value();
  EXPECT_EQ(parser_output.structure_features().ooxml().entries_size(), 13);
  EXPECT_EQ(
      parser_output.script_features().scripts()[0].vba_code().chunk_size(), 1);
  EXPECT_EQ(parser_output.metadata_features()
                .ooxml_metadata_features()
                .relationships_size(),
            9);
  EXPECT_EQ(parser_output.metadata_features()
                .summary_information()
                .property_set_size(),
            1);
  EXPECT_EQ(parser_output.metadata_features()
                .document_summary_information()
                .property_set_size(),
            1);
}

TEST(ExtractOoxml, InvalidFileIsNotParsed) {
  const StatusOr<office::ParserOutput> status_or = GetOoxmlParserOutputProto(
      GetTestContent("sample_document_invalid_archive.docx"),
      maldoca::utils::GetDefaultOoxmlToProtoSettings());
  EXPECT_FALSE(status_or.ok());
}

TEST(ExtractOoxml, ExtractsHyperlinks) {
  auto ooxml =
      TryParseOOXMLFile("hyperlink_relationship.docx",
                        maldoca::utils::GetDefaultOoxmlToProtoSettings());
  MALDOCA_EXPECT_OK(ooxml);
  bool has_google_hyperlink = false;
  for (const auto& relationship : ooxml->relationships()) {
    if (relationship.target() == "https://www.google.com/") {
      has_google_hyperlink = true;
    }
  }
  EXPECT_TRUE(has_google_hyperlink);
}

TEST(ExtractOoxml, TestParserConfig) {
  OoxmlToProtoSettings settings =
      maldoca::utils::GetDefaultOoxmlToProtoSettings();

  auto ooxml = TryParseOOXMLFile("sample_document.docm", settings);
  MALDOCA_EXPECT_OK(ooxml);
  EXPECT_FALSE(ooxml->summary_information().property_set().empty());
  EXPECT_FALSE(ooxml->document_summary_information().property_set().empty());
  EXPECT_FALSE(ooxml->ole_entries().empty());
  EXPECT_FALSE(
      ooxml->ole_entries().at(0).ole_content().vba_code().chunk().empty());
  EXPECT_FALSE(ooxml->entries().empty());
  EXPECT_FALSE(ooxml->relationships().empty());

  settings.set_include_metadata(false);
  ooxml = TryParseOOXMLFile("sample_document.docm", settings);
  MALDOCA_EXPECT_OK(ooxml);
  EXPECT_TRUE(ooxml->summary_information().property_set().empty());
  EXPECT_TRUE(ooxml->document_summary_information().property_set().empty());
  EXPECT_FALSE(ooxml->ole_entries().empty());
  EXPECT_FALSE(
      ooxml->ole_entries().at(0).ole_content().vba_code().chunk().empty());
  EXPECT_FALSE(ooxml->entries().empty());
  EXPECT_TRUE(ooxml->relationships().empty());

  settings.set_include_vba_code(false);
  ooxml = TryParseOOXMLFile("sample_document.docm", settings);
  MALDOCA_EXPECT_OK(ooxml);
  EXPECT_TRUE(ooxml->summary_information().property_set().empty());
  EXPECT_TRUE(ooxml->document_summary_information().property_set().empty());
  EXPECT_FALSE(ooxml->ole_entries().empty());
  EXPECT_FALSE(ooxml->entries().empty());
  EXPECT_TRUE(ooxml->relationships().empty());

  settings.set_include_structure_information(false);
  ooxml = TryParseOOXMLFile("sample_document.docm", settings);
  MALDOCA_EXPECT_OK(ooxml);
  EXPECT_TRUE(ooxml->summary_information().property_set().empty());
  EXPECT_TRUE(ooxml->document_summary_information().property_set().empty());
  EXPECT_TRUE(ooxml->ole_entries().empty());
  EXPECT_TRUE(ooxml->entries().empty());
  EXPECT_TRUE(ooxml->relationships().empty());

  settings.set_include_embedded_objects(false);
  ooxml = TryParseOOXMLFile("sample_document.docm", settings);
  MALDOCA_EXPECT_OK(ooxml);
  EXPECT_TRUE(ooxml->summary_information().property_set().empty());
  EXPECT_TRUE(ooxml->document_summary_information().property_set().empty());
  EXPECT_TRUE(ooxml->ole_entries().empty());
  EXPECT_TRUE(ooxml->entries().empty());
  EXPECT_TRUE(ooxml->relationships().empty());

  settings.set_include_vba_code(true);
  ooxml = TryParseOOXMLFile("sample_document.docm", settings);
  MALDOCA_EXPECT_OK(ooxml);
  EXPECT_TRUE(ooxml->summary_information().property_set().empty());
  EXPECT_TRUE(ooxml->document_summary_information().property_set().empty());
  EXPECT_FALSE(
      ooxml->ole_entries().at(0).ole_content().vba_code().chunk().empty());
  EXPECT_TRUE(ooxml->entries().empty());
  EXPECT_TRUE(ooxml->relationships().empty());
}
}  // namespace
}  // namespace maldoca

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  maldoca::InitLogging();
  return RUN_ALL_TESTS();
}
