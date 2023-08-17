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
#include "absl/strings/str_cat.h"
#include "filesystem.h"
#include "sentencepiece_model.pb.h"
#include "testharness.h"
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

void CheckNormalizer(absl::string_view filename,
                     bool expected_has_normalizer,
                     bool expected_has_denormalizer) {
  SentencePieceProcessor sp;
  ASSERT_TRUE(sp.Load(filename.data()).ok());
  const auto& normalizer_spec = sp.model_proto().normalizer_spec();
  const auto& denormalizer_spec = sp.model_proto().denormalizer_spec();
  EXPECT_EQ(!normalizer_spec.precompiled_charsmap().empty(),
            expected_has_normalizer);
  EXPECT_EQ(!denormalizer_spec.precompiled_charsmap().empty(),
            expected_has_denormalizer);
}

TEST(SentencePieceTrainerTest, TrainFromArgsTest) {
  const std::string input =
      util::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kTestData);
  const std::string model =
      util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "m");

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
    explicit VectorIterator(std::vector<std::string>&& vec)
        : vec_(std::move(vec)) {}

    bool done() const override { return idx_ == vec_.size(); }
    void Next() override { ++idx_; }
    const std::string& value() const override { return vec_[idx_]; }
    util::Status status() const override { return util::OkStatus(); }

   private:
    std::vector<std::string> vec_;
    size_t idx_ = 0;
  };

  const std::string input =
      util::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kTestData);
  const std::string model =
      util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "m");

  std::vector<std::string> sentences;
  {
    auto fs = filesystem::NewReadableFile(input);
    CHECK_OK(fs->status());
    std::string line;
    while (fs->ReadLine(&line)) {
      sentences.emplace_back(line);
    }
  }

  VectorIterator it(std::move(sentences));
  ASSERT_TRUE(
      SentencePieceTrainer::Train(
          absl::StrCat("--model_prefix=", model, " --vocab_size=1000"), &it)
          .ok());
  CheckVocab(model + ".model", 1000);
  CheckNormalizer(model + ".model", true, false);
}

TEST(SentencePieceTrainerTest, TrainWithCustomNormalizationRule) {
  std::string input =
      util::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kTestData);
  std::string rule =
      util::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kNfkcTestData);
  const std::string model =
      util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "m");

  EXPECT_TRUE(SentencePieceTrainer::Train(
                  absl::StrCat("--input=", input, " --model_prefix=", model,
                               " --vocab_size=1000 ",
                               "--normalization_rule_tsv=", rule))
                  .ok());
  CheckNormalizer(model + ".model", true, false);
}

TEST(SentencePieceTrainerTest, TrainWithCustomDenormalizationRule) {
  const std::string input_file =
      util::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kTestDataJa);
  const std::string model =
      util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "m");
  const std::string norm_rule_tsv =
      util::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kIdsNormTsv);
  const std::string denorm_rule_tsv =
      util::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kIdsDenormTsv);
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
  trainer_spec.add_input(
      util::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kTestData));
  trainer_spec.set_model_prefix(
      util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "m"));
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

}  // namespace
}  // namespace sentencepiece
