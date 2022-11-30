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

#include <algorithm>
#ifndef MALDOCA_CHROME
#include "google/protobuf/text_format.h"  // nogncheck
#endif  // MALODCA_CHOMRE
#include "maldoca/base/file.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/status_macros.h"

namespace maldoca {
absl::Status ProcessingPipeline::Add(
    const std::vector<
        std::unique_ptr<Handler<absl::string_view, ParsedDocument>>> &hdlrs) {
  for (const auto &v : hdlrs) {
    MALDOCA_RETURN_IF_ERROR(Add(v.get()));
  }
  return absl::OkStatus();
}
absl::Status ProcessingPipeline::Add(
    const std::vector<
        std::unique_ptr<Handler<ParsedDocument, DocumentFeatures>>> &hdlrs) {
  for (const auto &v : hdlrs) {
    MALDOCA_RETURN_IF_ERROR(Add(v.get()));
  }
  return absl::OkStatus();
}
absl::Status ProcessingPipeline::Add(
    const std::vector<
        std::unique_ptr<Handler<DocumentFeatures, ExportedFeatures>>> &hdlrs) {
  for (const auto &v : hdlrs) {
    MALDOCA_RETURN_IF_ERROR(Add(v.get()));
  }
  return absl::OkStatus();
}

absl::Status ProcessingPipeline::Add(
    Handler<absl::string_view, ParsedDocument> *hdl) {
  MALDOCA_RETURN_IF_ERROR(RegisterName(hdl->GetName()));
  parser_.push_back(hdl);
  return absl::OkStatus();
}
absl::Status ProcessingPipeline::Add(
    Handler<ParsedDocument, DocumentFeatures> *hdl) {
  MALDOCA_RETURN_IF_ERROR(RegisterName(hdl->GetName()));
  feature_extractor_.push_back(hdl);
  return absl::OkStatus();
}
absl::Status ProcessingPipeline::Add(
    Handler<DocumentFeatures, ExportedFeatures> *hdl) {
  MALDOCA_RETURN_IF_ERROR(RegisterName(hdl->GetName()));
  feature_exporter_.push_back(hdl);
  return absl::OkStatus();
}

void ProcessingPipeline::ResetPipelineData() {
  parsed_information_ = absl::make_unique<ParsedDocument>();
  extracted_features_ = absl::make_unique<DocumentFeatures>();
  exported_features_ = absl::make_unique<ExportedFeatures>();
}

absl::Status ProcessingPipeline::RegisterName(absl::string_view name) {
  std::string name_str(name.data(), name.size());
  if (registration_set_.contains(name_str)) {
    return absl::InternalError(
        absl::StrCat("Handler name: ", name, " already exists"));
  }
  registration_set_.insert(name_str);
  return absl::OkStatus();
}

absl::Status ProcessingPipeline::Process(absl::string_view input) {
  for (auto hdlr : parser_) {
    auto status = hdlr->Handle(input, parsed_information_.get());
    if (!status.ok()) {
      LOG(ERROR) << "[" << hdlr->GetName() << "] Parsing Component Failed";
      return status;
    }
  }

  for (auto hdlr : feature_extractor_) {
    auto status = hdlr->Handle(*parsed_information_, extracted_features_.get());
    if (!status.ok()) {
      LOG(ERROR) << "[" << hdlr->GetName()
                 << "] Feature Extraction Component Failed";
      return status;
    }
  }

  for (auto hdlr : feature_exporter_) {
    auto status = hdlr->Handle(*extracted_features_, exported_features_.get());
    if (!status.ok()) {
      LOG(ERROR) << "[" << hdlr->GetName()
                 << "] Feature Exporting Component Failed";
      return status;
    }
  }
  return absl::OkStatus();
}

}  // namespace maldoca
