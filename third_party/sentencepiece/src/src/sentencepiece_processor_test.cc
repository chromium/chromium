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

#include "sentencepiece_processor.h"

#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "builder.h"
#include "filesystem.h"
#include "model_interface.h"
#include "normalizer.h"
#include "sentencepiece.pb.h"
#include "sentencepiece_model.pb.h"
#include "sentencepiece_trainer.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {

// Space symbol
#define WS "\xe2\x96\x81"

class MockModel : public ModelInterface {
 public:
  void SetEncodeResult(absl::string_view input, const EncodeResult &output) {
    input_ = input;
    output_ = output;
  }

  void SetNBestEncodeResult(absl::string_view input,
                            const NBestEncodeResult &output) {
    input_ = input;
    nbest_output_ = output;
  }

  EncodeResult Encode(absl::string_view normalized) const {
    EXPECT_EQ(normalized, input_);
    return output_;
  }

  EncodeResult SampleEncode(absl::string_view normalized, float alpha) const {
    EXPECT_EQ(normalized, input_);
    return output_;
  }

  NBestEncodeResult NBestEncode(absl::string_view normalized,
                                int nbest_size) const {
    EXPECT_EQ(normalized, input_);
    return nbest_output_;
  }

  bool IsSampleEncodeAvailable() const override { return true; }

  bool IsNBestEncodeAvailable() const override { return true; }

  bool IsControl(int id) const { return id == 1 || id == 2; }

  bool IsUnknown(int id) const { return id == 0; }

  int GetPieceSize() const { return 10; }

  int PieceToId(absl::string_view piece) const { return 0; }

  const std::string &IdToPiece(int id) const { return kEmptyString; }

  float GetScore(int id) const { return 0.0; }

 private:
  absl::string_view input_;
  EncodeResult output_;
  NBestEncodeResult nbest_output_;
  const std::string kEmptyString;
};

class ByteFallbackMockModel : public MockModel {
 public:
  bool ByteFallbackEnabled() const override { return true; }
};

std::vector<std::string> GetSpVec(const EncodeResult &pieces) {
  std::vector<std::string> sps;
  for (const auto &p : pieces) {
    sps.emplace_back(std::string(p.first));
  }
  return sps;
}

std::vector<int> GetIdVec(const EncodeResult &pieces) {
  std::vector<int> ids;
  for (const auto &p : pieces) {
    ids.emplace_back(p.second);
  }
  return ids;
}

std::vector<std::string> GetSpVec(const SentencePieceText &spt) {
  std::vector<std::string> sps;
  for (auto &sp : spt.pieces()) {
    sps.emplace_back(sp.piece());
  }
  return sps;
}

NormalizerSpec MakeDefaultNormalizerSpec() {
  return SentencePieceTrainer::GetNormalizerSpec("nmt_nfkc");
}

TEST(SentencepieceProcessorTest, StatusTest) {
  SentencePieceProcessor sp;
  EXPECT_FALSE(sp.status().ok());
  auto mock = absl::make_unique<MockModel>();
  sp.SetModel(std::move(mock));
  EXPECT_FALSE(sp.status().ok());
}

