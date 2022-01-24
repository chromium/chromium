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

// Main pipeline for handler execution. Defines a way to execute chained
// handlers. A pipeline is a chain of handlers. There are 3 types of handlers:
// parser, feature extracting, and feature exporting. A pipeline will never
// execute a feature extracting handler before a parser handler, or a feature
// exporting handler before a feature extraction handler. Also, this pipeline
// can hold both office handlers and pdf handlers, so one should pay attention
// not to combine pdf type handlers with office handlers in the same pipeline.
// The way this pipeline works and the handler chaining is very simple.
// Consider the following example:
//
//  ProcessingPipeline p;
//  ParserHandler p1, p2, p3;
//  FeatureExtractionHandler f1, f2, f3;
//  FeatureExportHandler e1, e2, e3;
//  p.Add(&p1);
//  p.Add(&f2);
//  p.Add(&e3);
//  p.Add(&e2);
//  p.Add(&p3);
//  p.Add(&e1);
//  p.Add(&p2);
//  p.Add(&f1);
//  p.Add(&f3);
// The code above arranges the handlers in the following order:
// p1->p3->p2->f2->f1->f3->e3->e2->e1
//
// In order to run the pipeline on a file, one can then simply run:
//  p.Process(document);
// where "document" is a string_view of the document that has to be processed.
// The results are saved in private fileds of the pipeline object:
// parsed_information_, extracted_features_, exported_features_, which can be
// accessed using the corresponding getter,
//
// Also keep in mind that the pipeline does not accept registering 2
// handlers (regardless of their types) with the same name.

#ifndef MALDOCA_SERVICE_COMMON_PROCESSING_PIPELINE_H_
#define MALDOCA_SERVICE_COMMON_PROCESSING_PIPELINE_H_

#include <fstream>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_set.h"
#ifndef MALDOCA_CHROME
#include "google/protobuf/message.h"  // nogncheck
#endif
#include "maldoca/service/common/processing_component.h"
#include "maldoca/service/proto/document_features.pb.h"
#include "maldoca/service/proto/exported_features.pb.h"
#include "maldoca/service/proto/parsed_document.pb.h"
#include "maldoca/service/proto/processing_config.pb.h"

namespace maldoca {
// This class is aimed at running the handlers and producing the 3 main results:
// a ParserPacket, FeaturesPacket and ExportedPackage, which are owned by this
// object.
class ProcessingPipeline {
 public:
  ProcessingPipeline() = default;

  // Runs all the handlers in this pipeline. The input is the string
  // representation of the pdf/office doc.
  absl::Status Process(absl::string_view input);

  // Add new Handlers to this pipeline. We define one method for each component
  // type. The caller owns the handler.
  absl::Status Add(
      const std::vector<
          std::unique_ptr<Handler<absl::string_view, ParsedDocument>>> &hdlrs);
  absl::Status Add(
      const std::vector<
          std::unique_ptr<Handler<ParsedDocument, DocumentFeatures>>> &hdlrs);
  absl::Status Add(
      const std::vector<
          std::unique_ptr<Handler<DocumentFeatures, ExportedFeatures>>> &hdlrs);

  // Add a single Handler to this pipeline. We define one method for each
  // component type. Caller owns the handler.
  absl::Status Add(Handler<absl::string_view, ParsedDocument> *hdl);
  absl::Status Add(Handler<ParsedDocument, DocumentFeatures> *hdl);
  absl::Status Add(Handler<DocumentFeatures, ExportedFeatures> *hdl);

  // We need this because we want to make changes to the ParsedDocument object
  // before starting the pipeline processing.
  ParsedDocument *GetMutableParsedDocument() {
    return parsed_information_.get();
  }

  // Returns a const reference to one of the 3 internal components owned
  // by this pipeline.
  const ParsedDocument &GetParsedDocument() { return *parsed_information_; }
  const DocumentFeatures &GetDocumentFeatures() { return *extracted_features_; }
  const ExportedFeatures &GetExportedFeatures() { return *exported_features_; }

  ParsedDocument *ReleaseParsedDocument() {
    return parsed_information_.release();
  }
  DocumentFeatures *ReleaseDocumentFeatures() {
    return extracted_features_.release();
  }
  ExportedFeatures *ReleaseExportedFeatures() {
    return exported_features_.release();
  }

  void ResetPipelineData();

 private:
  absl::Status RegisterName(absl::string_view name);
  // Members filled out by Process().
  std::unique_ptr<ParsedDocument> parsed_information_;
  std::unique_ptr<DocumentFeatures> extracted_features_;
  std::unique_ptr<ExportedFeatures> exported_features_;

  // Main components: parser, feature extractor, feature exporter.
  std::vector<Handler<absl::string_view, ParsedDocument> *> parser_;
  std::vector<Handler<ParsedDocument, DocumentFeatures> *> feature_extractor_;
  std::vector<Handler<DocumentFeatures, ExportedFeatures> *> feature_exporter_;

  // Making sure there are no name collisions
  absl::flat_hash_set<std::string> registration_set_;
};

}  // namespace maldoca

#endif  // MALDOCA_SERVICE_COMMON_PROCESSING_PIPELINE_H_
