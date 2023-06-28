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

#ifndef BPE_MODEL_H_
#define BPE_MODEL_H_

#include "src/model_interface.h"
#include "src/sentencepiece_model.pb.h"

namespace sentencepiece {
namespace bpe {

// Segmentation model with BPE (Byte Pair Encoding)
// Details:
// Neural Machine Translation of Rare Words with Subword Units
// https://arxiv.org/abs/1508.07909
//
// https://en.wikipedia.org/wiki/Byte_pair_encoding
class Model : public ModelInterface {
 public:
  explicit Model(const ModelProto &model_proto);
  ~Model() override;

  EncodeResult Encode(absl::string_view normalized) const override;
};
}  // namespace bpe
}  // namespace sentencepiece
#endif  // BPE_MODEL_H_
