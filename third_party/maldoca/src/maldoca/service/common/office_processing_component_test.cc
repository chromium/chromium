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

#include "maldoca/service/common/office_processing_component.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/parse_text_proto.h"
#include "maldoca/base/testing/protocol-buffer-matchers.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/service/common/utils.h"

namespace maldoca {
namespace {

using ::maldoca::testing::EqualsProto;
using ::maldoca::testing::ServiceTestFilename;
using ::maldoca::testing::proto::IgnoringRepeatedFieldOrdering;

constexpr char kConfgString[] = R"(
  handler_configs {
      key: "office_parser"
      value {
        doc_type: OFFICE
        parser_config {
          handler_type: DEFAULT_OFFICE_PARSER
          use_sandbox: false
          handler_config{
            default_office_parser_config {
              ole_to_proto_settings {
                include_summary_information: true
                include_vba_code : true
                include_directory_structure : true
                include_stream_hashes : true
                include_olenative_metadata : true
                include_olenative_content : true
                include_unknown_strings : true
                include_excel4_macros : true
              }
            }
          }
        }
      }
    }
    handler_configs {
      key: "office_extractor"
      value {
        doc_type: OFFICE
        feature_extractor_config {
          handler_type: DEFAULT_OFFICE_FEATURE_EXTRACTOR
        }
        dependencies: "office_parser"
      }
    }
)";

void ProcessDocument(const std::string& input_file_name,
                     const std::string& input, const ProcessorConfig& config,
                     ParsedDocument* parsed_doc,
                     DocumentFeatures* doc_features) {
  auto parsed_doc_handler = std::make_unique<OfficeParserHandler>(
      "office_parser",
      config.handler_configs().at("office_parser").parser_config());
  auto features_handler = std::make_unique<OfficeFeatureExtractorHandler>(
      "office_extractor", config.handler_configs()
                              .at("office_extractor")
                              .feature_extractor_config());
  auto exported_features_handler =
      std::make_unique<OfficeFeatureExportHandler>("FeatureExporter");

  auto doc_type = utils::InferDocTypeByName(input_file_name);
  PrepParsedOfficeDocument(input_file_name, input, "", doc_type, parsed_doc);
  MALDOCA_ASSERT_OK(parsed_doc_handler->Handle(input, parsed_doc));
  MALDOCA_ASSERT_OK(features_handler->Handle(*parsed_doc, doc_features));
}

void ValidateParsedProto(absl::string_view file_base, absl::string_view ext,
                         const ProcessorConfig& config) {
  std::string input_file_name = absl::StrCat(file_base, ".", ext);
  std::string input;
  MALDOCA_ASSERT_OK(
      testing::GetTestContents(ServiceTestFilename(input_file_name), &input));
  ParsedDocument parsed_doc;
  DocumentFeatures doc_features;
  ProcessDocument(input_file_name, input, config, &parsed_doc, &doc_features);

  std::string expected_parsed_doc_file_name =
      absl::StrCat(file_base, ".parsed.textproto");

  ParsedDocument expected_parsed_doc;
  MALDOCA_ASSERT_OK(
      file::GetTextProto(ServiceTestFilename(expected_parsed_doc_file_name),
                         &expected_parsed_doc));
  std::string expected_doc_features_file_name =
      absl::StrCat(file_base, ".features.textproto");

  DocumentFeatures expected_doc_features;
  MALDOCA_ASSERT_OK(
      file::GetTextProto(ServiceTestFilename(expected_doc_features_file_name),
                         &expected_doc_features));

#ifdef MALDOCA_CHROME
  // remove vba features from the expected
  expected_doc_features.mutable_office()->clear_vba_project_feature();
  expected_doc_features.mutable_office()->clear_vba_file_features();
#endif

  EXPECT_THAT(parsed_doc,
              IgnoringRepeatedFieldOrdering(EqualsProto(expected_parsed_doc)));

  EXPECT_THAT(doc_features, IgnoringRepeatedFieldOrdering(
                                EqualsProto(expected_doc_features)));
}

// Copy config by value on purpose since we want a mutable copy.
void CheckConfigIsUsed(ProcessorConfig config) {
  auto settings = config.mutable_handler_configs()
                      ->at("office_parser")
                      .mutable_parser_config()
                      ->mutable_handler_config()
                      ->mutable_default_office_parser_config()
                      ->mutable_ole_to_proto_settings();
  settings->set_include_vba_code(false);  // turns off vba
  std::string input;
  MALDOCA_ASSERT_OK(testing::GetTestContents(
      ServiceTestFilename("ffc835c9a950beda17fa79dd0acf28d1df3835232"
                          "877b5fdd512b3df2ffb2431_xor_0x42_encoded.doc"),
      &input));

  ParsedDocument parsed_doc;
  DocumentFeatures doc_features;
  ProcessDocument(
      "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_"
      "0x42_encoded.doc",
      input, config, &parsed_doc, &doc_features);
  EXPECT_EQ(
      2, parsed_doc.office().parser_output().script_features().scripts_size());
  // No VBA should be found
  EXPECT_EQ(0, parsed_doc.office()
                   .parser_output()
                   .script_features()
                   .scripts(0)
                   .vba_code()
                   .chunk_size());
  EXPECT_EQ(0, parsed_doc.office()
                   .parser_output()
                   .script_features()
                   .scripts(1)
                   .vba_code()
                   .chunk_size());
}

//  Just couple simple tests for now; basically copied from ole_to_proto_test.
TEST(ParseOfficeDoc, CorrectlyParse) {
  ProcessorConfig config = ParseTextOrDie<ProcessorConfig>(kConfgString);
  ValidateParsedProto(
      "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_"
      "0x42_encoded",
      "doc", config);
  ValidateParsedProto(
      "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_"
      "0x42_encoded",
      "docx", config);
  // Test that config is used by turning off VBA
  CheckConfigIsUsed(config);
}

#ifndef MALDOCA_CHROME
// Test using sandbox
TEST(ParseOfficeDoc, CorrectlyParse_Sandbox) {
  ProcessorConfig config = ParseTextOrDie<ProcessorConfig>(kConfgString);
  config.mutable_handler_configs()
      ->at("office_parser")
      .mutable_parser_config()
      ->set_use_sandbox(true);
  ValidateParsedProto(
      "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_"
      "0x42_encoded",
      "doc", config);
  ValidateParsedProto(
      "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_"
      "0x42_encoded",
      "docx", config);
  CheckConfigIsUsed(config);
}
#endif
}  // namespace
}  // namespace maldoca

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MALDOCA_CHROME
  // mini_chromium needs InitLogging
  maldoca::InitLogging();
#endif
  return RUN_ALL_TESTS();
}
