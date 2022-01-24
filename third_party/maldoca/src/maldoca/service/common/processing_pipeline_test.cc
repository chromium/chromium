/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "maldoca/service/common/processing_pipeline.h"

#include <memory>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/testing/protocol-buffer-matchers.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/service/common/office_processing_component.h"

#ifndef MALDOCA_CHROME
#include "maldoca/service/common/pdf_processing_component.h"
#endif

#include "maldoca/service/common/utils.h"

namespace maldoca {
namespace {

using ::maldoca::testing::EqualsProto;
using ::maldoca::testing::ServiceTestFilename;
using ::maldoca::testing::proto::IgnoringRepeatedFieldOrdering;
using ::testing::Test;

class ProcesDocTest : public Test {
 protected:
#ifndef MALDOCA_CHROME
  void SetUp() override {
    Test::SetUp();
    MALDOCA_ASSERT_OK_AND_ASSIGN(identifier_,
                                 utils::FileTypeIdentifierForDocType());
    CHECK(processor_.InitProcessor()) << "Failed to init pdfium processor";
  }
#endif

  void SetupPipeline(const HandlerConfig &config) {
#ifndef MALDOCA_CHROME
    pdf_parser_vector_.push_back(
        std::make_unique<PdfParserHandler>("Parser", &processor_));
    pdf_feature_extractor_vector_.push_back(
        std::make_unique<PdfFeatureExtractorHandler>("FeatureExtraction",
                                                     &api_mapper_));
    pdf_feature_exporter_vector_.push_back(
        std::make_unique<PdfFeatureExportHandler>("Exporter"));

    MALDOCA_EXPECT_OK(pdf_pipeline_.Add(pdf_parser_vector_));
    MALDOCA_EXPECT_OK(pdf_pipeline_.Add(pdf_feature_extractor_vector_));
    MALDOCA_EXPECT_OK(pdf_pipeline_.Add(pdf_feature_exporter_vector_));
#endif

    office_parser_vector_.push_back(std::make_unique<OfficeParserHandler>(
        "Parser", config.parser_config()));
    office_feature_extractor_vector_.push_back(
        std::make_unique<OfficeFeatureExtractorHandler>(
            "FeatureExtraction", config.feature_extractor_config()));
    MALDOCA_EXPECT_OK(office_pipeline_.Add(office_parser_vector_));
    MALDOCA_EXPECT_OK(office_pipeline_.Add(office_feature_extractor_vector_));
  }

  void ValidateProcessedProto(absl::string_view file_base,
                              absl::string_view ext) {
    std::string input_file_name = absl::StrCat(file_base, ".", ext);
    std::string input;
    MALDOCA_ASSERT_OK(
        testing::GetTestContents(ServiceTestFilename(input_file_name), &input));

#ifndef MALDOCA_CHROME
    pdf_pipeline_.ResetPipelineData();
#endif
    office_pipeline_.ResetPipelineData();

    ProcessingPipeline *crt_pipeline;
    // we are testing the pipeline functionality right now, so we hard code
    // information like doc type or file name
    ParsedDocument *parsed_doc;

#ifndef MALDOCA_CHROME
    if (ext == "pdf") {
      crt_pipeline = &pdf_pipeline_;
      parsed_doc = crt_pipeline->GetMutableParsedDocument();
      PrepParsedPDFDocument(input_file_name, input, "", parsed_doc);
    } else {
      crt_pipeline = &office_pipeline_;
      parsed_doc = crt_pipeline->GetMutableParsedDocument();
      auto doc_type = utils::InferDocTypeByName(input_file_name);
      PrepParsedOfficeDocument(input_file_name, input, "", doc_type,
                               parsed_doc);
    }
#else
    crt_pipeline = &office_pipeline_;
    parsed_doc = crt_pipeline->GetMutableParsedDocument();
    auto doc_type = utils::InferDocTypeByName(input_file_name);
    PrepParsedOfficeDocument(input_file_name, input, "", doc_type, parsed_doc);
#endif
    MALDOCA_ASSERT_OK(crt_pipeline->Process(input));

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

    EXPECT_THAT(
        crt_pipeline->GetParsedDocument(),
        IgnoringRepeatedFieldOrdering(EqualsProto(expected_parsed_doc)));

    EXPECT_THAT(
        crt_pipeline->GetDocumentFeatures(),
        IgnoringRepeatedFieldOrdering(EqualsProto(expected_doc_features)));
  }

  void NameCollisions() {
    // There already is a handler named "Parser" in the office pipeline.
    std::vector<std::unique_ptr<Handler<absl::string_view, ParsedDocument>>>
        office_parser_vector;
    office_parser_vector.push_back(
        std::make_unique<OfficeParserHandler>("Parser"));
    auto status = office_pipeline_.Add(office_parser_vector);
    EXPECT_NE(status, absl::OkStatus());

#ifndef MALDOCA_CHROME
    // There already is a handler name "Exporter" in the pdf pipeline.
    std::vector<std::unique_ptr<Handler<DocumentFeatures, ExportedFeatures>>>
        pdf_exported_features;
    pdf_exported_features.push_back(
        std::make_unique<PdfFeatureExportHandler>("Exporter"));
    status = pdf_pipeline_.Add(pdf_exported_features);
    EXPECT_NE(status, absl::OkStatus());
#endif
  }

 private:
#ifndef MALDOCA_CHROME
  std::vector<std::unique_ptr<Handler<absl::string_view, ParsedDocument>>>
      pdf_parser_vector_;
  std::vector<std::unique_ptr<Handler<ParsedDocument, DocumentFeatures>>>
      pdf_feature_extractor_vector_;
  std::vector<std::unique_ptr<Handler<DocumentFeatures, ExportedFeatures>>>
      pdf_feature_exporter_vector_;
#endif

  std::vector<std::unique_ptr<Handler<absl::string_view, ParsedDocument>>>
      office_parser_vector_;
  std::vector<std::unique_ptr<Handler<ParsedDocument, DocumentFeatures>>>
      office_feature_extractor_vector_;
  std::vector<std::unique_ptr<Handler<DocumentFeatures, ExportedFeatures>>>
      office_feature_exporter_vector_;

#ifndef MALDOCA_CHROME
  PdfiumProcessor processor_;
  AdobeAPIMapper api_mapper_;
  ProcessingPipeline pdf_pipeline_;
  std::unique_ptr<FileTypeIdentifier> identifier_;
#endif

  ProcessingPipeline office_pipeline_;
};

//  Just a few simple tests for now
TEST_F(ProcesDocTest, CorrectlyParse) {
  HandlerConfig config;
  SetupPipeline(config);
  ValidateProcessedProto(
      "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_"
      "0x42_encoded",
      "doc");
  ValidateProcessedProto(
      "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_"
      "0x42_encoded",
      "docx");
#ifndef MALDOCA_CHROME
  ValidateProcessedProto("image_and_text", "pdf");
#endif
  NameCollisions();
}

#ifndef MALDOCA_CHROME
// Test using sandbox.
TEST_F(ProcesDocTest, CorrectlyParse_Sandbox) {
  HandlerConfig config;
  config.mutable_parser_config()->set_use_sandbox(true);
  SetupPipeline(config);
  ValidateProcessedProto(
      "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_"
      "0x42_encoded",
      "doc");
  ValidateProcessedProto(
      "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_"
      "0x42_encoded",
      "docx");
  ValidateProcessedProto("image_and_text", "pdf");
}
#endif
}  // namespace
}  // namespace maldoca

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef MALDOCA_CHROME
  // mini_chromium needs InitLogging
  maldoca::InitLogging();
#endif
  return RUN_ALL_TESTS();
}
