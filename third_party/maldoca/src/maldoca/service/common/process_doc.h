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

// Process office and pdf docs.

#ifndef MALDOCA_SERVICE_COMMON_PROCESS_DOC_H_
#define MALDOCA_SERVICE_COMMON_PROCESS_DOC_H_

#include <memory>

#include "maldoca/base/status.h"

#ifndef MALDOCA_CHROME
#include "maldoca/pdf_parser/adobe_api_mapper.h"
#include "maldoca/pdf_parser/pdfium_processor.h"
#include "maldoca/service/common/file_type_identifier.h"
#endif

#include "maldoca/service/common/processing_component_factory.h"
#include "maldoca/service/common/processing_pipeline.h"
#include "maldoca/service/proto/maldoca_service.pb.h"
#include "maldoca/service/proto/processing_config.pb.h"

namespace maldoca {
// A wrapper class to contain all the resources needed to parse and extract
// features from a document. Note this class is not thread safe.
class DocProcessor {
 public:
  explicit DocProcessor(const ProcessorConfig& config);
  DocProcessor(
      const ProcessorConfig& config,
      std::unique_ptr<ParserHandlerFactory> parser_factory,
      std::unique_ptr<FeatureExtractorHandlerFactory> extractor_factory,
      std::unique_ptr<FeatureExportHandlerFactory> export_factory);
  virtual ~DocProcessor() = default;

  // This function fills out the "ProcessDocumentResponse" which is sent
  // to the client - runs the whole pipeline for the current document ("doc")
  absl::Status ProcessDoc(absl::string_view file_name, absl::string_view doc,
                          const ProcessDocumentRequest* request,
                          ProcessDocumentResponse* response);

  // The same call as ProcessDoc above but assumes the file_name and
  // content are already in the request.
  inline absl::Status ProcessDoc(const ProcessDocumentRequest* request,
                                 ProcessDocumentResponse* response) {
    return ProcessDoc(request->file_name(), request->doc_content(), request,
                      response);
  }

 protected:
  void Init(const ProcessorConfig& config);

  ProcessorConfig config_;
#ifndef MALDOCA_CHROME
  // Set up the pdf pipeline.
  absl::Status CreatePdfPipeline();
#endif
  // Set up the office pipeline.
  absl::Status CreateOfficePipeline();

#ifndef MALDOCA_CHROME
  // Object used to automatically identify file types.
  std::unique_ptr<FileTypeIdentifier> identifier_;
  // We use the same pdfium processor for all pdfs.
  PdfiumProcessor pdfium_processor_;
  // We use the same adobe api mapper for all pdfs.
  AdobeAPIMapper api_mapper_;
  // Pipeline used for pdf processing.
  ProcessingPipeline pdf_pipeline_;
#endif

  // Pipeline used for office processing.
  ProcessingPipeline office_pipeline_;
  // Holder for the processors
  std::vector<std::unique_ptr<Handler<absl::string_view, ParsedDocument>>>
      office_parser_vector_;
  std::vector<std::unique_ptr<Handler<ParsedDocument, DocumentFeatures>>>
      office_feature_extractor_vector_;
  std::vector<std::unique_ptr<Handler<DocumentFeatures, ExportedFeatures>>>
      office_feature_export_vector_;
  std::vector<std::unique_ptr<Handler<absl::string_view, ParsedDocument>>>
      pdf_parser_vector_;
  std::vector<std::unique_ptr<Handler<ParsedDocument, DocumentFeatures>>>
      pdf_feature_extractor_vector_;
  std::vector<std::unique_ptr<Handler<DocumentFeatures, ExportedFeatures>>>
      pdf_feature_export_vector_;
  std::unique_ptr<ParserHandlerFactory> parser_factory_;
  std::unique_ptr<FeatureExtractorHandlerFactory> extractor_factory_;
  std::unique_ptr<FeatureExportHandlerFactory> export_factory_;
};
}  // namespace maldoca

#endif  // MALDOCA_SERVICE_COMMON_PROCESS_DOC_H_
