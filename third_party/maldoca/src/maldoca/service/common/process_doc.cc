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

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#ifndef MALDOCA_IN_CHROMIUM
#include "google/protobuf/message.h"  // nogncheck
#endif
#include "maldoca/base/digest.h"
#include "maldoca/base/encoding_error.h"
#include "maldoca/base/status.h"
#include "maldoca/base/status_macros.h"
#include "maldoca/service/proto/doc_type.pb.h"
#include "maldoca/service/proto/processing_config.pb.h"

#ifndef MALDOCA_CHROME
#include "maldoca/pdf_parser/pdf_features.h"
#endif

#include "maldoca/service/common/office_processing_component.h"

#ifndef MALDOCA_CHROME
#include "maldoca/service/common/pdf_processing_component.h"
#endif

#include "maldoca/service/common/processing_pipeline.h"
#include "maldoca/service/common/utils.h"
#include "maldoca/service/proto/doc_type.pb.h"
// #include "pdfium/public/fpdfview.h"

namespace maldoca {
void DocProcessor::Init(const ProcessorConfig &config) {
  config_ = config;
  if (!config.disable_file_type_check()) {
#ifdef MALDOCA_CHROME
    LOG(ERROR) << "FiletypeIdentifier not defined for Chrome";
    // let it pass for now as we will just use supplied type
#else
    auto identifier = utils::FileTypeIdentifierForDocType();
    MALDOCA_CHECK_OK(identifier.status());
    identifier_ = std::move(*identifier);
#endif
  }

  // For now initializing with the default pipeline (chain) structure
#ifndef MALDOCA_CHROME
  MALDOCA_CHECK_OK(CreatePdfPipeline());
#endif
  MALDOCA_CHECK_OK(CreateOfficePipeline());
}

DocProcessor::DocProcessor(const ProcessorConfig &config)
    : DocProcessor(config, absl::make_unique<ParserHandlerFactory>(),
                   absl::make_unique<FeatureExtractorHandlerFactory>(),
                   absl::make_unique<FeatureExportHandlerFactory>()) {}

DocProcessor::DocProcessor(
    const ProcessorConfig &config,
    std::unique_ptr<ParserHandlerFactory> parser_factory,
    std::unique_ptr<FeatureExtractorHandlerFactory> extractor_factory,
    std::unique_ptr<FeatureExportHandlerFactory> export_factory)
    : parser_factory_(std::move(parser_factory)),
      extractor_factory_(std::move(extractor_factory)),
      export_factory_(std::move(export_factory)) {
  Init(config);
}

// non-MALDOCA_CHROME-begin
#ifndef MALDOCA_CHROME
absl::Status DocProcessor::CreatePdfPipeline() {
  bool need_pdf_processor = false;
  std::vector<std::pair<absl::string_view, const HandlerConfig *>> config_vec;
  for (const auto &conf : config_.handler_configs()) {
    if (conf.second.doc_type() == HandlerConfig::PDF) {
      config_vec.push_back({conf.first, &conf.second});
      if (conf.second.has_parser_config()) {
        need_pdf_processor = true;
      }
    }
  }
  if (config_vec.empty()) {
    return absl::OkStatus();  // nothing to do
  }
  if (need_pdf_processor) {
    CHECK(pdfium_processor_.InitProcessor()) << "Fail to init pdfium processor";
    parser_factory_->SetPdfiumProcessor(&pdfium_processor_);
  }
  extractor_factory_->SetAdobeAPIMapper(&api_mapper_);
  // name -> index map
  absl::flat_hash_map<absl::string_view, int> lookup;
  for (int i = 0; i < config_vec.size(); ++i) {
    lookup[config_vec[i].first] = i;
  }
  std::vector<std::vector<int32_t>> dependencies(config_vec.size());
  for (int i = 0; i < config_vec.size(); ++i) {
    const auto &config = config_vec[i];
    for (const auto &d : config.second->dependencies()) {
      auto iter = lookup.find(d);
      if (iter == lookup.end()) {
        // Error not found
        return absl::InvalidArgumentError(
            absl::StrCat("Missing dependency ", d, " for ", config.first));
      }
      dependencies[i].push_back(iter->second);
    }
  }
  auto sorted = ::maldoca::utils::SortDependencies(dependencies);
  if (!sorted.ok()) {
    return sorted.status();
  }

  // Since the pipeline is single threaded for now, can ignore connected
  // component
  for (const auto &order : sorted.value()) {
    const auto &config = config_vec[order.first];
    const auto &config_proto = *config.second;
    if (config_proto.has_parser_config()) {
      MALDOCA_ASSIGN_OR_RETURN(
          auto parser,
          parser_factory_->Build(config.first, config_proto.parser_config()));
      MALDOCA_RETURN_IF_ERROR(pdf_pipeline_.Add(parser.get()));
      pdf_parser_vector_.push_back(std::move(parser));

      LOG(INFO) << "Add Handler: " << config.first;
    } else if (config_proto.has_feature_extractor_config()) {
      MALDOCA_ASSIGN_OR_RETURN(
          auto extractor,
          extractor_factory_->Build(config.first,
                                    config_proto.feature_extractor_config()));
      MALDOCA_RETURN_IF_ERROR(pdf_pipeline_.Add(extractor.get()));
      pdf_feature_extractor_vector_.push_back(std::move(extractor));
      LOG(INFO) << "Add Handler: " << config.first;

    } else if (config_proto.has_feature_export_config()) {
      MALDOCA_ASSIGN_OR_RETURN(
          auto exporter,
          export_factory_->Build(config.first,
                                 config_proto.feature_export_config()));
      MALDOCA_RETURN_IF_ERROR(pdf_pipeline_.Add(exporter.get()));
      pdf_feature_export_vector_.push_back(std::move(exporter));

      LOG(INFO) << "Add Handler: " << config.first;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Missing config for ", config.first));
    }
  }

  return absl::OkStatus();
}
#endif
// non-MALDOCA_CHROME-end

absl::Status DocProcessor::CreateOfficePipeline() {
  std::vector<std::pair<absl::string_view, const HandlerConfig *>> config_vec;
  for (const auto &conf : config_.handler_configs()) {
    if (conf.second.doc_type() == HandlerConfig::OFFICE) {
#ifdef MALDOCA_CHROME
      // For now only allow parsers
      if (!conf.second.has_parser_config()) {
        return absl::UnimplementedError(
            "Only office parsers are implemented for the Chrome version");
      }
#endif
      config_vec.push_back({conf.first, &conf.second});
    }
  }
  if (config_vec.empty()) {
    return absl::OkStatus();  // nothing to do
  }
  // name -> index map
  absl::flat_hash_map<absl::string_view, int> lookup;
  for (int i = 0; i < config_vec.size(); ++i) {
    lookup[config_vec[i].first] = i;
  }
  std::vector<std::vector<int32_t>> dependencies(config_vec.size());
  for (int i = 0; i < config_vec.size(); ++i) {
    const auto &config = config_vec[i];
    for (const auto &d : config.second->dependencies()) {
      auto iter = lookup.find(d);
      if (iter == lookup.end()) {
        // Error not found
        return absl::InvalidArgumentError(
            absl::StrCat("Missing dependency ", d, " for ", config.first));
      }
      dependencies[i].push_back(iter->second);
    }
  }
  auto sorted = ::maldoca::utils::SortDependencies(dependencies);
  if (!sorted.ok()) {
    return sorted.status();
  }

  // Since the pipeline is single threaded for now, can ignore connected
  // component
  for (const auto &order : sorted.value()) {
    const auto &config = config_vec[order.first];
    const auto &config_proto = *config.second;
    if (config_proto.has_parser_config()) {
      MALDOCA_ASSIGN_OR_RETURN(
          auto parser,
          parser_factory_->Build(config.first, config_proto.parser_config()));
      MALDOCA_RETURN_IF_ERROR(office_pipeline_.Add(parser.get()));
      office_parser_vector_.push_back(std::move(parser));

      LOG(INFO) << "Add Handler: " << config.first;
    } else if (config_proto.has_feature_extractor_config()) {
      MALDOCA_ASSIGN_OR_RETURN(
          auto extractor,
          extractor_factory_->Build(config.first,
                                    config_proto.feature_extractor_config()));
      MALDOCA_RETURN_IF_ERROR(office_pipeline_.Add(extractor.get()));
      office_feature_extractor_vector_.push_back(std::move(extractor));

      LOG(INFO) << "Add Handler: " << config.first;
    } else if (config_proto.has_feature_export_config()) {
      MALDOCA_ASSIGN_OR_RETURN(
          auto exporter,
          export_factory_->Build(config.first,
                                 config_proto.feature_export_config()));
      MALDOCA_RETURN_IF_ERROR(office_pipeline_.Add(exporter.get()));
      office_feature_export_vector_.push_back(std::move(exporter));
      LOG(INFO) << "Add Handler: " << config.first;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Missing config for ", config.first));
    }
  }

  return absl::OkStatus();
}

absl::Status DocProcessor::ProcessDoc(absl::string_view file_name,
                                      absl::string_view doc,
                                      const ProcessDocumentRequest *request,
                                      ProcessDocumentResponse *response) {
#ifndef MALDOCA_CHROME
  auto doc_type = (identifier_ == nullptr)
                      ? request->doc_type()
                      : utils::InferDocType(file_name, doc, identifier_.get());
  // This allows parsing for vba script since the type cannot be infered by
  // file magics.
  if (doc_type == DocType::UNKNOWN_TYPE &&
      ((request->doc_type() == DocType::VB) ||
       (request->doc_type() == DocType::VBS) ||
       (request->doc_type() == DocType::VBE))) {
    doc_type = request->doc_type();
  }
#else
  auto doc_type = request->doc_type();
#endif

  ParsedDocument *parsed_doc;
  ProcessingPipeline *current_document_pipeline;
  auto general_doc_type = utils::DocTypeToGeneralType(doc_type);
  switch (general_doc_type) {
#ifndef MALDOCA_CHROME
    case GeneralDocType::GENERAL_DOC_TYPE_PDF: {
      // Routing processing to the pdf pipeline
      pdf_pipeline_.ResetPipelineData();
      // Need to fill out some information before running the pipeline
      current_document_pipeline = &pdf_pipeline_;
      parsed_doc = current_document_pipeline->GetMutableParsedDocument();
      PrepParsedPDFDocument(file_name, doc, "", parsed_doc);
      DLOG(INFO) << "Processing PDF...";
      break;
    }
#endif
    case GeneralDocType::GENERAL_DOC_TYPE_VBA:
    case GeneralDocType::GENERAL_DOC_TYPE_DOC:
    case GeneralDocType::GENERAL_DOC_TYPE_XLS:
    case GeneralDocType::GENERAL_DOC_TYPE_PPT: {
      // Routing processing to the office pipeline
      office_pipeline_.ResetPipelineData();
      current_document_pipeline = &office_pipeline_;
      parsed_doc = current_document_pipeline->GetMutableParsedDocument();
      PrepParsedOfficeDocument(file_name, doc, "", doc_type, parsed_doc);
      DLOG(INFO) << "Processing Office...";
      break;
    }
    case GeneralDocType::GENERAL_DOC_TYPE_UNKNOWN:
    default: {
      return ::maldoca::InternalError(
          absl::StrCat("Unsupported Type: ", DocType_Name(doc_type)),
          MaldocaErrorCode::UNSUPPORTED_DOC_TYPE);
    }
  }

  absl::Status status = current_document_pipeline->Process(doc);
  if (!status.ok()) {
    std::string failed_encoding = ::maldoca::GetFailedEncoding();
    if (!failed_encoding.empty()) {
      return maldoca::UnimplementedError(failed_encoding,
                                         MaldocaErrorCode::MISSING_ENCODING);
    } else {
      return status;
    }
  }

  response->set_allocated_parsed_document(
      current_document_pipeline->ReleaseParsedDocument());
  response->set_allocated_document_features(
      current_document_pipeline->ReleaseDocumentFeatures());
  response->set_allocated_exported_features(
      current_document_pipeline->ReleaseExportedFeatures());

  return absl::OkStatus();
}
}  // namespace maldoca