TEST(SentencepieceProcessorTest, EncodeTest) {
  const absl::string_view kInput = WS "ABC" WS "DEF";
  SentencePieceProcessor sp;

  const auto normalization_spec = MakeDefaultNormalizerSpec();

  {
    auto mock = absl::make_unique<MockModel>();

    const EncodeResult result = {
        {WS "ABC", 3}, {WS "DE", 4}, {"F", 0}, {"</s>", 2}};
    mock->SetEncodeResult(kInput, result);

    sp.SetModel(std::move(mock));
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    std::vector<std::string> output;
    EXPECT_TRUE(sp.Encode("ABC DEF", &output).ok());
    EXPECT_EQ(GetSpVec(result), output);

    std::vector<int> ids;
    EXPECT_TRUE(sp.Encode("ABC DEF", &ids).ok());
    EXPECT_EQ(GetIdVec(result), ids);

    SentencePieceText spt;
    EXPECT_TRUE(sp.Encode("ABC DEF", &spt).ok());
    EXPECT_EQ(4, spt.pieces_size());
    for (int i = 0; i < 4; ++i) {
      EXPECT_EQ(result[i].first, spt.pieces(i).piece());
    }

    SentencePieceText spt2;
    EXPECT_TRUE(spt2.ParseFromString(sp.EncodeAsSerializedProto("ABC DEF")));
    EXPECT_EQ(spt.SerializeAsString(), spt2.SerializeAsString());

    EXPECT_EQ("ABC", spt.pieces(0).surface());
    EXPECT_EQ(" DE", spt.pieces(1).surface());
    EXPECT_EQ("F", spt.pieces(2).surface());
    EXPECT_EQ("", spt.pieces(3).surface());  // </s>

    EXPECT_EQ(3, spt.pieces(0).id());
    EXPECT_EQ(4, spt.pieces(1).id());
    EXPECT_EQ(0, spt.pieces(2).id());
    EXPECT_EQ(2, spt.pieces(3).id());

    EXPECT_EQ(0, spt.pieces(0).begin());
    EXPECT_EQ(3, spt.pieces(0).end());
    EXPECT_EQ(3, spt.pieces(1).begin());
    EXPECT_EQ(6, spt.pieces(1).end());
    EXPECT_EQ(6, spt.pieces(2).begin());
    EXPECT_EQ(7, spt.pieces(2).end());
    EXPECT_EQ(7, spt.pieces(3).begin());
    EXPECT_EQ(7, spt.pieces(3).end());
  }

  // Unknown sequences.
  {
    auto mock = absl::make_unique<MockModel>();

    const EncodeResult result = {
        {WS "ABC", 3}, {WS "D", 4}, {"E", 0}, {"F", 0}, {"</s>", 2}};
    const EncodeResult expected = {
        {WS "ABC", 3}, {WS "D", 4}, {"EF", 0}, {"</s>", 2}};

    mock->SetEncodeResult(kInput, result);
    sp.SetModel(std::move(mock));
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    std::vector<std::string> output;
    EXPECT_TRUE(sp.Encode("ABC DEF", &output).ok());
    EXPECT_EQ(GetSpVec(expected), output);

    std::vector<int> ids;
    EXPECT_TRUE(sp.Encode("ABC DEF", &ids).ok());
    EXPECT_EQ(GetIdVec(expected), ids);

    SentencePieceText spt;
    EXPECT_TRUE(sp.Encode("ABC DEF", &spt).ok());
    EXPECT_EQ(4, spt.pieces_size());
    for (int i = 0; i < 4; ++i) {
      EXPECT_EQ(expected[i].first, spt.pieces(i).piece());
    }

    EXPECT_EQ("ABC", spt.pieces(0).surface());
    EXPECT_EQ(" D", spt.pieces(1).surface());
    EXPECT_EQ("EF", spt.pieces(2).surface());
    EXPECT_EQ("", spt.pieces(3).surface());  // </s>

    EXPECT_EQ(3, spt.pieces(0).id());
    EXPECT_EQ(4, spt.pieces(1).id());
    EXPECT_EQ(0, spt.pieces(2).id());
    EXPECT_EQ(2, spt.pieces(3).id());

    EXPECT_EQ(0, spt.pieces(0).begin());
    EXPECT_EQ(3, spt.pieces(0).end());
    EXPECT_EQ(3, spt.pieces(1).begin());
    EXPECT_EQ(5, spt.pieces(1).end());
    EXPECT_EQ(5, spt.pieces(2).begin());
    EXPECT_EQ(7, spt.pieces(2).end());
    EXPECT_EQ(7, spt.pieces(3).begin());
    EXPECT_EQ(7, spt.pieces(3).end());
  }

  // Byte-fallback.
  {
    const absl::string_view kInput2 = WS "ABC" WS "DEFあ";
    auto mock = absl::make_unique<ByteFallbackMockModel>();

    const EncodeResult result = {{WS "ABC", 3}, {WS "D", 4}, {"E", 0},
                                 {"F", 0},      {"あ", 0},   {"</s>", 2}};
    // "E" -> 0x45
    // "F" -> 0x46
    // "あ" -> 0xe38182
    const EncodeResult expected = {{WS "ABC", 3}, {WS "D", 4},   {"<0x45>", 0},
                                   {"<0x46>", 0}, {"<0xE3>", 0}, {"<0x81>", 0},
                                   {"<0x82>", 0}, {"</s>", 2}};

    mock->SetEncodeResult(kInput2, result);
    sp.SetModel(std::move(mock));
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    std::vector<std::string> output;
    EXPECT_TRUE(sp.Encode("ABC DEFあ", &output).ok());
    EXPECT_EQ(GetSpVec(expected), output);

    std::vector<int> ids;
    EXPECT_TRUE(sp.Encode("ABC DEFあ", &ids).ok());
    EXPECT_EQ(GetIdVec(expected), ids);

    SentencePieceText spt;
    EXPECT_TRUE(sp.Encode("ABC DEFあ", &spt).ok());
    EXPECT_EQ(8, spt.pieces_size());
    for (int i = 0; i < 8; ++i) {
      EXPECT_EQ(expected[i].first, spt.pieces(i).piece());
    }

    EXPECT_EQ("ABC", spt.pieces(0).surface());
    EXPECT_EQ(" D", spt.pieces(1).surface());
    EXPECT_EQ("E", spt.pieces(2).surface());
    EXPECT_EQ("F", spt.pieces(3).surface());
    EXPECT_EQ("", spt.pieces(4).surface());    // あ
    EXPECT_EQ("", spt.pieces(5).surface());    // あ
    EXPECT_EQ("あ", spt.pieces(6).surface());  // あ
    EXPECT_EQ("", spt.pieces(7).surface());    // </s>

    EXPECT_EQ(3, spt.pieces(0).id());
    EXPECT_EQ(4, spt.pieces(1).id());
    EXPECT_EQ(0, spt.pieces(2).id());
    EXPECT_EQ(0, spt.pieces(3).id());
    EXPECT_EQ(0, spt.pieces(4).id());
    EXPECT_EQ(0, spt.pieces(5).id());
    EXPECT_EQ(0, spt.pieces(6).id());
    EXPECT_EQ(2, spt.pieces(7).id());

    EXPECT_EQ(0, spt.pieces(0).begin());
    EXPECT_EQ(3, spt.pieces(0).end());
    EXPECT_EQ(3, spt.pieces(1).begin());
    EXPECT_EQ(5, spt.pieces(1).end());
    EXPECT_EQ(5, spt.pieces(2).begin());
    EXPECT_EQ(6, spt.pieces(2).end());
    EXPECT_EQ(6, spt.pieces(3).begin());
    EXPECT_EQ(7, spt.pieces(3).end());
    EXPECT_EQ(7, spt.pieces(4).begin());  // あ
    EXPECT_EQ(7, spt.pieces(4).end());
    EXPECT_EQ(7, spt.pieces(5).begin());  // あ
    EXPECT_EQ(7, spt.pieces(5).end());
    EXPECT_EQ(7, spt.pieces(6).begin());  // あ
    EXPECT_EQ(10, spt.pieces(6).end());
    EXPECT_EQ(10, spt.pieces(7).begin());  // </s>
    EXPECT_EQ(10, spt.pieces(7).end());
  }

  // Crash if
  // ModelInterface::Encode() returns shorter results.
  {
    auto mock = absl::make_unique<MockModel>();
    const EncodeResult result = {{WS "ABC", 3}};
    mock->SetEncodeResult(kInput, result);
    sp.SetModel(std::move(mock));
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));
    SentencePieceText spt;
    // Expects crash.
    EXPECT_FALSE(sp.Encode("ABC DEF", &spt).ok());
  }

  // Crash if
  // ModelInterface::Encode() returns longer results.
  {
    auto mock = absl::make_unique<MockModel>();
    const EncodeResult result = {
        {WS "ABC", 3}, {WS "DE", 4}, {"F", 5}, {"G", 6}};
    mock->SetEncodeResult(kInput, result);
    sp.SetModel(std::move(mock));
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));
    SentencePieceText spt;
    // Expects crash.
    EXPECT_FALSE(sp.Encode("ABC DEF", &spt).ok());
  }

  // Crash if
  // ModelInterface::Encode() returns an empty piece.
  {
    auto mock = absl::make_unique<MockModel>();
    const EncodeResult result = {
        {WS "ABC", 3}, {WS "DE", 4}, {"", 5}, {"F", 6}};
    mock->SetEncodeResult(kInput, result);
    sp.SetModel(std::move(mock));
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));
    SentencePieceText spt;
    // Expects crash.
    EXPECT_FALSE(sp.Encode("ABC DEF", &spt).ok());
  }

  // Halfwidth to Fullwidith katakana normalization.
  {
    auto mock = absl::make_unique<MockModel>();
    const EncodeResult result = {{WS "グー", 3}, {"グル", 4}, {"</s>", 2}};
    const absl::string_view input = WS "グーグル";
    mock->SetEncodeResult(input, result);
    sp.SetModel(std::move(mock));
    std::vector<std::string> output;
    EXPECT_TRUE(sp.Encode("ｸﾞｰｸﾞﾙ", &output).ok());
    EXPECT_EQ(GetSpVec(result), output);

    SentencePieceText spt;
    EXPECT_TRUE(sp.Encode("ｸﾞｰｸﾞﾙ", &spt).ok());
    EXPECT_EQ(3, spt.pieces_size());
    for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(result[i].first, spt.pieces(i).piece());
    }

    EXPECT_EQ("ｸﾞｰ", spt.pieces(0).surface());
    EXPECT_EQ("ｸﾞﾙ", spt.pieces(1).surface());
    EXPECT_EQ("", spt.pieces(2).surface());

    EXPECT_EQ(3, spt.pieces(0).id());
    EXPECT_EQ(4, spt.pieces(1).id());
    EXPECT_EQ(2, spt.pieces(2).id());

    EXPECT_EQ(0, spt.pieces(0).begin());
    EXPECT_EQ(9, spt.pieces(0).end());
    EXPECT_EQ(9, spt.pieces(1).begin());
    EXPECT_EQ(18, spt.pieces(1).end());
    EXPECT_EQ(18, spt.pieces(2).begin());  // </s>
    EXPECT_EQ(18, spt.pieces(2).end());
  }

  // One to many normalization.
  {
    auto mock = absl::make_unique<MockModel>();
    const EncodeResult result = {{WS "株式", 3}, {"会社", 4}, {"</s>", 2}};
    const absl::string_view input = WS "株式会社";
    mock->SetEncodeResult(input, result);
    sp.SetModel(std::move(mock));
    std::vector<std::string> output;
    EXPECT_TRUE(sp.Encode("㍿", &output).ok());
    EXPECT_EQ(GetSpVec(result), output);

    SentencePieceText spt;
    EXPECT_TRUE(sp.Encode("㍿", &spt).ok());
    EXPECT_EQ(3, spt.pieces_size());
    for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(result[i].first, spt.pieces(i).piece());
    }

    EXPECT_EQ("", spt.pieces(0).surface());
    EXPECT_EQ("㍿", spt.pieces(1).surface());
    EXPECT_EQ("", spt.pieces(2).surface());

    EXPECT_EQ(3, spt.pieces(0).id());
    EXPECT_EQ(4, spt.pieces(1).id());
    EXPECT_EQ(2, spt.pieces(2).id());

    EXPECT_EQ(0, spt.pieces(0).begin());  // 株式
    EXPECT_EQ(0, spt.pieces(0).end());
    EXPECT_EQ(0, spt.pieces(1).begin());  // 会社
    EXPECT_EQ(3, spt.pieces(1).end());
    EXPECT_EQ(3, spt.pieces(2).begin());  // </s>
    EXPECT_EQ(3, spt.pieces(2).end());
  }
}

TEST(SentencepieceProcessorTest, NBestEncodeTest) {
  const std::string kInput = WS "ABC" WS "DEF";
  SentencePieceProcessor sp;

  const auto normalization_spec = MakeDefaultNormalizerSpec();

  auto mock = absl::make_unique<MockModel>();

  const NBestEncodeResult result = {
      {{{WS "ABC", 3}, {WS "DE", 4}, {"F", 0}, {"</s>", 2}},
       static_cast<float>(1.0)},
      {{{WS "AB", 5}, {WS "CD", 6}, {"EF", 7}, {"</s>", 2}},
       static_cast<float>(0.9)}};

  mock->SetNBestEncodeResult(kInput, result);
  sp.SetModel(std::move(mock));
  sp.SetNormalizer(
      absl::make_unique<normalizer::Normalizer>(normalization_spec));

  std::vector<std::vector<std::string>> output;
  EXPECT_TRUE(sp.NBestEncode("ABC DEF", 2, &output).ok());
  EXPECT_EQ(2, output.size());
  EXPECT_EQ(GetSpVec(result[0].first), output[0]);
  EXPECT_EQ(GetSpVec(result[1].first), output[1]);

  std::vector<std::vector<int>> ids;
  EXPECT_TRUE(sp.NBestEncode("ABC DEF", 2, &ids).ok());
  EXPECT_EQ(2, ids.size());
  EXPECT_EQ(GetIdVec(result[0].first), ids[0]);
  EXPECT_EQ(GetIdVec(result[1].first), ids[1]);

  NBestSentencePieceText spt;
  EXPECT_TRUE(sp.NBestEncode("ABC DEF", 2, &spt).ok());
  EXPECT_EQ(2, spt.nbests_size());
  EXPECT_EQ(4, spt.nbests(0).pieces_size());
  EXPECT_EQ(4, spt.nbests(1).pieces_size());
  EXPECT_NEAR(result[0].second, spt.nbests(0).score(), 0.001);
  EXPECT_NEAR(result[1].second, spt.nbests(1).score(), 0.001);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(result[0].first[i].first, spt.nbests(0).pieces(i).piece());
    EXPECT_EQ(result[1].first[i].first, spt.nbests(1).pieces(i).piece());
  }

  NBestSentencePieceText spt2;
  EXPECT_TRUE(
      spt2.ParseFromString(sp.NBestEncodeAsSerializedProto("ABC DEF", 2)));
  EXPECT_EQ(spt.SerializeAsString(), spt2.SerializeAsString());

  auto mock_empty = absl::make_unique<MockModel>();
  mock_empty->SetNBestEncodeResult(kInput, {});
  sp.SetModel(std::move(mock_empty));
  EXPECT_FALSE(sp.NBestEncode("ABC DEF", 2, &output).ok());
}

