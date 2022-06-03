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

// This file defines the handler structure for the office files.
// There are 3 main types of handlers for office docs: parser,
// feature extractor, and feature exporting handlers. All these
// handlers have a common function, "Handle", where the implementation
// of the handlers should go. All handlers in a category
// (parser/feature extractor/feature exporting) take the same input and
// apply changes to the same output proto. For example, a feature extraction
// handler takes a constant "ParsedDocument" as input and makes changes to
// a "DocumentFeatures" object. These handlers will never be used by themselves
// (e.g., by calling handler.Handle()), but they will rather be used through
// a processing pipeline (see processing_pipeline.h).

#ifndef MALDOCA_SERVICE_COMMON_OFFICE_PROCESSING_COMPONENT_H_
#define MALDOCA_SERVICE_COMMON_OFFICE_PROCESSING_COMPONENT_H_

#include "maldoca/ole/proto/ole_to_proto_settings.pb.h"
#include "maldoca/ole/proto/ooxml_to_proto_settings.pb.h"

#ifndef MALDOCA_CHROME
#include "maldoca/service/common/office_sandbox/office-parser-sapi.sapi.h"
#include "maldoca/service/common/office_sandbox/sandbox.h"
#include "maldoca/service/common/office_sandbox/sapi_office_params.pb.h"
#endif

#include "maldoca/service/common/processing_component.h"
#include "maldoca/service/common/utils.h"
#include "maldoca/service/proto/document_features.pb.h"
#include "maldoca/service/proto/exported_features.pb.h"
#include "maldoca/service/proto/parsed_document.pb.h"
#include "maldoca/service/proto/processing_config.pb.h"

#ifndef MALDOCA_CHROME
#include "third_party/sandboxed_api/transaction.h"
#include "third_party/sandboxed_api/util/status_macros.h"
#include "third_party/sandboxed_api/vars.h"
#endif

namespace maldoca {
// non-MALDOCA_CHROME-begin
#ifndef MALDOCA_CHROME
class OfficeTransaction : public sapi::Transaction {
 public:
  // Time (wall-time) limit for a single Run() call (in seconds). 0 means: no
  // wall-time limit.
  OfficeTransaction(std::unique_ptr<sapi::Sandbox> sandbox, time_t timeout,
                    const ParserHandlerConfig *config)
      : sapi::Transaction(std::move(sandbox)), config_(config) {
    sapi::Transaction::SetTimeLimit(timeout);
  }
  // Default timeout 10 sec.
  OfficeTransaction(std::unique_ptr<sapi::Sandbox> sandbox,
                    const ParserHandlerConfig *config)
      : OfficeTransaction(std::move(sandbox), 10, config) {}

  void SetupData(absl::string_view input, ParsedOfficeDocument *output) {
    this->input_ = input;
    this->output_ = output;
  }

 private:
  // This function is called when running sandbox.Run()
  absl::Status Main() override;

  // The input to the parser. The actual data must not be deleted before
  // MAIN is returned.
  absl::string_view input_;
  // The output of the parser
  ParsedOfficeDocument *output_;
  // The communication object for sandboxing
  OfficeParserApi api_ = OfficeParserApi(sandbox());
  const ParserHandlerConfig *config_ = nullptr;
};
#endif
// non-MALDOCA_CHROME-end

// This is a parser handler that performs the main office parsing.
class OfficeParserHandler : public ParserHandler {
 public:
  OfficeParserHandler(absl::string_view name, const ParserConfig &config);
  explicit OfficeParserHandler(absl::string_view name)
      : OfficeParserHandler(name, GetDefaultParserConfig()) {}
  absl::Status Handle(const absl::string_view &input,
                      ParsedDocument *output) override;

 private:
  const maldoca::ole::OleToProtoSettings *ole_to_proto_settings_;
  const maldoca::ooxml::OoxmlToProtoSettings *ooxml_to_proto_settings_;

#ifndef MALDOCA_CHROME
  std::unique_ptr<OfficeTransaction> parser_transaction_;
#endif
};

// This is a feature extraction handler that performs the initial feature
// extraction for office documents.
class OfficeFeatureExtractorHandler : public FeatureExtractorHandler {
 public:
  OfficeFeatureExtractorHandler(absl::string_view name,
                                const FeatureExtractorConfig &config)
      : FeatureExtractorHandler(name, config) {}
  explicit OfficeFeatureExtractorHandler(absl::string_view name)
      : OfficeFeatureExtractorHandler(name,
                                      GetDefaultFeatureExtractorConfig()) {}
  absl::Status Handle(const ParsedDocument &input,
                      DocumentFeatures *output) override;
};

// This is a feature exporting handler that exports the extracted features to
// a desired format (e.g., tf-example), currently a NOP.
class OfficeFeatureExportHandler : public FeatureExportHandler {
 public:
  OfficeFeatureExportHandler(absl::string_view name,
                             const FeatureExportConfig &config)
      : FeatureExportHandler(name, config) {}
  explicit OfficeFeatureExportHandler(absl::string_view name)
      : OfficeFeatureExportHandler(name, GetDefaultFeatureExportConfig()) {}
  absl::Status Handle(const DocumentFeatures &input,
                      ExportedFeatures *output) override;
};

// Filling out information such as specific office document type
void PrepParsedOfficeDocument(absl::string_view file_name,
                              absl::string_view doc, absl::string_view sha256,
                              DocType doc_type, ParsedDocument *pd);

}  // namespace maldoca

#endif  // MALDOCA_SERVICE_COMMON_OFFICE_PROCESSING_COMPONENT_H_
