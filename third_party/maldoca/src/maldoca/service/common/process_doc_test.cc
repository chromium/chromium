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

#include "maldoca/service/common/process_doc.h"

#include <memory>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/parse_text_proto.h"
#include "maldoca/base/testing/protocol-buffer-matchers.h"
#include "maldoca/base/testing/status_matchers.h"
#include "maldoca/base/testing/test_utils.h"
#include "maldoca/service/common/utils.h"
#include "maldoca/service/proto/maldoca_service.pb.h"

namespace maldoca {
namespace {

using ::maldoca::ParseTextOrDie;
using ::maldoca::testing::EqualsProto;
using ::maldoca::testing::ServiceTestFilename;
using ::maldoca::testing::proto::IgnoringRepeatedFieldOrdering;
using ::testing::Test;

class ProcessDocTest : public Test {
 protected:
  class DocProcessorTester : public DocProcessor {
   public:
    explicit DocProcessorTester(const ProcessorConfig &config)
        : DocProcessor(config) {}
    ~DocProcessorTester() override = default;

    const std::vector<
        std::unique_ptr<Handler<absl::string_view, ParsedDocument>>>
        &GetOfficeParser() {
      return office_parser_vector_;
    }
    const std::vector<
        std::unique_ptr<Handler<ParsedDocument, DocumentFeatures>>>
        &GetOfficeExtractor() {
      return office_feature_extractor_vector_;
    }
#ifndef MALDOCA_CHROME
    const std::vector<
        std::unique_ptr<Handler<absl::string_view, ParsedDocument>>>
        &GetPdfParser() {
      return pdf_parser_vector_;
    }
    const std::vector<
        std::unique_ptr<Handler<ParsedDocument, DocumentFeatures>>>
        &GetPdfExtractor() {
      return pdf_feature_extractor_vector_;
    }
    const std::vector<
        std::unique_ptr<Handler<DocumentFeatures, ExportedFeatures>>>
        &GetPdfExport() {
      return pdf_feature_export_vector_;
    }
#endif
  };

