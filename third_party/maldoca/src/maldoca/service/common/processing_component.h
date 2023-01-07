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

/* This Class defines the interface for the handlers placed in the chain of
 * responsibilities. There is also an implementation for a parser based on
 * pdfium, a baseline feature extraction, and a handler which enables exporting
 * these features to a TfExample.
 */

#ifndef MALDOCA_SERVICE_COMMON_PROCESSING_COMPONENT_H_
#define MALDOCA_SERVICE_COMMON_PROCESSING_COMPONENT_H_

#include <memory>

#include "absl/status/status.h"
#include "maldoca/base/statusor.h"
#include "maldoca/service/proto/document_features.pb.h"
#include "maldoca/service/proto/exported_features.pb.h"
#include "maldoca/service/proto/parsed_document.pb.h"
#include "maldoca/service/proto/processing_config.pb.h"

namespace maldoca {

// This class defines a handler that can be placed in the chain of
// responsibility. Input and Output define the type of handler.
// For example if Input="absl::string_view" and Output="ParsedDocument", we deal
// with a parser handler, as the parsing part uses the raw pdf/office
// representation to create a "ParsedDocument" object.
template <typename Input, typename Output>
class Handler {
 public:
  explicit Handler(absl::string_view name) : registration_name_(name) {}
  virtual absl::Status Handle(const Input &packet, Output *out) = 0;
  absl::string_view GetName() { return registration_name_; }
  virtual ~Handler() = default;

 private:
  std::string registration_name_;
};

const ParserConfig &GetDefaultParserConfig();
const FeatureExtractorConfig &GetDefaultFeatureExtractorConfig();
const FeatureExportConfig &GetDefaultFeatureExportConfig();
const ProcessorConfig &GetDefaultProcessorConfig();

// Specialized handler for parsing
class ParserHandler : public Handler<absl::string_view, ParsedDocument> {
 public:
  ParserHandler(absl::string_view name, const ParserConfig &config)
      : Handler<absl::string_view, ParsedDocument>(name), config_(config) {}

  const ParserConfig &Config() { return config_; }

 private:
  const ParserConfig &config_;
};

// Specialize handler for Feature Extraction
class FeatureExtractorHandler
    : public Handler<ParsedDocument, DocumentFeatures> {
 public:
  FeatureExtractorHandler(absl::string_view name,
                          const FeatureExtractorConfig &config)
      : Handler<ParsedDocument, DocumentFeatures>(name), config_(config) {}

  const FeatureExtractorConfig &Config() { return config_; }

 private:
  const FeatureExtractorConfig &config_;
};

// Specialize handler for Feature Export
class FeatureExportHandler
    : public Handler<DocumentFeatures, ExportedFeatures> {
 public:
  FeatureExportHandler(absl::string_view name,
                       const FeatureExportConfig &config)
      : Handler<DocumentFeatures, ExportedFeatures>(name), config_(config) {}

  const FeatureExportConfig &Config() { return config_; }

 private:
  const FeatureExportConfig &config_;
};
}  // namespace maldoca

#endif  // MALDOCA_SERVICE_COMMON_PROCESSING_COMPONENT_H_
