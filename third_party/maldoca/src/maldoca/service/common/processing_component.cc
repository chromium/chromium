
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

#include "maldoca/service/common/processing_component.h"

namespace maldoca {

const ParserConfig &GetDefaultParserConfig() {
  static const ParserConfig *config = new ParserConfig();
  return *config;
}

const FeatureExtractorConfig &GetDefaultFeatureExtractorConfig() {
  static const FeatureExtractorConfig *config = new FeatureExtractorConfig();
  return *config;
}

const FeatureExportConfig &GetDefaultFeatureExportConfig() {
  static const FeatureExportConfig *config = new FeatureExportConfig();
  return *config;
}

const ProcessorConfig &GetDefaultProcessorConfig() {
  static const ProcessorConfig *config = new ProcessorConfig();
  return *config;
}
}  // namespace maldoca
