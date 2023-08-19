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
#include "bpe_model_trainer.h"
#include "filesystem.h"
#include "sentencepiece_processor.h"
#include "sentencepiece_trainer.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {
namespace bpe {
namespace {

// Space symbol
#define WS "\xe2\x96\x81"

std::string RunTrainer(
    const std::vector<std::string> &input, int size,
    const std::vector<std::string> &user_defined_symbols = {}) {
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
  trainer_spec.set_model_type(TrainerSpec::BPE);
  trainer_spec.add_input(input_file);
  trainer_spec.set_vocab_size(size - 3);  // remove <unk>, <s>, </s>
  trainer_spec.set_model_prefix(model_prefix);

  NormalizerSpec normalizer_spec;
  normalizer_spec.set_name("identity");
  normalizer_spec.set_add_dummy_prefix(false);

  NormalizerSpec denormalizer_spec;

  for (const auto &w : user_defined_symbols) {
    trainer_spec.add_user_defined_symbols(w);
  }

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

TEST(BPETrainerTest, BasicTest) {
  EXPECT_EQ("ab ra abra ad cad abracad abracadabra ac br a b r c d",
            RunTrainer({"abracadabra"}, 20));
  EXPECT_EQ("ap le app apple en in ine pen p e a l n i",
            RunTrainer({"pen", "pineapple", "apple"}, 20));
  EXPECT_EQ("he ll llo hello hellohe el lo oh hel ohe e h l o",
            RunTrainer({"hellohe"}, 20));
  EXPECT_EQ("app le en in ine pen pine ne pe e l n p i",
            RunTrainer({"pen", "pineapple", "apple"}, 20, {"app"}));
}

static constexpr char kTestInputData[] = "wagahaiwa_nekodearu.txt";

TEST(BPETrainerTest, EndToEndTest) {
  const std::string input =
      util::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kTestInputData);

  ASSERT_TRUE(
      SentencePieceTrainer::Train(
          absl::StrCat(
              "--model_prefix=",
              util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "tmp_model"),
              " --input=", input,
              " --vocab_size=8000 --normalization_rule_name=identity"
              " --model_type=bpe --control_symbols=<ctrl> "
              "--max_sentence_length=2048"))
          .ok());

  SentencePieceProcessor sp;
  ASSERT_TRUE(sp.Load(std::string(util::JoinPath(
                          absl::GetFlag(FLAGS_test_tmpdir), "tmp_model.model")))
                  .ok());
  EXPECT_EQ(8000, sp.GetPieceSize());

  const int cid = sp.PieceToId("<ctrl>");
  EXPECT_TRUE(sp.IsControl(cid));

  std::vector<std::string> tok;
  ASSERT_TRUE(sp.Encode("", &tok).ok());
  ASSERT_TRUE(tok.empty());

  EXPECT_TRUE(sp.Encode("吾輩《わがはい》は猫である。名前はまだ無い。"
                        "どこで生れたかとんと見当《けんとう》がつかぬ。"
                        "何でも薄暗いじめじめした所でニャーニャー泣いていた事だ"
                        "けは記憶している"
                        "。",
                        &tok)
                  .ok());
  EXPECT_EQ(WS
            " 吾輩 《 わが はい 》 は猫 である 。 名前 はまだ 無い 。 "
            "どこで 生 れた か とん と見 当 《 けんとう 》 が つかぬ 。 "
            "何でも 薄 暗 いじ め じ め した 所で ニャー ニャー 泣 いていた "
            "事 だけは 記憶 している 。",
            absl::StrJoin(tok, " "));
}

}  // namespace
}  // namespace bpe
}  // namespace sentencepiece
