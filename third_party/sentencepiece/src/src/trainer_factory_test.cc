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

#include "trainer_factory.h"
#include "testharness.h"

namespace sentencepiece {

TEST(TrainerFactoryTest, BasicTest) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;

  trainer_spec.set_model_prefix("model");
  trainer_spec.add_input("input");

  {
    trainer_spec.set_model_type(TrainerSpec::UNIGRAM);
    auto m = TrainerFactory::Create(trainer_spec, normalizer_spec,
                                    denormalizer_spec);
  }

  {
    trainer_spec.set_model_type(TrainerSpec::BPE);
    auto m = TrainerFactory::Create(trainer_spec, normalizer_spec,
                                    denormalizer_spec);
  }

  {
    trainer_spec.set_model_type(TrainerSpec::WORD);
    auto m = TrainerFactory::Create(trainer_spec, normalizer_spec,
                                    denormalizer_spec);
  }

  {
    trainer_spec.set_model_type(TrainerSpec::CHAR);
    auto m = TrainerFactory::Create(trainer_spec, normalizer_spec,
                                    denormalizer_spec);
  }
}
}  // namespace sentencepiece