TEST(SentencepieceProcessorTest, SampleEncodeTest) {
  const std::string kInput = WS "ABC" WS "DEF";
  SentencePieceProcessor sp;

  const auto normalization_spec = MakeDefaultNormalizerSpec();

  auto mock = absl::make_unique<MockModel>();

  const EncodeResult result = {
      {WS "ABC", 3}, {WS "DE", 4}, {"F", 0}, {"</s>", 2}};
  const NBestEncodeResult nbest_result = {
      {{{WS "ABC", 3}, {WS "DE", 4}, {"F", 0}, {"</s>", 2}},
       static_cast<float>(1.0)},
      {{{WS "AB", 5}, {WS "CD", 6}, {"EF", 7}, {"</s>", 2}},
       static_cast<float>(0.1)}};

  mock->SetNBestEncodeResult(kInput, nbest_result);
  mock->SetEncodeResult(kInput, result);
  sp.SetModel(std::move(mock));
  sp.SetNormalizer(
      absl::make_unique<normalizer::Normalizer>(normalization_spec));

  std::vector<std::string> output;
  EXPECT_TRUE(sp.SampleEncode("ABC DEF", -1, 0.5, &output).ok());
  EXPECT_EQ(4, output.size());
  EXPECT_EQ(GetSpVec(result), output);

  std::vector<int> ids;
  EXPECT_TRUE(sp.SampleEncode("ABC DEF", -1, 0.5, &ids).ok());
  EXPECT_EQ(4, ids.size());
  EXPECT_EQ(GetIdVec(result), ids);

  SentencePieceText spt;
  EXPECT_TRUE(sp.SampleEncode("ABC DEF", -1, 0.5, &spt).ok());
  EXPECT_EQ(4, spt.pieces_size());
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(result[i].first, spt.pieces(i).piece());
    EXPECT_EQ(result[i].second, spt.pieces(i).id());
  }

  SentencePieceText spt2;
  EXPECT_TRUE(spt2.ParseFromString(
      sp.SampleEncodeAsSerializedProto("ABC DEF", -1, 0.5)));
  EXPECT_EQ(spt.SerializeAsString(), spt2.SerializeAsString());

  EXPECT_FALSE(sp.SampleEncode("ABC DEF", 1024, 0.5, &output).ok());
  EXPECT_TRUE(sp.SampleEncode("ABC DEF", 0, 0.5, &output).ok());
  EXPECT_TRUE(sp.SampleEncode("ABC DEF", 1, 0.5, &output).ok());

  std::vector<int> freq(2, 0);
  for (int i = 0; i < 5000; ++i) {
    EXPECT_TRUE(sp.SampleEncode("ABC DEF", 20, 0.5, &output).ok());
    EXPECT_EQ(4, output.size());
    if (GetSpVec(nbest_result[0].first) == output)
      freq[0]++;
    else if (GetSpVec(nbest_result[1].first) == output)
      freq[1]++;
    else
      LOG(FATAL) << "Invalid result.";
  }

  const float expected_prob =
      std::exp(0.5 * 1.0) / (std::exp(0.5 * 1.0) + std::exp(0.5 * 0.1));
  const float prob = 1.0 * freq[0] / (freq[0] + freq[1]);
  EXPECT_NEAR(prob, expected_prob, 0.05);

  auto mock_empty = absl::make_unique<MockModel>();
  mock_empty->SetNBestEncodeResult(kInput, {});
  sp.SetModel(std::move(mock_empty));
  EXPECT_FALSE(sp.SampleEncode("ABC DEF", 10, 0.5, &output).ok());
}

TEST(SentencepieceProcessorTest, DecodeTest) {
  class DecodeMockModel : public ModelInterface {
   public:
    EncodeResult Encode(absl::string_view normalized) const override {
      return {};
    }

    int GetPieceSize() const override { return 7; }

    int PieceToId(absl::string_view piece) const override {
      static absl::flat_hash_map<absl::string_view, int> kMap = {
          {"<unk>", 0}, {"<s>", 1}, {"</s>", 2},    {WS "ABC", 3},
          {WS "DE", 4}, {"F", 5},   {"G" WS "H", 6}};
      return port::FindWithDefault(kMap, piece, 0);
    }

    const std::string &IdToPiece(int id) const override {
      static std::vector<std::string> kMap = {
          "<unk>", "<s>", "</s>", WS "ABC", WS "DE", "F", "G" WS "H"};
      return kMap[id];
    }

    bool IsUnknown(int id) const override { return (id == 0); }

    bool IsControl(int id) const override { return (id == 1 || id == 2); }

    bool IsByte(int id) const override { return false; }

    float GetScore(int id) const override { return 0.0; }
  };

  const std::vector<std::string> input = {"<s>", WS "ABC",   "<unk>", WS "DE",
                                          "F",   "G" WS "H", "I",     "</s>"};

  {
    SentencePieceProcessor sp;
    auto mock = absl::make_unique<DecodeMockModel>();
    sp.SetModel(std::move(mock));

    const auto normalization_spec = MakeDefaultNormalizerSpec();
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    SentencePieceText spt;

    EXPECT_TRUE(sp.Decode(input, &spt).ok());
    EXPECT_EQ("ABC \xE2\x81\x87  DEFG HI", spt.text());
    EXPECT_EQ(8, spt.pieces_size());

    for (int i = 0; i < 6; ++i) {
      EXPECT_EQ(input[i], spt.pieces(i).piece());
    }

    EXPECT_EQ("", spt.pieces(0).surface());
    EXPECT_EQ("ABC", spt.pieces(1).surface());
    EXPECT_EQ(" \xE2\x81\x87 ", spt.pieces(2).surface());
    EXPECT_EQ(" DE", spt.pieces(3).surface());
    EXPECT_EQ("F", spt.pieces(4).surface());
    EXPECT_EQ("G H", spt.pieces(5).surface());
    EXPECT_EQ("I", spt.pieces(6).surface());
    EXPECT_EQ("", spt.pieces(7).surface());

    EXPECT_EQ(0, spt.pieces(0).begin());
    EXPECT_EQ(0, spt.pieces(0).end());
    EXPECT_EQ(0, spt.pieces(1).begin());
    EXPECT_EQ(3, spt.pieces(1).end());
    EXPECT_EQ(3, spt.pieces(2).begin());
    EXPECT_EQ(8, spt.pieces(2).end());
    EXPECT_EQ(8, spt.pieces(3).begin());
    EXPECT_EQ(11, spt.pieces(3).end());
    EXPECT_EQ(11, spt.pieces(4).begin());
    EXPECT_EQ(12, spt.pieces(4).end());
    EXPECT_EQ(12, spt.pieces(5).begin());
    EXPECT_EQ(15, spt.pieces(5).end());
    EXPECT_EQ(15, spt.pieces(6).begin());
    EXPECT_EQ(16, spt.pieces(6).end());
    EXPECT_EQ(16, spt.pieces(7).begin());
    EXPECT_EQ(16, spt.pieces(7).end());

    SentencePieceText spt2;
    EXPECT_TRUE(spt2.ParseFromString(sp.DecodePiecesAsSerializedProto(input)));
    EXPECT_EQ(spt.SerializeAsString(), spt2.SerializeAsString());
  }

  // unk_surface is not defined.
  {
    SentencePieceProcessor sp;
    auto proto = absl::make_unique<ModelProto>();
    sp.Load(std::move(proto)).IgnoreError();

    auto mock = absl::make_unique<DecodeMockModel>();
    sp.SetModel(std::move(mock));

    const auto normalization_spec = MakeDefaultNormalizerSpec();
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    SentencePieceText spt;

    EXPECT_TRUE(sp.Decode(input, &spt).ok());
    EXPECT_EQ("ABC \xE2\x81\x87  DEFG HI", spt.text());
    EXPECT_EQ(8, spt.pieces_size());
  }

  {
    SentencePieceProcessor sp;
    auto proto = absl::make_unique<ModelProto>();
    proto->mutable_trainer_spec()->set_unk_surface("");
    sp.Load(std::move(proto)).IgnoreError();

    auto mock = absl::make_unique<DecodeMockModel>();
    sp.SetModel(std::move(mock));

    const auto normalization_spec = MakeDefaultNormalizerSpec();
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    SentencePieceText spt;

    EXPECT_TRUE(sp.Decode(input, &spt).ok());
    EXPECT_EQ("ABC DEFG HI", spt.text());
    EXPECT_EQ(8, spt.pieces_size());
  }

  {
    SentencePieceProcessor sp;
    auto proto = absl::make_unique<ModelProto>();
    proto->mutable_trainer_spec()->set_unk_surface("<UNK>");
    sp.Load(std::move(proto)).IgnoreError();

    auto mock = absl::make_unique<DecodeMockModel>();
    sp.SetModel(std::move(mock));

    const auto normalization_spec = MakeDefaultNormalizerSpec();
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    SentencePieceText spt;

    EXPECT_TRUE(sp.Decode(input, &spt).ok());
    EXPECT_EQ("ABC<UNK> DEFG HI", spt.text());
    EXPECT_EQ(8, spt.pieces_size());
  }

  {
    SentencePieceProcessor sp;
    auto proto = absl::make_unique<ModelProto>();
    proto->mutable_trainer_spec()->set_unk_surface("");
    proto->mutable_normalizer_spec()->set_add_dummy_prefix(false);
    proto->mutable_normalizer_spec()->set_remove_extra_whitespaces(false);
    sp.Load(std::move(proto)).IgnoreError();

    auto mock = absl::make_unique<DecodeMockModel>();
    sp.SetModel(std::move(mock));

    const auto normalization_spec = MakeDefaultNormalizerSpec();
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    SentencePieceText spt;

    EXPECT_TRUE(sp.Decode(input, &spt).ok());
    EXPECT_EQ(" ABC DEFG HI", spt.text());
    EXPECT_EQ(8, spt.pieces_size());
  }
}

