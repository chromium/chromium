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

#include "model_factory.h"
#include "absl/memory/memory.h"
#include "bpe_model.h"
#include "char_model.h"
#include "unigram_model.h"
#include "word_model.h"

namespace sentencepiece {

// Instantiate Model instance from |model_proto|
std::unique_ptr<ModelInterface> ModelFactory::Create(
    const ModelProto& model_proto) {
  const auto& trainer_spec = model_proto.trainer_spec();

  switch (trainer_spec.model_type()) {
    case TrainerSpec::UNIGRAM:
      return absl::make_unique<unigram::Model>(model_proto);
      break;
    case TrainerSpec::BPE:
      return absl::make_unique<bpe::Model>(model_proto);
      break;
    case TrainerSpec::WORD:
      return absl::make_unique<word::Model>(model_proto);
      break;
    case TrainerSpec::CHAR:
      return absl::make_unique<character::Model>(model_proto);
      break;
    default:
      LOG(ERROR) << "Unknown model_type: " << trainer_spec.model_type();
      return nullptr;
      break;
  }

  return absl::make_unique<unigram::Model>(model_proto);
}
}  // namespace sentencepiece
