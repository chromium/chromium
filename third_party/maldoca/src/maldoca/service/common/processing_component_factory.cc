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

#include "maldoca/service/common/processing_component_factory.h"

#include "maldoca/service/common/office_processing_component.h"

#ifndef MALDOCA_CHROME
#include "maldoca/service/common/pdf_processing_component.h"
#include "maldoca/service/common/vba_processing_component.h"
#endif

namespace maldoca {

// Do something simple for now -- hand code the mapping.
StatusOr<std::unique_ptr<ParserHandler>> ParserHandlerFactory::Build(
    absl::string_view name, const ParserConfig &config) {
  switch (config.handler_type()) {
    case HandlerConfig::DEFAULT_OFFICE_PARSER:
      DLOG(INFO) << "Making OfficeParserHandler";
      return std::make_unique<OfficeParserHandler>(name, config);

#ifndef MALDOCA_CHROME
    case HandlerConfig::DEFAULT_PDF_PARSER:
      CHECK(pdfium_ != nullptr);
      DLOG(INFO) << "Making PdfParserHandler";
      return std::make_unique<PdfParserHandler>(name, config, pdfium_);
    case HandlerConfig::DEFAULT_VBA_PARSER:
      DLOG(INFO) << "Making VbaParserHandler";
      return std::make_unique<VbaParserHandler>(name, config);
#endif

    default:
      return absl::InvalidArgumentError(
          absl::StrCat(name, " has an unknown handler type: ",
                       HandlerConfig::HandlerType_Name(config.handler_type())));
  }
}

StatusOr<std::unique_ptr<FeatureExtractorHandler>>
FeatureExtractorHandlerFactory::Build(absl::string_view name,
                                      const FeatureExtractorConfig &config) {
  switch (config.handler_type()) {
    case HandlerConfig::DEFAULT_OFFICE_FEATURE_EXTRACTOR:
      return std::make_unique<OfficeFeatureExtractorHandler>(name, config);

#ifndef MALDOCA_CHROME
    case HandlerConfig::DEFAULT_PDF_FEATURE_EXTRACTOR:
      CHECK(api_mapper_ != nullptr);
      return std::make_unique<PdfFeatureExtractorHandler>(name, config,
                                                          api_mapper_);
#endif

    default:
      return absl::InvalidArgumentError(
          absl::StrCat(name, " has an unknown handler type: ",
                       HandlerConfig::HandlerType_Name(config.handler_type())));
  }
}

StatusOr<std::unique_ptr<FeatureExportHandler>>
FeatureExportHandlerFactory::Build(absl::string_view name,
                                   const FeatureExportConfig &config) {
  switch (config.handler_type()) {
    case HandlerConfig::DEFAULT_OFFICE_EXPORT:
      return std::make_unique<OfficeFeatureExportHandler>(name, config);

#ifndef MALDOCA_CHROME
    case HandlerConfig::DEFAULT_PDF_EXPORT:
      return std::make_unique<PdfFeatureExportHandler>(name, config);
#endif

    default:
      return absl::InvalidArgumentError(
          absl::StrCat(name, " has an unknown handler type: ",
                       HandlerConfig::HandlerType_Name(config.handler_type())));
  }
}
}  // namespace maldoca