TEST(SentencepieceProcessorTest, DummyPrefixDecodeTest) {
  class DecodeMockModel : public ModelInterface {
   public:
    EncodeResult Encode(absl::string_view normalized) const override {
      return {};
    }

    int GetPieceSize() const override { return 7; }

    int PieceToId(absl::string_view piece) const override {
      static absl::flat_hash_map<absl::string_view, int> kMap = {
          {"<unk>", 0}, {"<s>", 1}, {"</s>", 2},     {WS "ABC", 3},
          {WS "DE", 4}, {"F", 5},   {"G" WS "H", 6}, {WS, 7}};
      return port::FindWithDefault(kMap, piece, 0);
    }

    const std::string& IdToPiece(int id) const override {
      static std::vector<std::string> kMap = {
          "<unk>", "<s>", "</s>", WS "ABC", WS "DE", "F", "G" WS "H", WS};
      return kMap[id];
    }

    bool IsUnknown(int id) const override { return (id == 0); }

    bool IsControl(int id) const override { return (id == 1 || id == 2); }

    bool IsByte(int id) const override { return false; }

    float GetScore(int id) const override { return 0.0; }
  };

  // start the sequence with a whitespace token
  const std::vector<std::string> input = {
      "<s>", WS, WS "ABC", "<unk>", WS "DE", "F", "G" WS "H", "I", "</s>"};

  {
    SentencePieceProcessor sp;
    auto proto = absl::make_unique<ModelProto>();
    proto->mutable_trainer_spec()->set_unk_surface("");
    proto->mutable_normalizer_spec()->set_add_dummy_prefix(true);
    proto->mutable_normalizer_spec()->set_remove_extra_whitespaces(false);
    sp.Load(std::move(proto)).IgnoreError();

    auto mock = absl::make_unique<DecodeMockModel>();
    sp.SetModel(std::move(mock));

    const auto normalization_spec = MakeDefaultNormalizerSpec();
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    SentencePieceText spt;

    EXPECT_TRUE(sp.Decode(input, &spt).ok());
    EXPECT_EQ(" ABC DEFG HI", spt.text());
    EXPECT_EQ(9, spt.pieces_size());
  }

  {
    SentencePieceProcessor sp;
    auto proto = absl::make_unique<ModelProto>();
    proto->mutable_trainer_spec()->set_unk_surface("");
    proto->mutable_normalizer_spec()->set_add_dummy_prefix(true);
    proto->mutable_normalizer_spec()->set_remove_extra_whitespaces(true);
    sp.Load(std::move(proto)).IgnoreError();

    auto mock = absl::make_unique<DecodeMockModel>();
    sp.SetModel(std::move(mock));

    const auto normalization_spec = MakeDefaultNormalizerSpec();
    sp.SetNormalizer(
        absl::make_unique<normalizer::Normalizer>(normalization_spec));

    SentencePieceText spt;

    EXPECT_TRUE(sp.Decode(input, &spt).ok());
    EXPECT_EQ("ABC DEFG HI", spt.text());
    EXPECT_EQ(9, spt.pieces_size());
  }
}

TEST(SentencepieceProcessorTest, ByteFallbackDecodeTest) {
  class ByteFallbackDecodeMockModel : public ModelInterface {
   public:
    EncodeResult Encode(absl::string_view normalized) const override {
      return {};
    }

    int PieceToId(absl::string_view piece) const override {
      using Map = absl::flat_hash_map<std::string, int>;
      static const Map kMap = []() -> Map {
        Map m = {
            {"<unk>", 0}, {"<s>", 1}, {"</s>", 2}, {"A", 3}, {"B", 4}, {"C", 5},
        };
        for (int i = 0; i < 256; ++i) {
          m[ByteToPiece(i)] = 6 + i;
        }
        return m;
      }();
      return port::FindWithDefault(kMap, std::string(piece), 0);
    }

    const std::string& IdToPiece(int id) const override {
      static std::vector<std::string> kMap = []() -> std::vector<std::string> {
        std::vector<std::string> m = {"<unk>", "<s>", "</s>", "A", "B", "C"};
        for (int i = 0; i < 256; ++i) {
          m.push_back(ByteToPiece(i));
        }
        return m;
      }();
      return kMap[id];
    }

    int GetPieceSize() const override { return 256; }

    bool IsUnknown(int id) const override { return (id == 0); }

    bool IsControl(int id) const override { return (id == 1 || id == 2); }

    bool IsByte(int id) const override { return id >= 6; }

    bool ByteFallbackEnabled() const override { return true; }
  };

  SentencePieceProcessor sp;
  auto mock = absl::make_unique<ByteFallbackDecodeMockModel>();
  sp.SetModel(std::move(mock));

  const auto normalization_spec = MakeDefaultNormalizerSpec();
  sp.SetNormalizer(
      absl::make_unique<normalizer::Normalizer>(normalization_spec));

  {
    const std::vector<std::string> input = {
        "<s>",
        "A",
        "B",
        // "あ" -> 0xE3 0x81 0x82
        "<0xE3>",
        "<0x81>",
        "<0x82>",
        // "Z" -> 0x5A
        "<0x5A>",
        // "Ω" -> 0xCE 0xA9
        "<0xCE>",
        "<0xA9>",
        "C",
        // Invalid UTF-8 bytes.
        "<0xE0>",
        "<0x80>",
        // "い" -> 0xE3 0x81 0x84
        "<0xE3>",
        "<0x81>",
        "<0x84>",
        // REPLACEMENT CHARACTER as byte pieces.
        "<0xEF>",
        "<0xBF>",
        "<0xBD>",
    };

    SentencePieceText spt;
    EXPECT_TRUE(sp.Decode(input, &spt).ok());
    EXPECT_EQ("ABあZΩC\xEF\xBF\xBD\xEF\xBF\xBDい\xEF\xBF\xBD", spt.text());
    EXPECT_EQ(18, spt.pieces_size());

    for (int i = 0; i < 18; ++i) {
      EXPECT_EQ(input[i], spt.pieces(i).piece());
    }

    EXPECT_EQ("", spt.pieces(0).surface());
    EXPECT_EQ(0, spt.pieces(0).begin());
    EXPECT_EQ(0, spt.pieces(0).end());

    EXPECT_EQ("A", spt.pieces(1).surface());
    EXPECT_EQ(0, spt.pieces(1).begin());
    EXPECT_EQ(1, spt.pieces(1).end());

    EXPECT_EQ("B", spt.pieces(2).surface());
    EXPECT_EQ(1, spt.pieces(2).begin());
    EXPECT_EQ(2, spt.pieces(2).end());

    EXPECT_EQ("", spt.pieces(3).surface());
    EXPECT_EQ("", spt.pieces(4).surface());
    EXPECT_EQ("あ", spt.pieces(5).surface());
    EXPECT_EQ(2, spt.pieces(3).begin());
    EXPECT_EQ(2, spt.pieces(3).end());
    EXPECT_EQ(2, spt.pieces(4).begin());
    EXPECT_EQ(2, spt.pieces(4).end());
    EXPECT_EQ(2, spt.pieces(5).begin());
    EXPECT_EQ(5, spt.pieces(5).end());

    EXPECT_EQ("Z", spt.pieces(6).surface());
    EXPECT_EQ(5, spt.pieces(6).begin());
    EXPECT_EQ(6, spt.pieces(6).end());

    EXPECT_EQ("", spt.pieces(7).surface());
    EXPECT_EQ("Ω", spt.pieces(8).surface());
    EXPECT_EQ(6, spt.pieces(7).begin());
    EXPECT_EQ(6, spt.pieces(7).end());
    EXPECT_EQ(6, spt.pieces(8).begin());
    EXPECT_EQ(8, spt.pieces(8).end());

    EXPECT_EQ("C", spt.pieces(9).surface());
    EXPECT_EQ(8, spt.pieces(9).begin());
    EXPECT_EQ(9, spt.pieces(9).end());

    EXPECT_EQ("\xEF\xBF\xBD", spt.pieces(10).surface());
    EXPECT_EQ(9, spt.pieces(10).begin());
    EXPECT_EQ(12, spt.pieces(10).end());

    EXPECT_EQ("\xEF\xBF\xBD", spt.pieces(11).surface());
    EXPECT_EQ(12, spt.pieces(11).begin());
    EXPECT_EQ(15, spt.pieces(11).end());

    EXPECT_EQ("", spt.pieces(12).surface());
    EXPECT_EQ("", spt.pieces(13).surface());
    EXPECT_EQ("い", spt.pieces(14).surface());
    EXPECT_EQ(15, spt.pieces(12).begin());
    EXPECT_EQ(15, spt.pieces(12).end());
    EXPECT_EQ(15, spt.pieces(13).begin());
    EXPECT_EQ(15, spt.pieces(13).end());
    EXPECT_EQ(15, spt.pieces(14).begin());
    EXPECT_EQ(18, spt.pieces(14).end());

    EXPECT_EQ("", spt.pieces(15).surface());
    EXPECT_EQ("", spt.pieces(16).surface());
    EXPECT_EQ("\xEF\xBF\xBD", spt.pieces(17).surface());
    EXPECT_EQ(18, spt.pieces(15).begin());
    EXPECT_EQ(18, spt.pieces(15).end());
    EXPECT_EQ(18, spt.pieces(16).begin());
    EXPECT_EQ(18, spt.pieces(16).end());
    EXPECT_EQ(18, spt.pieces(17).begin());
    EXPECT_EQ(21, spt.pieces(17).end());
  }
}

