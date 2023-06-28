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

#include "src/unigram_model_trainer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "src/sentencepiece_model.pb.h"
#include "src/sentencepiece_processor.h"
#include "src/sentencepiece_trainer.h"
#include "src/util.h"

namespace sentencepiece {
namespace unigram {
namespace {

// Space symbol
#define WS "\xe2\x96\x81"

TEST(UnigramTrainerTest, TrainerModelTest) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  const TrainerModel model(trainer_spec, normalizer_spec);
  EXPECT_EQ(EncodeResult(), model.Encode("test"));
}

static constexpr char kTestInputData[] =
    "src/test_data/wagahaiwa_nekodearu.txt";

TEST(UnigramTrainerTest, EndToEndTest) {
  const std::string input = absl::StrCat(getenv("TEST_SRCDIR"), kTestInputData);

  ASSERT_TRUE(
      SentencePieceTrainer::Train(
          absl::StrCat("--model_prefix=", getenv("TEST_TMPDIR"), "/tmp_model",
                       " --input=", input,
                       " --vocab_size=8000 --normalization_rule_name=identity",
                       " --model_type=unigram --user_defined_symbols=<user>",
                       " --control_symbols=<ctrl> --max_sentence_length=2048"))
          .ok());

  SentencePieceProcessor sp;
  EXPECT_TRUE(
      sp.Load(absl::StrCat(getenv("TEST_TMPDIR"), "/tmp_model.model")).ok());
  EXPECT_EQ(8000, sp.GetPieceSize());

  const int cid = sp.PieceToId("<ctrl>");
  const int uid = sp.PieceToId("<user>");
  EXPECT_TRUE(sp.IsControl(cid));
  EXPECT_FALSE(sp.IsUnknown(uid));

  std::vector<std::string> tok;

  EXPECT_TRUE(sp.Encode("", &tok).ok());
  EXPECT_TRUE(tok.empty());

  EXPECT_TRUE(sp.Encode("吾輩《わがはい》は猫である。名前はまだ無い。"
                        "どこで生れたかとんと見当《けんとう》がつかぬ。"
                        "何でも薄暗いじめじめした所でニャーニャー泣いていた事だ"
                        "けは記憶している"
                        "。",
                        &tok)
                  .ok());
  // TODO(taku): Temporally disable this test on Windows.
#ifndef OS_WIN
  EXPECT_EQ(WS
            " 吾輩 《 わが はい 》 は 猫 である 。 名前 はまだ 無い 。 "
            "どこ で 生 れた か とん と 見当 《 けん とう 》 が つか ぬ 。 "
            "何でも 薄 暗 い じめ じめ した 所で ニャーニャー "
            "泣 い ていた 事 だけは 記憶 している 。",
            absl::StrJoin(tok, " "));
#endif
}

}  // namespace
}  // namespace unigram
}  // namespace sentencepiece