  void SetUp() override {
    Test::SetUp();
// non-MALDOCA_CHROME-begin
#ifndef MALDOCA_CHROME
    config_ = ParseTextOrDie<ProcessorConfig>(R"pb(
      handler_configs {
        key: "vba_parser"
        value {
          doc_type: OFFICE
          parser_config { handler_type: DEFAULT_VBA_PARSER use_sandbox: false }
        }
      }
      handler_configs {
        key: "office_parser"
        value {
          doc_type: OFFICE
          parser_config {
            handler_type: DEFAULT_OFFICE_PARSER
            use_sandbox: false
          }
        }
      }
      handler_configs {
        key: "pdf_parser"
        value {
          doc_type: PDF
          parser_config { handler_type: DEFAULT_PDF_PARSER use_sandbox: false }
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
      handler_configs {
        key: "pdf_extractor"
        value {
          doc_type: PDF
          feature_extractor_config {
            handler_type: DEFAULT_PDF_FEATURE_EXTRACTOR
          }
          dependencies: "pdf_parser"
        }
      }
      handler_configs {
        key: "pdf_export"
        value {
          doc_type: PDF
          feature_export_config { handler_type: DEFAULT_PDF_EXPORT }
          dependencies: "pdf_extractor"
        }
      }
    )pb");
#else
    config_ = ParseTextOrDie<ProcessorConfig>(R"pb(
      handler_configs {
        key: "office_parser"
        value {
          doc_type: OFFICE
          parser_config {
            handler_type: DEFAULT_OFFICE_PARSER
            use_sandbox: false
          }
        }
      }
    )pb");
#endif
    // non-MALDOCA_CHROME-end
  }

  void ValidateProcessedProto(const std::string &test_file,
                              const std::string &expected_response_file_name,
                              DocProcessor *processor,
                              const DocType doc_type = DocType::UNKNOWN_TYPE) {
    ProcessDocumentRequest request;
    request.set_file_name(test_file);
    if (doc_type != DocType::UNKNOWN_TYPE) {
      request.set_doc_type(doc_type);
    } else {
      request.set_doc_type(::maldoca::utils::InferDocTypeByName(test_file));
    }
    MALDOCA_ASSERT_OK(testing::GetTestContents(ServiceTestFilename(test_file),
                                               request.mutable_doc_content()));
    ProcessDocumentResponse response;
    MALDOCA_ASSERT_OK(processor->ProcessDoc(&request, &response));

    ProcessDocumentResponse expected_response;
    MALDOCA_ASSERT_OK(file::GetTextProto(
        ServiceTestFilename(expected_response_file_name), &expected_response));

#ifdef MALDOCA_CHROME
    // remove features from the expected since not doing any extraction
    expected_response.clear_document_features();
    // making an empy document_features so passes test
    expected_response.mutable_document_features();
#endif

    EXPECT_THAT(response,
                IgnoringRepeatedFieldOrdering(EqualsProto(expected_response)));
  }

  void TestFiles(const ProcessorConfig &config) {
    DocProcessor processor(config);
#ifndef MALDOCA_CHROME
    ValidateProcessedProto("vba1_xor_0x42_encoded.bin",
                           "vba1_xor_0x42_encoded.bin.response.textproto",
                           &processor, DocType::VB);
    ValidateProcessedProto("image_and_text.pdf",
                           "image_and_text.pdf.response.textproto", &processor);
    ValidateProcessedProto("embedded_file.pdf",
                           "embedded_file.pdf.response.textproto", &processor);
    ValidateProcessedProto("image_and_link.pdf",
                           "image_and_link.pdf.response.textproto", &processor);
#endif
    ValidateProcessedProto(
        "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_"
        "0x42_encoded.doc",
        "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_"
        "0x42_encoded.response.textproto",
        &processor);
    ValidateProcessedProto(
        "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_"
        "0x42_encoded.docx",
        "c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d_xor_"
        "0x42_encoded.response.textproto",
        &processor);
  }

  ProcessorConfig config_;
};

TEST_F(ProcessDocTest, DocumentProcessorTest) { TestFiles(config_); }

TEST_F(ProcessDocTest, DisableMagician) {
  ProcessorConfig config = config_;
  config.set_disable_file_type_check(true);
  DocProcessor processor(config);
  ProcessDocumentRequest request;
  std::string test_file(
      "ffc835c9a950beda17fa79dd0acf28d1df3835232877b5fdd512b3df2ffb2431_xor_"
      "0x42_encoded.doc");
  request.set_file_name(test_file);
  MALDOCA_ASSERT_OK(testing::GetTestContents(ServiceTestFilename(test_file),
                                             request.mutable_doc_content()));
  ProcessDocumentResponse response;
  // Magician is disabled and the doc_type is not set so should fail
  EXPECT_FALSE(processor.ProcessDoc(&request, &response).ok());
  // Set to the doc_type and should work
  request.set_doc_type(DocType::DOC);
  EXPECT_TRUE(processor.ProcessDoc(&request, &response).ok());
}

// non-MALDOCA_CHROME-begin
#ifndef MALDOCA_CHROME
// Test using sandbox
TEST_F(ProcessDocTest, DocumentProcessorTest_Sandbox) {
  ProcessorConfig config = config_;
  config.mutable_handler_configs()
      ->at("office_parser")
      .mutable_parser_config()
      ->set_use_sandbox(true);
  config.mutable_handler_configs()
      ->at("pdf_parser")
      .mutable_parser_config()
      ->set_use_sandbox(true);
  TestFiles(config);
}

TEST_F(ProcessDocTest, HandlerOrdering) {
  ProcessorConfig config = ParseTextOrDie<ProcessorConfig>(R"pb(
    handler_configs {
      key: "office_parser_1"
      value {
        doc_type: OFFICE
        parser_config { handler_type: DEFAULT_OFFICE_PARSER }
        dependencies: "office_parser_2"
      }
    }
    handler_configs {
      key: "office_parser_2"
      value {
        doc_type: OFFICE
        parser_config { handler_type: DEFAULT_OFFICE_PARSER }
        dependencies: "office_parser_3"
      }
    }
    handler_configs {
      key: "office_parser_3"
      value {
        doc_type: OFFICE
        parser_config { handler_type: DEFAULT_OFFICE_PARSER }
      }
    }
    handler_configs {
      key: "pdf_parser_1"
      value {
        doc_type: PDF
        parser_config { handler_type: DEFAULT_PDF_PARSER }
        dependencies: "pdf_parser_2"
        dependencies: "pdf_parser_3"
      }
    }
    handler_configs {
      key: "pdf_parser_2"
      value {
        doc_type: PDF
        parser_config { handler_type: DEFAULT_PDF_PARSER }
      }
    }
    handler_configs {
      key: "pdf_parser_3"
      value {
        doc_type: PDF
        parser_config { handler_type: DEFAULT_PDF_PARSER }
        dependencies: "pdf_parser_2"
      }
    }
    handler_configs {
      key: "office_extractor_1"
      value {
        doc_type: OFFICE
        feature_extractor_config {
          handler_type: DEFAULT_OFFICE_FEATURE_EXTRACTOR
        }
        dependencies: "office_extractor_2"
      }
    }
    handler_configs {
      key: "office_extractor_2"
      value {
        doc_type: OFFICE
        feature_extractor_config {
          handler_type: DEFAULT_OFFICE_FEATURE_EXTRACTOR
        }
      }
    }
    handler_configs {
      key: "pdf_extractor_1"
      value {
        doc_type: PDF
        feature_extractor_config { handler_type: DEFAULT_PDF_FEATURE_EXTRACTOR }
      }
    }
    handler_configs {
      key: "pdf_extractor_2"
      value {
        doc_type: PDF
        feature_extractor_config { handler_type: DEFAULT_PDF_FEATURE_EXTRACTOR }
        dependencies: "pdf_extractor_1"
      }
    }
    handler_configs {
      key: "pdf_export_1"
      value {
        doc_type: PDF
        feature_export_config { handler_type: DEFAULT_PDF_EXPORT }
        dependencies: "pdf_export_2"
      }
    }
    handler_configs {
      key: "pdf_export_2"
      value {
        doc_type: PDF
        feature_export_config { handler_type: DEFAULT_PDF_EXPORT }
      }
    }
  )pb");

  DocProcessorTester processor(config);
  const auto &office_parser = processor.GetOfficeParser();
  const auto &office_extractor = processor.GetOfficeExtractor();
  const auto &pdf_parser = processor.GetPdfParser();
  const auto &pdf_extractor = processor.GetPdfExtractor();
  const auto &pdf_export = processor.GetPdfExport();

  EXPECT_EQ(3, office_parser.size());
  EXPECT_EQ("office_parser_3", office_parser[0]->GetName());
  EXPECT_EQ("office_parser_2", office_parser[1]->GetName());
  EXPECT_EQ("office_parser_1", office_parser[2]->GetName());
  EXPECT_EQ(3, pdf_parser.size());
  EXPECT_EQ("pdf_parser_2", pdf_parser[0]->GetName());
  EXPECT_EQ("pdf_parser_3", pdf_parser[1]->GetName());
  EXPECT_EQ("pdf_parser_1", pdf_parser[2]->GetName());
  EXPECT_EQ(2, office_extractor.size());
  EXPECT_EQ("office_extractor_2", office_extractor[0]->GetName());
  EXPECT_EQ("office_extractor_1", office_extractor[1]->GetName());
  EXPECT_EQ(2, pdf_extractor.size());
  EXPECT_EQ("pdf_extractor_1", pdf_extractor[0]->GetName());
  EXPECT_EQ("pdf_extractor_2", pdf_extractor[1]->GetName());
  EXPECT_EQ(2, pdf_export.size());
  EXPECT_EQ("pdf_export_2", pdf_export[0]->GetName());
  EXPECT_EQ("pdf_export_1", pdf_export[1]->GetName());
}
#endif
// non-MALDOCA_CHROME-end

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