void AddPiece(ModelProto *model_proto, absl::string_view piece,
              float score = 0.0) {
  auto *sp = model_proto->add_pieces();
  sp->set_piece(std::string(piece));
  sp->set_score(score);
}

TEST(SentencePieceProcessorTest, LoadInvalidModelTest) {
  SentencePieceProcessor sp;
  EXPECT_FALSE(sp.Load("").ok());
  EXPECT_FALSE(sp.Load("__UNKNOWN_FILE__").ok());
}

TEST(SentencePieceProcessorTest, LoadSerializedProtoTest) {
  ModelProto model_proto;
  auto *sp1 = model_proto.add_pieces();
  sp1->set_type(ModelProto::SentencePiece::UNKNOWN);
  sp1->set_piece("<unk>");
  AddPiece(&model_proto, WS, 0.0);
  *(model_proto.mutable_normalizer_spec()) = MakeDefaultNormalizerSpec();

  SentencePieceProcessor sp;
  EXPECT_FALSE(sp.LoadFromSerializedProto("__NOT_A_PROTO__").ok());
  EXPECT_TRUE(sp.LoadFromSerializedProto(model_proto.SerializeAsString()).ok());
  EXPECT_EQ(model_proto.SerializeAsString(),
            sp.model_proto().SerializeAsString());
}

