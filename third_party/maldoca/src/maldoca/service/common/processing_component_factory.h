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

#ifndef THIRD_PARTY_MALDOCA_SERVICE_COMMON_PROCESSING_COMPONENT_FACTORY_H_
#define THIRD_PARTY_MALDOCA_SERVICE_COMMON_PROCESSING_COMPONENT_FACTORY_H_

#ifndef MALDOCA_CHROME
#include "maldoca/pdf_parser/adobe_api_mapper.h"
#include "maldoca/pdf_parser/pdfium_processor.h"
#endif

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "maldoca/service/common/processing_component.h"

namespace maldoca {
// Factory class for making handlers.
template <typename Handler, typename Configuration>
class AbstractHandlerFactory {
 public:
  AbstractHandlerFactory() = default;
  virtual ~AbstractHandlerFactory() = default;

  virtual StatusOr<std::unique_ptr<Handler>> Build(
      absl::string_view name, const Configuration& config) = 0;
};

class ParserHandlerFactory
    : public AbstractHandlerFactory<ParserHandler, ParserConfig> {
 public:
  ParserHandlerFactory() = default;
  ~ParserHandlerFactory() override = default;

  StatusOr<std::unique_ptr<ParserHandler>> Build(
      absl::string_view name, const ParserConfig& config) override;
#ifndef MALDOCA_CHROME
  void SetPdfiumProcessor(PdfiumProcessor* p) { pdfium_ = p; }

 protected:
  PdfiumProcessor* pdfium_ = nullptr;
#endif
};

class FeatureExtractorHandlerFactory
    : public AbstractHandlerFactory<FeatureExtractorHandler,
                                    FeatureExtractorConfig> {
 public:
  FeatureExtractorHandlerFactory() = default;
  ~FeatureExtractorHandlerFactory() override = default;

  StatusOr<std::unique_ptr<FeatureExtractorHandler>> Build(
      absl::string_view name, const FeatureExtractorConfig& config) override;
#ifndef MALDOCA_CHROME

  void SetAdobeAPIMapper(AdobeAPIMapper* p) { api_mapper_ = p; }

 protected:
  AdobeAPIMapper* api_mapper_ = nullptr;
#endif
};

class FeatureExportHandlerFactory
    : public AbstractHandlerFactory<FeatureExportHandler, FeatureExportConfig> {
 public:
  FeatureExportHandlerFactory() = default;
  ~FeatureExportHandlerFactory() override = default;

  StatusOr<std::unique_ptr<FeatureExportHandler>> Build(
      absl::string_view name, const FeatureExportConfig& config) override;
};

}  // namespace maldoca
#endif  // THIRD_PARTY_MALDOCA_SERVICE_COMMON_PROCESSING_COMPONENT_FACTORY_H_
