// Copyright 2018 Google Inc.
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

#include "src/sentencepiece_trainer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/strings/str_cat.h"
#include "src/sentencepiece_model.pb.h"
#include "src/util.h"

namespace sentencepiece {
namespace {

static constexpr char kTestData[] =
    "src/test_data/botchan.txt";
static constexpr char kNfkcTestData[] =
    "src/test_data/nfkc.tsv";

void CheckVocab(absl::string_view filename, int expected_vocab_size) {
  SentencePieceProcessor sp;
  ASSERT_TRUE(sp.Load(filename.data()).ok());
  EXPECT_EQ(expected_vocab_size, sp.model_proto().trainer_spec().vocab_size());
  EXPECT_EQ(sp.model_proto().pieces_size(),
            sp.model_proto().trainer_spec().vocab_size());
}

TEST(SentencePieceTrainerTest, TrainFromArgsTest) {
  const std::string input = absl::StrCat(getenv("TEST_SRCDIR"), kTestData);
  const std::string model = absl::StrCat(getenv("TEST_TMPDIR"), "/m");

  ASSERT_TRUE(SentencePieceTrainer::Train(
                  absl::StrCat("--input=", input, " --model_prefix=", model,
                               " --vocab_size=1000"))
                  .ok());
  CheckVocab(model + ".model", 1000);

  ASSERT_TRUE(
      SentencePieceTrainer::Train(
          absl::StrCat("--input=", input, " --model_prefix=", model,
                       " --vocab_size=1000 --self_test_sample_size=100"))
          .ok());
  CheckVocab(model + ".model", 1000);

  ASSERT_TRUE(SentencePieceTrainer::Train(
                  absl::StrCat("--input=", input, " --model_prefix=", model,
                               " --vocab_size=1000 ", "--model_type=bpe"))
                  .ok());
  CheckVocab(model + ".model", 1000);

  ASSERT_TRUE(SentencePieceTrainer::Train(
                  absl::StrCat("--input=", input, " --model_prefix=", model,
                               " --vocab_size=1000 ", "--model_type=char"))
                  .ok());
  CheckVocab(model + ".model", 72);

  ASSERT_TRUE(SentencePieceTrainer::Train(
                  absl::StrCat("--input=", input, " --model_prefix=", model,
                               " --vocab_size=1000 ", "--model_type=word"))
                  .ok());
  CheckVocab(model + ".model", 1000);

  ASSERT_TRUE(SentencePieceTrainer::Train(
                  absl::StrCat("--input=", input, " --model_prefix=", model,
                               " --vocab_size=1000 ",
                               "--model_type=char --use_all_vocab=true"))
                  .ok());
  CheckVocab(model + ".model", 86);

  ASSERT_TRUE(SentencePieceTrainer::Train(
                  absl::StrCat("--input=", input, " --model_prefix=", model,
                               " --vocab_size=1000 ",
                               "--model_type=word --use_all_vocab=true"))
                  .ok());
  CheckVocab(model + ".model", 9186);
}

TEST(SentencePieceTrainerTest, TrainWithCustomNormalizationRule) {
  std::string input = absl::StrCat(getenv("TEST_SRCDIR"), kTestData);
  std::string rule = absl::StrCat(getenv("TEST_SRCDIR"), kNfkcTestData);
  const std::string model = absl::StrCat(getenv("TEST_TMPDIR"), "/m");

  SentencePieceTrainer::Train(
      absl::StrCat("--input=", input, " --model_prefix=", model,
                   "--vocab_size=1000 ", "--normalization_rule_tsv=", rule))
      .IgnoreError();
}

TEST(SentencePieceTrainerTest, TrainErrorTest) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  normalizer_spec.set_normalization_rule_tsv("foo.tsv");
  normalizer_spec.set_precompiled_charsmap("foo");
  EXPECT_FALSE(SentencePieceTrainer::Train(trainer_spec, normalizer_spec).ok());
}

TEST(SentencePieceTrainerTest, TrainTest) {
  TrainerSpec trainer_spec;
  trainer_spec.add_input(absl::StrCat(getenv("TEST_SRCDIR"), kTestData));
  trainer_spec.set_model_prefix(absl::StrCat(getenv("TEST_TMPDIR"), "/m"));
  trainer_spec.set_vocab_size(1000);
  NormalizerSpec normalizer_spec;
  ASSERT_TRUE(SentencePieceTrainer::Train(trainer_spec, normalizer_spec).ok());
  ASSERT_TRUE(SentencePieceTrainer::Train(trainer_spec).ok());
}

TEST(SentencePieceTrainerTest, SetProtoFieldTest) {
  {
    TrainerSpec spec;

    EXPECT_FALSE(
        SentencePieceTrainer::SetProtoField("dummy", "1000", &spec).ok());

    ASSERT_TRUE(
        SentencePieceTrainer::SetProtoField("vocab_size", "1000", &spec).ok());
    EXPECT_EQ(1000, spec.vocab_size());
    EXPECT_FALSE(
        SentencePieceTrainer::SetProtoField("vocab_size", "UNK", &spec).ok());

    ASSERT_TRUE(
        SentencePieceTrainer::SetProtoField("input_format", "TSV", &spec).ok());
    EXPECT_EQ("TSV", spec.input_format());
    ASSERT_TRUE(
        SentencePieceTrainer::SetProtoField("input_format", "123", &spec).ok());
    EXPECT_EQ("123", spec.input_format());

    ASSERT_TRUE(SentencePieceTrainer::SetProtoField("split_by_whitespace",
                                                    "false", &spec)
                    .ok());
    EXPECT_FALSE(spec.split_by_whitespace());
    ASSERT_TRUE(
        SentencePieceTrainer::SetProtoField("split_by_whitespace", "", &spec)
            .ok());
    EXPECT_TRUE(spec.split_by_whitespace());

    ASSERT_TRUE(
        SentencePieceTrainer::SetProtoField("character_coverage", "0.5", &spec)
            .ok());
    EXPECT_NEAR(spec.character_coverage(), 0.5, 0.001);
    EXPECT_FALSE(
        SentencePieceTrainer::SetProtoField("character_coverage", "UNK", &spec)
            .ok());

    ASSERT_TRUE(
        SentencePieceTrainer::SetProtoField("input", "foo,bar,buz", &spec)
            .ok());
    EXPECT_EQ(3, spec.input_size());
    EXPECT_EQ("foo", spec.input(0));
    EXPECT_EQ("bar", spec.input(1));
    EXPECT_EQ("buz", spec.input(2));

    ASSERT_TRUE(
        SentencePieceTrainer::SetProtoField("model_type", "BPE", &spec).ok());
    EXPECT_FALSE(
        SentencePieceTrainer::SetProtoField("model_type", "UNK", &spec).ok());
  }

  {
    NormalizerSpec spec;
    ASSERT_TRUE(
        SentencePieceTrainer::SetProtoField("add_dummy_prefix", "false", &spec)
            .ok());
    EXPECT_FALSE(spec.add_dummy_prefix());

    ASSERT_TRUE(SentencePieceTrainer::SetProtoField("escape_whitespaces",
                                                    "false", &spec)
                    .ok());
    EXPECT_FALSE(spec.escape_whitespaces());

    EXPECT_FALSE(
        SentencePieceTrainer::SetProtoField("dummy", "1000", &spec).ok());
  }
}

TEST(SentencePieceTrainerTest, MergeSpecsFromArgs) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  EXPECT_FALSE(
      SentencePieceTrainer::MergeSpecsFromArgs("", nullptr, nullptr).ok());

