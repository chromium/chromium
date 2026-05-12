// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#include "util.h"
#include "word_model.h"

namespace sentencepiece {
namespace word {

Model::Model(const ModelProto &model_proto) {
  model_proto_ = &model_proto;
  InitializePieces();
}

Model::~Model() {}

EncodeResult Model::Encode(absl::string_view normalized) const {
  if (!status().ok() || normalized.empty()) {
    return {};
  }

  EncodeResult output;
  for (const auto &w : SplitIntoWords(normalized)) {
    output.emplace_back(w, PieceToId(w));
  }

  return output;
}

}  // namespace word
}  // namespace sentencepiece
