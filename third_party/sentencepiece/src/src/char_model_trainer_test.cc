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

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "char_model_trainer.h"
#include "filesystem.h"
#include "sentencepiece_processor.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {
namespace character {
namespace {

// Space symbol (U+2581)
#define WS "\xE2\x96\x81"

std::string RunTrainer(const std::vector<std::string> &input, int size) {
  const std::string input_file =
      util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "input");
  const std::string model_prefix =
      util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "model");
  {
    auto output = filesystem::NewWritableFile(input_file);
    for (const auto &line : input) {
      output->WriteLine(line);
    }
  }

  TrainerSpec trainer_spec;
  trainer_spec.set_model_type(TrainerSpec::CHAR);
  trainer_spec.add_input(input_file);
  trainer_spec.set_vocab_size(size);
  trainer_spec.set_model_prefix(model_prefix);

  NormalizerSpec normalizer_spec;
  normalizer_spec.set_name("identity");

  NormalizerSpec denormalizer_spec;

  Trainer trainer(trainer_spec, normalizer_spec, denormalizer_spec);
  EXPECT_TRUE(trainer.Train().ok());

  SentencePieceProcessor processor;
  EXPECT_TRUE(processor.Load(model_prefix + ".model").ok());

  const auto &model = processor.model_proto();
  std::vector<std::string> pieces;

  // remove <unk>, <s>, </s>
  for (int i = 3; i < model.pieces_size(); ++i) {
    pieces.emplace_back(model.pieces(i).piece());
  }

  return absl::StrJoin(pieces, " ");
}

TEST(TrainerTest, BasicTest) {
  EXPECT_EQ(WS " a e p n I h l v",
            RunTrainer({"I have a pen", "I have an apple", "apple pen"}, 100));
  EXPECT_EQ(WS " a",  // <unk>, <s>, </s>, _, a
            RunTrainer({"I have a pen", "I have an apple", "apple pen"}, 5));
}

}  // namespace
}  // namespace character
}  // namespace sentencepiece