TEST(SentencePieceProcessorTest, EndToEndTest) {
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

  AddPiece(&model_proto, "a", 0.0);
  AddPiece(&model_proto, "b", 0.3);
  AddPiece(&model_proto, "c", 0.2);
  AddPiece(&model_proto, "ab", 1.0);
  AddPiece(&model_proto, "\xE2\x96\x81", 3.0);  // kSpaceSymbol

  *(model_proto.mutable_normalizer_spec()) = MakeDefaultNormalizerSpec();

  {
    auto output = filesystem::NewWritableFile(
        util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "model"), true);
    output->Write(model_proto.SerializeAsString());
  }

  SentencePieceProcessor sp;
  EXPECT_TRUE(
      sp.Load(util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "model")).ok());

  EXPECT_EQ(model_proto.SerializeAsString(),
            sp.model_proto().SerializeAsString());

  EXPECT_EQ(8, sp.GetPieceSize());
  EXPECT_EQ(0, sp.PieceToId("<unk>"));
  EXPECT_EQ(1, sp.PieceToId("<s>"));
  EXPECT_EQ(2, sp.PieceToId("</s>"));
  EXPECT_EQ(3, sp.PieceToId("a"));
  EXPECT_EQ(4, sp.PieceToId("b"));
  EXPECT_EQ(5, sp.PieceToId("c"));
  EXPECT_EQ(6, sp.PieceToId("ab"));
  EXPECT_EQ(7, sp.PieceToId("\xE2\x96\x81"));

  EXPECT_EQ("<unk>", sp.IdToPiece(0));
  EXPECT_EQ("<s>", sp.IdToPiece(1));
  EXPECT_EQ("</s>", sp.IdToPiece(2));
  EXPECT_EQ("a", sp.IdToPiece(3));
  EXPECT_EQ("b", sp.IdToPiece(4));
  EXPECT_EQ("c", sp.IdToPiece(5));
  EXPECT_EQ("ab", sp.IdToPiece(6));
  EXPECT_EQ("\xE2\x96\x81", sp.IdToPiece(7));

  EXPECT_NEAR(0.0, sp.GetScore(0), 0.001);
  EXPECT_NEAR(0.0, sp.GetScore(1), 0.001);
  EXPECT_NEAR(0.0, sp.GetScore(2), 0.001);
  EXPECT_NEAR(0.0, sp.GetScore(3), 0.001);
  EXPECT_NEAR(0.3, sp.GetScore(4), 0.001);
  EXPECT_NEAR(0.2, sp.GetScore(5), 0.001);
  EXPECT_NEAR(1.0, sp.GetScore(6), 0.001);
  EXPECT_NEAR(3.0, sp.GetScore(7), 0.001);

  EXPECT_TRUE(sp.IsUnknown(0));
  EXPECT_FALSE(sp.IsUnknown(1));
  EXPECT_FALSE(sp.IsUnknown(2));
  EXPECT_FALSE(sp.IsUnknown(3));
  EXPECT_FALSE(sp.IsUnknown(4));
  EXPECT_FALSE(sp.IsUnknown(5));
  EXPECT_FALSE(sp.IsUnknown(6));
  EXPECT_FALSE(sp.IsUnknown(7));

  EXPECT_FALSE(sp.IsControl(0));
  EXPECT_TRUE(sp.IsControl(1));
  EXPECT_TRUE(sp.IsControl(2));
  EXPECT_FALSE(sp.IsControl(3));
  EXPECT_FALSE(sp.IsControl(4));
  EXPECT_FALSE(sp.IsControl(5));
  EXPECT_FALSE(sp.IsControl(6));
  EXPECT_FALSE(sp.IsControl(7));

  EXPECT_EQ(0, sp.unk_id());
  EXPECT_EQ(1, sp.bos_id());
  EXPECT_EQ(2, sp.eos_id());
  EXPECT_EQ(-1, sp.pad_id());

  {
    std::vector<std::string> sps;
    const std::vector<std::string> expected_str = {WS, "ab", "c"};
    EXPECT_TRUE(sp.Encode("abc", &sps).ok());
    EXPECT_EQ(expected_str, sps);

    std::vector<int> ids;
    const std::vector<int> expected_id = {7, 6, 5};
    EXPECT_TRUE(sp.Encode("abc", &ids).ok());
    EXPECT_EQ(expected_id, ids);
  }

  {
    EXPECT_TRUE(sp.SetEncodeExtraOptions("bos").ok());

    std::vector<std::string> sps;
    const std::vector<std::string> expected_str = {"<s>", WS, "ab", "c"};
    EXPECT_TRUE(sp.Encode("abc", &sps).ok());
    EXPECT_EQ(expected_str, sps);

    std::vector<int> ids;
    const std::vector<int> expected_id = {1, 7, 6, 5};
    EXPECT_TRUE(sp.Encode("abc", &ids).ok());
    EXPECT_EQ(expected_id, ids);
  }

  {
    EXPECT_TRUE(sp.SetEncodeExtraOptions("eos").ok());

    std::vector<std::string> sps;
    const std::vector<std::string> expected_str = {WS, "ab", "c", "</s>"};
    EXPECT_TRUE(sp.Encode("abc", &sps).ok());
    EXPECT_EQ(expected_str, sps);

    std::vector<int> ids;
    const std::vector<int> expected_id = {7, 6, 5, 2};
    EXPECT_TRUE(sp.Encode("abc", &ids).ok());
    EXPECT_EQ(expected_id, ids);
  }

  {
    EXPECT_TRUE(sp.SetEncodeExtraOptions("reverse").ok());

    std::vector<std::string> sps;
    const std::vector<std::string> expected_str = {"c", "ab", WS};
    EXPECT_TRUE(sp.Encode("abc", &sps).ok());
    EXPECT_EQ(expected_str, sps);

    std::vector<int> ids;
    const std::vector<int> expected_id = {5, 6, 7};
    EXPECT_TRUE(sp.Encode("abc", &ids).ok());
    EXPECT_EQ(expected_id, ids);
  }

  {
    EXPECT_TRUE(sp.SetEncodeExtraOptions("bos:eos").ok());

    std::vector<std::string> sps;
    const std::vector<std::string> expected_str = {"<s>", WS, "ab", "c",
                                                   "</s>"};
    EXPECT_TRUE(sp.Encode("abc", &sps).ok());
    EXPECT_EQ(expected_str, sps);

    std::vector<int> ids;
    const std::vector<int> expected_id = {1, 7, 6, 5, 2};
    EXPECT_TRUE(sp.Encode("abc", &ids).ok());
    EXPECT_EQ(expected_id, ids);
  }

  {
    EXPECT_TRUE(sp.SetEncodeExtraOptions("reverse:bos:eos").ok());

    std::vector<std::string> sps;
    const std::vector<std::string> expected_str = {"<s>", "c", "ab", WS,
                                                   "</s>"};
    EXPECT_TRUE(sp.Encode("abc", &sps).ok());
    EXPECT_EQ(expected_str, sps);

    std::vector<int> ids;
    const std::vector<int> expected_id = {1, 5, 6, 7, 2};
    EXPECT_TRUE(sp.Encode("abc", &ids).ok());
    EXPECT_EQ(expected_id, ids);
  }

  {
    EXPECT_TRUE(sp.SetEncodeExtraOptions("bos:eos:reverse").ok());

    std::vector<std::string> sps;
    const std::vector<std::string> expected_str = {"</s>", "c", "ab", WS,
                                                   "<s>"};
    EXPECT_TRUE(sp.Encode("abc", &sps).ok());
    EXPECT_EQ(expected_str, sps);

    std::vector<int> ids;
    const std::vector<int> expected_id = {2, 5, 6, 7, 1};
    EXPECT_TRUE(sp.Encode("abc", &ids).ok());
    EXPECT_EQ(expected_id, ids);
  }

  {
    std::string output;
    const std::vector<std::string> sps = {"ab", "c"};
    EXPECT_TRUE(sp.Decode(sps, &output).ok());
    EXPECT_EQ("abc", output);

    const std::vector<int> ids = {3, 4, 5};
    EXPECT_TRUE(sp.Decode(ids, &output).ok());
    EXPECT_EQ("abc", output);
  }

  {
    EXPECT_TRUE(sp.SetDecodeExtraOptions("bos").ok());

    std::string output;
    const std::vector<std::string> sps = {"ab", "c"};
    EXPECT_TRUE(sp.Decode(sps, &output).ok());
    EXPECT_EQ("abc", output);

    const std::vector<int> ids = {3, 4, 5};
    EXPECT_TRUE(sp.Decode(ids, &output).ok());
    EXPECT_EQ("abc", output);
  }

  {
    EXPECT_TRUE(sp.SetDecodeExtraOptions("eos").ok());

    std::string output;
    const std::vector<std::string> sps = {"ab", "c"};
    EXPECT_TRUE(sp.Decode(sps, &output).ok());
    EXPECT_EQ("abc", output);

    const std::vector<int> ids = {3, 4, 5};
    EXPECT_TRUE(sp.Decode(ids, &output).ok());
    EXPECT_EQ("abc", output);
  }

  {
    EXPECT_TRUE(sp.SetDecodeExtraOptions("reverse").ok());

    std::string output;
    const std::vector<std::string> sps = {"ab", "c"};
    EXPECT_TRUE(sp.Decode(sps, &output).ok());
    EXPECT_EQ("cab", output);

    const std::vector<int> ids = {3, 4, 5};
    EXPECT_TRUE(sp.Decode(ids, &output).ok());
    EXPECT_EQ("cba", output);
  }

  {
    EXPECT_TRUE(sp.SetDecodeExtraOptions("bos:eos").ok());

    std::string output;
    const std::vector<std::string> sps = {"ab", "c"};
    EXPECT_TRUE(sp.Decode(sps, &output).ok());
    EXPECT_EQ("abc", output);

    const std::vector<int> ids = {3, 4, 5};
    EXPECT_TRUE(sp.Decode(ids, &output).ok());
    EXPECT_EQ("abc", output);
  }

  {
    EXPECT_TRUE(sp.SetDecodeExtraOptions("reverse:bos:eos").ok());

    std::string output;
    const std::vector<std::string> sps = {"ab", "c"};
    EXPECT_TRUE(sp.Decode(sps, &output).ok());
    EXPECT_EQ("cab", output);

    const std::vector<int> ids = {3, 4, 5};
    EXPECT_TRUE(sp.Decode(ids, &output).ok());
    EXPECT_EQ("cba", output);
  }

  // Out of range
  {
    std::string output;
    const std::vector<int> ids = {3, 4, 127};
    EXPECT_FALSE(sp.Decode(ids, &output).ok());
  }

  {
    EXPECT_TRUE(sp.SetDecodeExtraOptions("bos:eos:reverse").ok());

    std::string output;
    const std::vector<std::string> sps = {"ab", "c"};
    EXPECT_TRUE(sp.Decode(sps, &output).ok());
    EXPECT_EQ("cab", output);

    const std::vector<int> ids = {3, 4, 5};
    EXPECT_TRUE(sp.Decode(ids, &output).ok());
    EXPECT_EQ("cba", output);
  }

  {
    EXPECT_TRUE(sp.SetDecodeExtraOptions("reverse:reverse").ok());

    std::string output;
    const std::vector<std::string> sps = {"ab", "c"};
    EXPECT_TRUE(sp.Decode(sps, &output).ok());
    EXPECT_EQ("abc", output);

    const std::vector<int> ids = {3, 4, 5};
    EXPECT_TRUE(sp.Decode(ids, &output).ok());
    EXPECT_EQ("abc", output);
  }

  EXPECT_TRUE(sp.SetEncodeExtraOptions("").ok());
  EXPECT_TRUE(sp.SetDecodeExtraOptions("").ok());

  EXPECT_FALSE(sp.SetEncodeExtraOptions("foo").ok());
  EXPECT_FALSE(sp.SetDecodeExtraOptions("foo").ok());

  auto RunTest = [&model_proto](const SentencePieceProcessor &sp) {
    EXPECT_EQ(model_proto.SerializeAsString(),
              sp.model_proto().SerializeAsString());

    EXPECT_EQ(8, sp.GetPieceSize());
    EXPECT_EQ(0, sp.PieceToId("<unk>"));
    EXPECT_EQ(1, sp.PieceToId("<s>"));
    EXPECT_EQ(2, sp.PieceToId("</s>"));
    EXPECT_EQ(3, sp.PieceToId("a"));
    EXPECT_EQ(4, sp.PieceToId("b"));
    EXPECT_EQ(5, sp.PieceToId("c"));
    EXPECT_EQ(6, sp.PieceToId("ab"));
    EXPECT_EQ(7, sp.PieceToId("\xE2\x96\x81"));

    EXPECT_EQ("<unk>", sp.IdToPiece(0));
    EXPECT_EQ("<s>", sp.IdToPiece(1));
    EXPECT_EQ("</s>", sp.IdToPiece(2));
    EXPECT_EQ("a", sp.IdToPiece(3));
    EXPECT_EQ("b", sp.IdToPiece(4));
    EXPECT_EQ("c", sp.IdToPiece(5));
    EXPECT_EQ("ab", sp.IdToPiece(6));
    EXPECT_EQ("\xE2\x96\x81", sp.IdToPiece(7));

    EXPECT_TRUE(sp.IsUnknown(0));
    EXPECT_FALSE(sp.IsUnknown(1));
    EXPECT_FALSE(sp.IsUnknown(2));
    EXPECT_FALSE(sp.IsUnknown(3));
    EXPECT_FALSE(sp.IsUnknown(4));
    EXPECT_FALSE(sp.IsUnknown(5));
    EXPECT_FALSE(sp.IsUnknown(6));
    EXPECT_FALSE(sp.IsUnknown(7));

    EXPECT_FALSE(sp.IsControl(0));
    EXPECT_TRUE(sp.IsControl(1));
    EXPECT_TRUE(sp.IsControl(2));
    EXPECT_FALSE(sp.IsControl(3));
    EXPECT_FALSE(sp.IsControl(4));
    EXPECT_FALSE(sp.IsControl(5));
    EXPECT_FALSE(sp.IsControl(6));
    EXPECT_FALSE(sp.IsControl(7));

    {
      std::vector<std::string> sps;
      const std::vector<std::string> expected_str = {WS, "ab", "c"};
      EXPECT_TRUE(sp.Encode("abc", &sps).ok());
      EXPECT_EQ(expected_str, sps);

      std::vector<int> ids;
      const std::vector<int> expected_id = {7, 6, 5};
      EXPECT_TRUE(sp.Encode("abc", &ids).ok());
      EXPECT_EQ(expected_id, ids);
    }

    {
      std::string output;
      const std::vector<std::string> sps = {"ab", "c"};
      EXPECT_TRUE(sp.Decode(sps, &output).ok());
      EXPECT_EQ("abc", output);

      const std::vector<int> ids = {3, 4, 5};
      EXPECT_TRUE(sp.Decode(ids, &output).ok());
      EXPECT_EQ("abc", output);
    }
  };

  // Copies ModelProto.
  {
    SentencePieceProcessor sp;
    const ModelProto copied = model_proto;
    EXPECT_TRUE(sp.Load(copied).ok());
    RunTest(sp);
  }

  // Moves ModelProto.
  {
    SentencePieceProcessor sp;
    auto moved = absl::make_unique<ModelProto>();
    const ModelProto *moved_ptr = moved.get();
    *moved = model_proto;
    EXPECT_TRUE(sp.Load(std::move(moved)).ok());
    EXPECT_EQ(moved_ptr, &sp.model_proto());
    RunTest(sp);
  }

  // Restrict Vocabulary.
  {
    SentencePieceProcessor sp;
    EXPECT_TRUE(sp.Load(model_proto).ok());
    EXPECT_TRUE(sp.SetVocabulary({"a", "b", "c"}).ok());  // remove "ab"

    const std::vector<std::string> expected_str = {WS, "a", "b", "c"};
    std::vector<std::string> sps;
    EXPECT_TRUE(sp.Encode("abc", &sps).ok());
    EXPECT_EQ(expected_str, sps);

    std::vector<int> ids;
    const std::vector<int> expected_id = {7, 3, 4, 5};
    EXPECT_TRUE(sp.Encode("abc", &ids).ok());
    EXPECT_EQ(expected_id, ids);
  }
}