  ASSERT_TRUE(SentencePieceTrainer::MergeSpecsFromArgs("", &trainer_spec,
                                                       &normalizer_spec)
                  .ok());

  EXPECT_FALSE(SentencePieceTrainer::MergeSpecsFromArgs(
                   "--unknown=BPE", &trainer_spec, &normalizer_spec)
                   .ok());

  EXPECT_FALSE(SentencePieceTrainer::MergeSpecsFromArgs(
                   "--vocab_size=UNK", &trainer_spec, &normalizer_spec)
                   .ok());

  EXPECT_FALSE(SentencePieceTrainer::MergeSpecsFromArgs(
                   "--model_type=UNK", &trainer_spec, &normalizer_spec)
                   .ok());

  ASSERT_TRUE(SentencePieceTrainer::MergeSpecsFromArgs(
                  "--model_type=bpe", &trainer_spec, &normalizer_spec)
                  .ok());

  ASSERT_TRUE(SentencePieceTrainer::MergeSpecsFromArgs(
                  "--split_by_whitespace", &trainer_spec, &normalizer_spec)
                  .ok());

  ASSERT_TRUE(
      SentencePieceTrainer::MergeSpecsFromArgs("--normalization_rule_name=foo",
                                               &trainer_spec, &normalizer_spec)
          .ok());
  EXPECT_EQ("foo", normalizer_spec.name());

  EXPECT_FALSE(SentencePieceTrainer::MergeSpecsFromArgs(
                   "--vocab_size=UNK", &trainer_spec, &normalizer_spec)
                   .ok());
}

TEST(SentencePieceTrainerTest, PopulateModelTypeFromStringTest) {
  TrainerSpec spec;
  EXPECT_OK(
      SentencePieceTrainer::PopulateModelTypeFromString("unigram", &spec));
  EXPECT_EQ(TrainreSpec::ModelType::UNIGRAM, spec.model_type());
  EXPECT_OK(SentencePieceTrainer::PopulateModelTypeFromString("bpe", &spec));
  EXPECT_EQ(TrainreSpec::ModelType::BPE, spec.model_type());
  EXPECT_OK(SentencePieceTrainer::PopulateModelTypeFromString("word", &spec));
  EXPECT_EQ(TrainreSpec::ModelType::WORD, spec.model_type());
  EXPECT_OK(SentencePieceTrainer::PopulateModelTypeFromString("char", &spec));
  EXPECT_EQ(TrainreSpec::ModelType::CHAR, spec.model_type());
  EXPECT_NOT_OK(SentencePieceTrainer::PopulateModelTypeFromString("", &spec));
}
}  // namespace
}  // namespace sentencepiece
