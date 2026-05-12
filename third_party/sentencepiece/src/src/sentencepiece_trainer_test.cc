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

#include "sentencepiece_trainer.h"

#include "filesystem.h"
#include "sentencepiece_model.pb.h"
#include "testharness.h"
#include "absl/strings/str_cat.h"
#include "util.h"

namespace sentencepiece {
namespace {

static constexpr char kTestData[] = "botchan.txt";
static constexpr char kNfkcTestData[] = "nfkc.tsv";
static constexpr char kTestDataJa[] = "wagahaiwa_nekodearu.txt";
static constexpr char kIdsNormTsv[] = "ids_norm.tsv";
static constexpr char kIdsDenormTsv[] = "ids_denorm.tsv";

void CheckVocab(absl::string_view filename, int expected_vocab_size) {
  SentencePieceProcessor sp;
  ASSERT_TRUE(sp.Load(filename.data()).ok());
  EXPECT_EQ(expected_vocab_size, sp.model_proto().trainer_spec().vocab_size());
  EXPECT_EQ(sp.model_proto().pieces_size(),
            sp.model_proto().trainer_spec().vocab_size());
}

void CheckNormalizer(absl::string_view filename, bool expected_has_normalizer,
                     bool expected_has_denormalizer) {
  SentencePieceProcessor sp;
  ASSERT_TRUE(sp.Load(filename.data()).ok());
  const auto &normalizer_spec = sp.model_proto().normalizer_spec();
  const auto &denormalizer_spec = sp.model_proto().denormalizer_spec();
  EXPECT_EQ(!normalizer_spec.precompiled_charsmap().empty(),
            expected_has_normalizer);
  EXPECT_EQ(!denormalizer_spec.precompiled_charsmap().empty(),
            expected_has_denormalizer);
}

TEST(SentencePieceTrainerTest, TrainFromArgsTest) {
  const std::string input = util::JoinPath(::testing::SrcDir(), kTestData);
  const std::string model = util::JoinPath(::testing::TempDir(), "m");

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

TEST(SentencePieceTrainerTest, TrainFromIterator) {
  class VectorIterator : public SentenceIterator {
   public:
    explicit VectorIterator(std::vector<std::string> &&vec)
        : vec_(std::move(vec)) {}

    bool done() const override { return idx_ == vec_.size(); }
    void Next() override { ++idx_; }
    const std::string &value() const override { return vec_[idx_]; }
    util::Status status() const override { return util::OkStatus(); }

   private:
    std::vector<std::string> vec_;
    size_t idx_ = 0;
  };

  const std::string input = util::JoinPath(::testing::SrcDir(), kTestData);
  const std::string model = util::JoinPath(::testing::TempDir(), "m");

  std::vector<std::string> sentences;
  {
    auto fs = filesystem::NewReadableFile(input);
    ABSL_CHECK_OK(fs->status());
    std::string line;
    while (fs->ReadLine(&line)) sentences.emplace_back(line);
  }

  ASSERT_TRUE(SentencePieceTrainer::Train(
                  absl::StrCat("--model_prefix=", model, " --vocab_size=1000"),
                  sentences)
                  .ok());
  CheckVocab(model + ".model", 1000);
  CheckNormalizer(model + ".model", true, false);

  ASSERT_TRUE(SentencePieceTrainer::Train(
                  {{"model_prefix", model}, {"vocab_size", "1000"}}, sentences)
                  .ok());
  CheckVocab(model + ".model", 1000);
  CheckNormalizer(model + ".model", true, false);

  VectorIterator it(std::move(sentences));
  ASSERT_TRUE(
      SentencePieceTrainer::Train(
          absl::StrCat("--model_prefix=", model, " --vocab_size=1000"), &it)
          .ok());
  CheckVocab(model + ".model", 1000);
  CheckNormalizer(model + ".model", true, false);
}

TEST(SentencePieceTrainerTest, TrainWithCustomNormalizationRule) {
  std::string input = util::JoinPath(::testing::SrcDir(), kTestData);
  std::string rule = util::JoinPath(::testing::SrcDir(), kNfkcTestData);
  const std::string model = util::JoinPath(::testing::TempDir(), "m");

  EXPECT_TRUE(SentencePieceTrainer::Train(
                  absl::StrCat("--input=", input, " --model_prefix=", model,
                               " --vocab_size=1000 ",
                               "--normalization_rule_tsv=", rule))
                  .ok());
  CheckNormalizer(model + ".model", true, false);
}

TEST(SentencePieceTrainerTest, TrainWithCustomDenormalizationRule) {
  const std::string input_file =
      util::JoinPath(::testing::SrcDir(), kTestDataJa);
  const std::string model = util::JoinPath(::testing::TempDir(), "m");
  const std::string norm_rule_tsv =
      util::JoinPath(::testing::SrcDir(), kIdsNormTsv);
  const std::string denorm_rule_tsv =
      util::JoinPath(::testing::SrcDir(), kIdsDenormTsv);
  EXPECT_TRUE(
      SentencePieceTrainer::Train(
          absl::StrCat("--input=", input_file, " --model_prefix=", model,
                       " --vocab_size=1000 --model_type=unigram "
                       "--normalization_rule_tsv=",
                       norm_rule_tsv,
                       " --denormalization_rule_tsv=", denorm_rule_tsv))
          .ok());
  CheckNormalizer(model + ".model", true, true);
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
  trainer_spec.add_input(util::JoinPath(::testing::SrcDir(), kTestData));
  trainer_spec.set_model_prefix(util::JoinPath(::testing::TempDir(), "m"));
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

    // CSV
    spec.Clear();
    ASSERT_TRUE(
        SentencePieceTrainer::SetProtoField("input", "\"foo,bar\",buz", &spec)
            .ok());
    EXPECT_EQ(2, spec.input_size());
    EXPECT_EQ("foo,bar", spec.input(0));
    EXPECT_EQ("buz", spec.input(1));

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
  NormalizerSpec denormalizer_spec;
  EXPECT_FALSE(
      SentencePieceTrainer::MergeSpecsFromArgs("", nullptr, nullptr, nullptr)
          .ok());

  ASSERT_TRUE(SentencePieceTrainer::MergeSpecsFromArgs(
                  "", &trainer_spec, &normalizer_spec, &denormalizer_spec)
                  .ok());

  EXPECT_FALSE(
      SentencePieceTrainer::MergeSpecsFromArgs(
          "--unknown=BPE", &trainer_spec, &normalizer_spec, &denormalizer_spec)
          .ok());

  EXPECT_FALSE(SentencePieceTrainer::MergeSpecsFromArgs(
                   "--vocab_size=UNK", &trainer_spec, &normalizer_spec,
                   &denormalizer_spec)
                   .ok());

  EXPECT_FALSE(SentencePieceTrainer::MergeSpecsFromArgs(
                   "--model_type=UNK", &trainer_spec, &normalizer_spec,
                   &denormalizer_spec)
                   .ok());

  ASSERT_TRUE(SentencePieceTrainer::MergeSpecsFromArgs(
                  "--model_type=bpe", &trainer_spec, &normalizer_spec,
                  &denormalizer_spec)
                  .ok());

  ASSERT_TRUE(SentencePieceTrainer::MergeSpecsFromArgs(
                  "--split_by_whitespace", &trainer_spec, &normalizer_spec,
                  &denormalizer_spec)
                  .ok());

  ASSERT_TRUE(SentencePieceTrainer::MergeSpecsFromArgs(
                  "--normalization_rule_name=foo", &trainer_spec,
                  &normalizer_spec, &denormalizer_spec)
                  .ok());
  EXPECT_EQ("foo", normalizer_spec.name());

  ASSERT_TRUE(SentencePieceTrainer::MergeSpecsFromArgs(
                  "--normalization_rule_tsv=foo.tsv", &trainer_spec,
                  &normalizer_spec, &denormalizer_spec)
                  .ok());
  EXPECT_EQ("foo.tsv", normalizer_spec.normalization_rule_tsv());

  ASSERT_TRUE(SentencePieceTrainer::MergeSpecsFromArgs(
                  "--denormalization_rule_tsv=bar.tsv", &trainer_spec,
                  &normalizer_spec, &denormalizer_spec)
                  .ok());
  EXPECT_EQ("bar.tsv", denormalizer_spec.normalization_rule_tsv());

  EXPECT_FALSE(SentencePieceTrainer::MergeSpecsFromArgs(
                   "--vocab_size=UNK", &trainer_spec, &normalizer_spec,
                   &denormalizer_spec)
                   .ok());
}

TEST(SentencePieceTrainerTest, PopulateModelTypeFromStringTest) {
  TrainerSpec spec;
  EXPECT_TRUE(
      SentencePieceTrainer::PopulateModelTypeFromString("unigram", &spec).ok());
  EXPECT_EQ(TrainerSpec::UNIGRAM, spec.model_type());
  EXPECT_TRUE(
      SentencePieceTrainer::PopulateModelTypeFromString("bpe", &spec).ok());
  EXPECT_EQ(TrainerSpec::BPE, spec.model_type());
  EXPECT_TRUE(
      SentencePieceTrainer::PopulateModelTypeFromString("word", &spec).ok());
  EXPECT_EQ(TrainerSpec::WORD, spec.model_type());
  EXPECT_TRUE(
      SentencePieceTrainer::PopulateModelTypeFromString("char", &spec).ok());
  EXPECT_EQ(TrainerSpec::CHAR, spec.model_type());
  EXPECT_FALSE(
      SentencePieceTrainer::PopulateModelTypeFromString("", &spec).ok());
}

TEST(SentencePieceTrainerTest, NormalizationTest) {
  const auto model_prefix = util::JoinPath(::testing::TempDir(), "m");
  const auto model_file = absl::StrCat(model_prefix, ".model");

  TrainerSpec trainer_spec;
  trainer_spec.add_input(util::JoinPath(::testing::SrcDir(), kTestData));
  trainer_spec.set_model_prefix(model_prefix);
  trainer_spec.set_vocab_size(1000);
  ASSERT_TRUE(SentencePieceTrainer::Train(trainer_spec).ok());

  constexpr absl::string_view kInput = "ＫＡＤＯＫＡＷＡ   ABC ";

  {
    SentencePieceProcessor sp;
    EXPECT_OK(sp.Load(model_file));
    EXPECT_EQ(sp.Normalize(kInput), "▁KADOKAWA▁ABC");

    std::string normalized;
    std::vector<size_t> offsets;

    EXPECT_OK(sp.Normalize(kInput, &normalized, &offsets));
    EXPECT_EQ(normalized, "▁KADOKAWA▁ABC");
    EXPECT_EQ(offsets, std::vector<size_t>({0, 0, 0, 0, 3, 6, 9, 12, 15, 18, 21,
                                            24, 24, 24, 27, 28, 29, 30}));
    ConvertToUnicodeAlignment(kInput, normalized, &offsets);
    EXPECT_EQ(offsets, std::vector<size_t>(
                           {0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14}));

    EXPECT_OK(sp.Normalize("㍻元年", &normalized, &offsets));
    EXPECT_EQ(normalized, "▁平成元年");
    ConvertToUnicodeAlignment(kInput, normalized, &offsets);
    EXPECT_EQ(offsets, std::vector<size_t>({0, 0, 0, 1, 2, 3}));

    EXPECT_OK(sp.Normalize("ｶﾞｲﾀﾞﾝｽ", &normalized, &offsets));
    EXPECT_EQ(normalized, "▁ガイダンス");
    ConvertToUnicodeAlignment(kInput, normalized, &offsets);
    EXPECT_EQ(offsets, std::vector<size_t>({0, 0, 2, 3, 5, 6, 7}));
  }

  auto set_normalization_only = [](SentencePieceNormalizer *normalizer) {
    SentencePieceTrainer::SetProtoField("add_dummy_prefix", "false",
                                        normalizer->mutable_normalizer_spec());
    SentencePieceTrainer::SetProtoField("escape_whitespaces", "false",
                                        normalizer->mutable_normalizer_spec());
    SentencePieceTrainer::SetProtoField("remove_extra_whitespaces", "false",
                                        normalizer->mutable_normalizer_spec());
  };

  {
    SentencePieceNormalizer sp;
    EXPECT_OK(sp.Load(model_file));
    set_normalization_only(&sp);
    EXPECT_EQ(sp.Normalize(kInput), "KADOKAWA   ABC ");
  }

  {
    SentencePieceNormalizer sp;
    EXPECT_OK(
        sp.LoadFromRuleTSV(util::JoinPath(::testing::SrcDir(), "nfkc_cf.tsv")));
    set_normalization_only(&sp);
    EXPECT_EQ(sp.Normalize("ABCD"), "abcd");
  }

  {
    SentencePieceNormalizer sp;
    EXPECT_FALSE(sp.LoadFromRuleTSV("__unknown__").ok());
  }

  {
    SentencePieceNormalizer sp;
    EXPECT_OK(sp.LoadFromRuleName("nfkc_cf"));
    set_normalization_only(&sp);
    EXPECT_EQ(sp.Normalize("ABCD"), "abcd");
  }

  {
    SentencePieceNormalizer sp;
    EXPECT_OK(sp.LoadFromRuleName("identity"));
    set_normalization_only(&sp);
    EXPECT_EQ(sp.Normalize("ＡＢＣＤ"), "ＡＢＣＤ");
  }

  {
    SentencePieceNormalizer sp;
    EXPECT_FALSE(sp.LoadFromRuleName("__unknown__").ok());
  }
}

}  // namespace
}  // namespace sentencepiece