TEST(SentencePieceProcessorTest, SkipNormalizationTest) {
  ModelProto model_proto;
  auto *sp1 = model_proto.add_pieces();
  auto *sp2 = model_proto.add_pieces();

  sp1->set_type(ModelProto::SentencePiece::UNKNOWN);
  sp1->set_piece("<unk>");
  sp2->set_type(ModelProto::SentencePiece::USER_DEFINED);
  sp2->set_piece("<USER>");

  AddPiece(&model_proto, "a", 0.0);
  AddPiece(&model_proto, "b", 0.3);
  AddPiece(&model_proto, "c", 0.2);
  AddPiece(&model_proto, "u", 0.2);
  AddPiece(&model_proto, "s", 0.2);
  AddPiece(&model_proto, "e", 0.2);
  AddPiece(&model_proto, "r", 0.2);

  *(model_proto.mutable_normalizer_spec()) =
      SentencePieceTrainer::GetNormalizerSpec("nmt_nfkc_cf");

  SentencePieceProcessor sp;
  EXPECT_TRUE(sp.Load(model_proto).ok());

  std::vector<std::string> pieces;
  EXPECT_TRUE(sp.Encode("AB<USER>C<uSEr>", &pieces).ok());
  for (const auto &sp : pieces) LOG(INFO) << sp;
  EXPECT_EQ(std::vector<std::string>(
                {WS, "a", "b", "<USER>", "c", "<", "u", "s", "e", "r", ">"}),
            pieces);
}

TEST(SentencePieceProcessorTest, ExtraOptionsUndefinedTest) {
  ModelProto model_proto;
  auto *sp1 = model_proto.add_pieces();

  // No BOS/EOS.
  sp1->set_type(ModelProto::SentencePiece::UNKNOWN);
  sp1->set_piece("<unk>");

  AddPiece(&model_proto, "a", 0.0);
  AddPiece(&model_proto, "b", 0.3);
  AddPiece(&model_proto, "c", 0.2);
  AddPiece(&model_proto, "ab", 1.0);

  SentencePieceProcessor sp;
  EXPECT_TRUE(sp.Load(model_proto).ok());

  EXPECT_FALSE(sp.SetEncodeExtraOptions("bos").ok());
  EXPECT_FALSE(sp.SetDecodeExtraOptions("eos").ok());
}

TEST(SentencePieceProcessorTest, OverrideSpecialPieceTest) {
  ModelProto model_proto;
  auto *sp1 = model_proto.add_pieces();
  auto *sp2 = model_proto.add_pieces();
  auto *sp3 = model_proto.add_pieces();

  model_proto.mutable_trainer_spec()->set_unk_piece("__UNK__");
  model_proto.mutable_trainer_spec()->set_bos_piece("__BOS__");
  model_proto.mutable_trainer_spec()->set_eos_piece("__EOS__");
  model_proto.mutable_trainer_spec()->set_pad_piece("__PAD__");

  // No BOS/EOS.
  sp1->set_type(ModelProto::SentencePiece::UNKNOWN);
  sp1->set_piece("__UNK__");
  sp2->set_type(ModelProto::SentencePiece::CONTROL);
  sp2->set_piece("__BOS__");
  sp3->set_type(ModelProto::SentencePiece::CONTROL);
  sp3->set_piece("__EOS__");

  AddPiece(&model_proto, "a", 0.0);
  AddPiece(&model_proto, "b", 0.3);

  SentencePieceProcessor sp;
  EXPECT_TRUE(sp.Load(model_proto).ok());
  EXPECT_EQ(0, sp.unk_id());
  EXPECT_EQ(1, sp.bos_id());
  EXPECT_EQ(2, sp.eos_id());
  EXPECT_EQ(-1, sp.pad_id());

  EXPECT_EQ("__UNK__", sp.IdToPiece(sp.unk_id()));
  EXPECT_EQ("__BOS__", sp.IdToPiece(sp.bos_id()));
  EXPECT_EQ("__EOS__", sp.IdToPiece(sp.eos_id()));
}

