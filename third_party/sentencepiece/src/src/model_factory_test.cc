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
#include "testharness.h"

namespace sentencepiece {

TEST(ModelFactoryTest, BasicTest) {
  ModelProto model_proto;

  auto *sp1 = model_proto.add_pieces();
  auto *sp2 = model_proto.add_pieces();
  auto *sp3 = model_proto.add_pieces();

  sp1->set_type(ModelProto::SentencePiece::UNKNOWN);
  sp1->set_piece("<unk>");
  sp2->set_type(ModelProto::SentencePiece::CONTROL);
  sp2->set_piece("<s>");
  sp3->set_type(ModelProto::SentencePiece::CONTROL);
  sp3->set_piece("</s>");

  auto *sp4 = model_proto.add_pieces();
  sp4->set_piece("test");
  sp4->set_score(1.0);

  {
    model_proto.mutable_trainer_spec()->set_model_type(TrainerSpec::UNIGRAM);
    auto m = ModelFactory::Create(model_proto);
  }

  {
    model_proto.mutable_trainer_spec()->set_model_type(TrainerSpec::BPE);
    auto m = ModelFactory::Create(model_proto);
  }

  {
    model_proto.mutable_trainer_spec()->set_model_type(TrainerSpec::WORD);
    auto m = ModelFactory::Create(model_proto);
  }

  {
    model_proto.mutable_trainer_spec()->set_model_type(TrainerSpec::CHAR);
    auto m = ModelFactory::Create(model_proto);
  }
}
}  // namespace sentencepiece