TEST(SentencePieceProcessorTest, VocabularyTest) {
  ModelProto model_proto;
  auto *sp1 = model_proto.add_pieces();
  auto *sp2 = model_proto.add_pieces();
  auto *sp3 = model_proto.add_pieces();

  auto GetInlineFilename = [](const std::string content) {
    {
      auto out = filesystem::NewWritableFile(
          util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "vocab.txt"));
      out->Write(content);
    }
    return util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "vocab.txt");
  };

  sp1->set_type(ModelProto::SentencePiece::UNKNOWN);
  sp1->set_piece("<unk>");
  sp2->set_type(ModelProto::SentencePiece::CONTROL);
  sp2->set_piece("<s>");
  sp3->set_type(ModelProto::SentencePiece::CONTROL);
  sp3->set_piece("</s>");

  AddPiece(&model_proto, "aa", 0.0);
  AddPiece(&model_proto, "bb", 0.0);
  AddPiece(&model_proto, "cc", 0.0);
  AddPiece(&model_proto, "dd", 0.0);
  AddPiece(&model_proto, "e", 0.0);

  SentencePieceProcessor sp;
  EXPECT_TRUE(sp.Load(model_proto).ok());

  EXPECT_FALSE(sp.IsUnused(0));
  EXPECT_FALSE(sp.IsUnused(1));
  EXPECT_FALSE(sp.IsUnused(2));
  EXPECT_FALSE(sp.IsUnused(3));
  EXPECT_FALSE(sp.IsUnused(4));
  EXPECT_FALSE(sp.IsUnused(5));
  EXPECT_FALSE(sp.IsUnused(6));
  EXPECT_FALSE(sp.IsUnused(7));

  EXPECT_TRUE(sp.SetVocabulary({"aa", "dd", "e"}).ok());

  EXPECT_FALSE(sp.IsUnused(0));
  EXPECT_FALSE(sp.IsUnused(1));
  EXPECT_FALSE(sp.IsUnused(2));
  EXPECT_FALSE(sp.IsUnused(3));
  EXPECT_TRUE(sp.IsUnused(4));
  EXPECT_TRUE(sp.IsUnused(5));
  EXPECT_FALSE(sp.IsUnused(6));
  EXPECT_FALSE(sp.IsUnused(7));  // single char "e" is always used.

  EXPECT_TRUE(sp.ResetVocabulary().ok());

  EXPECT_FALSE(sp.IsUnused(3));
  EXPECT_FALSE(sp.IsUnused(4));
  EXPECT_FALSE(sp.IsUnused(5));
  EXPECT_FALSE(sp.IsUnused(6));
  EXPECT_FALSE(sp.IsUnused(7));

  EXPECT_TRUE(sp.SetVocabulary({"bb"}).ok());
  EXPECT_TRUE(sp.IsUnused(3));
  EXPECT_FALSE(sp.IsUnused(4));
  EXPECT_TRUE(sp.IsUnused(5));
  EXPECT_TRUE(sp.IsUnused(6));
  EXPECT_FALSE(sp.IsUnused(7));

  EXPECT_TRUE(sp.LoadVocabulary(GetInlineFilename("aa\t1\ndd\t2\n"), 2).ok());
  EXPECT_TRUE(sp.IsUnused(3));
  EXPECT_TRUE(sp.IsUnused(4));
  EXPECT_TRUE(sp.IsUnused(5));
  EXPECT_FALSE(sp.IsUnused(6));
  EXPECT_FALSE(sp.IsUnused(7));

  EXPECT_TRUE(sp.LoadVocabulary(GetInlineFilename("aa\t1\ndd\t1\n"), 2).ok());
  EXPECT_TRUE(sp.IsUnused(3));
  EXPECT_TRUE(sp.IsUnused(4));
  EXPECT_TRUE(sp.IsUnused(5));
  EXPECT_TRUE(sp.IsUnused(6));
  EXPECT_FALSE(sp.IsUnused(7));

  EXPECT_TRUE(sp.LoadVocabulary(GetInlineFilename("aa\t1\ndd\t1\n"), 1).ok());
  EXPECT_FALSE(sp.IsUnused(3));
  EXPECT_TRUE(sp.IsUnused(4));
  EXPECT_TRUE(sp.IsUnused(5));
  EXPECT_FALSE(sp.IsUnused(6));
  EXPECT_FALSE(sp.IsUnused(7));

  EXPECT_TRUE(sp.LoadVocabulary(GetInlineFilename("aa\t0\ndd\t0\n"), 0).ok());
  EXPECT_FALSE(sp.IsUnused(3));
  EXPECT_TRUE(sp.IsUnused(4));
  EXPECT_TRUE(sp.IsUnused(5));
  EXPECT_FALSE(sp.IsUnused(6));
  EXPECT_FALSE(sp.IsUnused(7));

  // No frequency.
  EXPECT_TRUE(sp.LoadVocabulary(GetInlineFilename("aa\ndd\n"), 1).ok());
  EXPECT_FALSE(sp.IsUnused(3));
  EXPECT_TRUE(sp.IsUnused(4));
  EXPECT_TRUE(sp.IsUnused(5));
  EXPECT_FALSE(sp.IsUnused(6));
  EXPECT_FALSE(sp.IsUnused(7));
}

TEST(SentencePieceProcessorTest, ImmutableSentencePieceTextTest) {
  ImmutableSentencePieceText spt;
  EXPECT_TRUE(spt.text().empty());
  EXPECT_EQ(spt.score(), 0.0);
  EXPECT_TRUE(spt.SerializeAsString().empty());

  auto* v = spt.mutable_proto();

  v->set_text("hello world");
  v->set_score(1.0);
  for (int i = 0; i < 10; ++i) {
    auto* p = v->add_pieces();
    p->set_surface(absl::StrCat("surface_", i));
    p->set_piece(absl::StrCat("surface_", i));
    p->set_id(i);
    p->set_begin(i + 10);
    p->set_end(i + 20);
  }

  EXPECT_EQ(v->pieces_size(), spt.pieces_size());
  for (int i = 0; i < spt.pieces_size(); ++i) {
    EXPECT_EQ(v->pieces(i).surface(), spt.pieces(i).surface());
    EXPECT_EQ(v->pieces(i).piece(), spt.pieces(i).piece());
    EXPECT_EQ(v->pieces(i).id(), spt.pieces(i).id());
    EXPECT_EQ(v->pieces(i).begin(), spt.pieces(i).begin());
    EXPECT_EQ(v->pieces(i).end(), spt.pieces(i).end());
  }

  auto check_proto = [&v](const ImmutableSentencePieceText& s) {
    int n = 0;
    for (auto& p : s.pieces()) {
      EXPECT_EQ(v->pieces(n).surface(), p.surface());
      EXPECT_EQ(v->pieces(n).piece(), p.piece());
      EXPECT_EQ(v->pieces(n).id(), p.id());
      EXPECT_EQ(v->pieces(n).begin(), p.begin());
      EXPECT_EQ(v->pieces(n).end(), p.end());
      ++n;
    }
    EXPECT_EQ(v->text(), s.text());
    EXPECT_EQ(v->score(), s.score());
    EXPECT_EQ(v->SerializeAsString(), s.SerializeAsString());
  };

  // test copy.
  const auto spt2 = spt;
  check_proto(spt2);

  // test assign.
  const ImmutableSentencePieceText spt3(spt);
  check_proto(spt3);

  // default piece.
  const ImmutableSentencePieceText_ImmutableSentencePiece piece;
  EXPECT_TRUE(piece.surface().empty());
  EXPECT_TRUE(piece.piece().empty());
  EXPECT_EQ(piece.begin(), 0);
  EXPECT_EQ(piece.end(), 0);
  EXPECT_EQ(piece.id(), 0);
}

TEST(SentencePieceProcessorTest, ImmutableNBestSentencePieceTextTest) {
  ImmutableNBestSentencePieceText spt;
  EXPECT_EQ(spt.nbests_size(), 0);
  EXPECT_TRUE(spt.SerializeAsString().empty());

  auto* v = spt.mutable_proto();

  for (int i = 0; i < 10; ++i) {
    auto* p = v->add_nbests();
    p->set_text(absl::StrCat("text_", i));
    p->set_score(2.0 * i);
  }

  auto check_proto = [&v](const ImmutableNBestSentencePieceText& s) {
    EXPECT_EQ(v->nbests_size(), s.nbests_size());
    for (int i = 0; i < v->nbests_size(); ++i) {
      EXPECT_EQ(v->nbests(i).text(), s.nbests(i).text());
      EXPECT_EQ(v->nbests(i).score(), s.nbests(i).score());
    }
    EXPECT_EQ(v->SerializeAsString(), s.SerializeAsString());
  };

  check_proto(spt);

  // test copy.
  const auto spt2 = spt;
  check_proto(spt2);

  // test assign.
  const ImmutableNBestSentencePieceText spt3(spt);
  check_proto(spt3);
}

TEST(SentencePieceProcessorTest, ConvertToUnicodeSpansTest) {
  auto make_spt = [&](const std::vector<std::string>& tokens) {
    ImmutableSentencePieceText ispt;
    auto* spt = ispt.mutable_proto();
    int prev = 0;
    std::string text;
    for (const auto& tok : tokens) {
      auto* piece = spt->add_pieces();
      piece->set_surface(tok);
      piece->set_piece(tok);
      piece->set_begin(prev);
      piece->set_end(prev + tok.size());
      prev += tok.size();
      text += tok;
    }
    spt->set_text(text);
    ispt.ConvertToUnicodeSpans();
    return ispt;
  };

  {
    const auto spt = make_spt({"hello", "_world", "."});
    EXPECT_EQ(spt.pieces_size(), 3);
    EXPECT_EQ(spt.pieces(0).begin(), 0);
    EXPECT_EQ(spt.pieces(0).end(), 5);
    EXPECT_EQ(spt.pieces(1).begin(), 5);
    EXPECT_EQ(spt.pieces(1).end(), 11);
    EXPECT_EQ(spt.pieces(2).begin(), 11);
    EXPECT_EQ(spt.pieces(2).end(), 12);
  }

  {
    const auto spt = make_spt({"これは", "test", "です"});
    EXPECT_EQ(spt.pieces_size(), 3);
    EXPECT_EQ(spt.pieces(0).begin(), 0);
    EXPECT_EQ(spt.pieces(0).end(), 3);
    EXPECT_EQ(spt.pieces(1).begin(), 3);
    EXPECT_EQ(spt.pieces(1).end(), 7);

    EXPECT_EQ(spt.pieces(2).begin(), 7);
    EXPECT_EQ(spt.pieces(2).end(), 9);
  }

  {
    const auto spt = make_spt({"いABは", "にほCD", "へと"});
    EXPECT_EQ(spt.pieces_size(), 3);
    EXPECT_EQ(spt.pieces(0).begin(), 0);
    EXPECT_EQ(spt.pieces(0).end(), 4);
    EXPECT_EQ(spt.pieces(1).begin(), 4);
    EXPECT_EQ(spt.pieces(1).end(), 8);
    EXPECT_EQ(spt.pieces(2).begin(), 8);
    EXPECT_EQ(spt.pieces(2).end(), 10);
  }
}

}  // namespace sentencepiece
