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

#include <cstdio>
#include <string>

#include "bpe_model.h"
#include "model_interface.h"
#include "testharness.h"

namespace sentencepiece {
namespace bpe {
namespace {

ModelProto MakeBaseModelProto() {
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

  return model_proto;
}

void AddPiece(ModelProto *model_proto, const std::string &piece,
              float score = 0.0) {
  auto *sp = model_proto->add_pieces();
  sp->set_piece(piece);
  sp->set_score(score);
}

TEST(BPEModelTest, EncodeTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "ab", 0.0);         // 3
  AddPiece(&model_proto, "cd", -0.1);        // 4
  AddPiece(&model_proto, "abc", -0.2);       // 5
  AddPiece(&model_proto, "a", -0.3);         // 6
  AddPiece(&model_proto, "b", -0.4);         // 7
  AddPiece(&model_proto, "c", -0.5);         // 8
  AddPiece(&model_proto, "ABC", -0.5);       // 9
  AddPiece(&model_proto, "abcdabcd", -0.5);  // 10
  AddPiece(&model_proto, "q", -0.5);         // 11
  AddPiece(&model_proto, "r", -0.5);         // 12
  AddPiece(&model_proto, "qr", -0.5);        // 13
  model_proto.mutable_pieces(9)->set_type(   // ABC
      ModelProto::SentencePiece::USER_DEFINED);
  model_proto.mutable_pieces(10)->set_type(  // abcdabcd
      ModelProto::SentencePiece::USER_DEFINED);
  model_proto.mutable_pieces(11)->set_type(  // q
      ModelProto::SentencePiece::USER_DEFINED);
  model_proto.mutable_pieces(12)->set_type(  // r
      ModelProto::SentencePiece::USER_DEFINED);

  const Model model(model_proto);

  EncodeResult result;

  result = model.Encode("");
  EXPECT_TRUE(result.empty());

  result = model.Encode("abc");
  EXPECT_EQ(1, result.size());
  EXPECT_EQ("abc", result[0].first);

  result = model.Encode("AB");
  EXPECT_EQ(2, result.size());
  EXPECT_EQ("A", result[0].first);
  EXPECT_EQ("B", result[1].first);

  result = model.Encode("abcd");
  EXPECT_EQ(2, result.size());
  EXPECT_EQ("ab", result[0].first);
  EXPECT_EQ("cd", result[1].first);

  result = model.Encode("abcc");
  EXPECT_EQ(2, result.size());
  EXPECT_EQ("abc", result[0].first);
  EXPECT_EQ("c", result[1].first);

  result = model.Encode("xabcabaabcdd");
  EXPECT_EQ(7, result.size());
  EXPECT_EQ("x", result[0].first);
  EXPECT_EQ("abc", result[1].first);
  EXPECT_EQ("ab", result[2].first);
  EXPECT_EQ("a", result[3].first);
  EXPECT_EQ("ab", result[4].first);
  EXPECT_EQ("cd", result[5].first);
  EXPECT_EQ("d", result[6].first);

  // all unknown.
  result = model.Encode("xyz東京");
  EXPECT_EQ(5, result.size());
  EXPECT_EQ("x", result[0].first);
  EXPECT_EQ("y", result[1].first);
  EXPECT_EQ("z", result[2].first);
  EXPECT_EQ("東", result[3].first);
  EXPECT_EQ("京", result[4].first);

  // User defined
  result = model.Encode("ABC");
  EXPECT_EQ(1, result.size());
  EXPECT_EQ("ABC", result[0].first);

  result = model.Encode("abABCcd");
  EXPECT_EQ(3, result.size());
  EXPECT_EQ("ab", result[0].first);
  EXPECT_EQ("ABC", result[1].first);
  EXPECT_EQ("cd", result[2].first);

  // middle "abcdabcd" is user defined.
  result = model.Encode("ababcdabcdcd");
  EXPECT_EQ(3, result.size());
  EXPECT_EQ("ab", result[0].first);
  EXPECT_EQ("abcdabcd", result[1].first);
  EXPECT_EQ("cd", result[2].first);

  result = model.Encode("abqrcd");
  EXPECT_EQ(4, result.size());
  EXPECT_EQ("ab", result[0].first);
  EXPECT_EQ("q", result[1].first);
  EXPECT_EQ("r", result[2].first);
  EXPECT_EQ("cd", result[3].first);
}

TEST(BPEModelTest, EncodeAmbiguousTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "aa", -0.1);
  AddPiece(&model_proto, "bb", -0.2);
  AddPiece(&model_proto, "ab", -0.3);
  AddPiece(&model_proto, "a", -0.4);
  AddPiece(&model_proto, "b", -0.5);

  const Model model(model_proto);

  EncodeResult result;

  // leftmost symbols are merged first.
  result = model.Encode("aaa");
  EXPECT_EQ(2, result.size());
  EXPECT_EQ("aa", result[0].first);
  EXPECT_EQ("a", result[1].first);

  // "bb" is replaced earlier than "ab".
  result = model.Encode("aabb");
  EXPECT_EQ(2, result.size());
  EXPECT_EQ("aa", result[0].first);
  EXPECT_EQ("bb", result[1].first);

  // "bb" is replaced earlier than "ab".
  result = model.Encode("aaabbb");
  EXPECT_EQ(4, result.size());
  EXPECT_EQ("aa", result[0].first);
  EXPECT_EQ("a", result[1].first);
  EXPECT_EQ("bb", result[2].first);
  EXPECT_EQ("b", result[3].first);

  result = model.Encode("aaaba");
  EXPECT_EQ(3, result.size());
  EXPECT_EQ("aa", result[0].first);
  EXPECT_EQ("ab", result[1].first);
  EXPECT_EQ("a", result[2].first);

  // makes a broken utf-8
  const std::string broken_utf8 = std::string("あ").substr(0, 1);
  result = model.Encode(broken_utf8);
  EXPECT_EQ(1, result.size());
  EXPECT_EQ(broken_utf8, result[0].first);
}

TEST(BPEModelTest, NotSupportedTest) {
  ModelProto model_proto = MakeBaseModelProto();
  const Model model(model_proto);
  EXPECT_EQ(NBestEncodeResult(), model.NBestEncode("test", 10));
}

TEST(BPEModelTest, EncodeWithUnusedTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "abcd", 10.0);  // 3
  AddPiece(&model_proto, "abc", 5.0);    // 4
  AddPiece(&model_proto, "ab", 2.0);     // 5
  AddPiece(&model_proto, "cd", 1.0);     // 6
  AddPiece(&model_proto, "a", 0.0);      // 7
  AddPiece(&model_proto, "b", 0.0);      // 8
  AddPiece(&model_proto, "c", 0.0);      // 9
  AddPiece(&model_proto, "d", 0.0);      // 10

  // No unused.
  {
    const Model model(model_proto);
    const auto result = model.Encode("abcd");
    EXPECT_EQ(1, result.size());
    EXPECT_EQ("abcd", result[0].first);
  }

  {
    model_proto.mutable_pieces(3)->set_type(ModelProto::SentencePiece::UNUSED);
    const Model model(model_proto);
    const auto result = model.Encode("abcd");
    EXPECT_EQ(2, result.size());
    EXPECT_EQ("abc", result[0].first);
    EXPECT_EQ("d", result[1].first);
  }

  {
    // The parent rule "abc" is still alive even if the child "ab" is unused.
    model_proto.mutable_pieces(3)->set_type(ModelProto::SentencePiece::UNUSED);
    model_proto.mutable_pieces(5)->set_type(ModelProto::SentencePiece::UNUSED);
    const Model model(model_proto);
    const auto result = model.Encode("abcd");
    EXPECT_EQ(2, result.size());
    EXPECT_EQ("abc", result[0].first);
    EXPECT_EQ("d", result[1].first);
  }

  {
    // This is tricky case. Even though "cd" is alive, it is not used, as
    // it is not merged during the segmentation step.
    // Segmentation: a|b|c|d => ab|c|d| => abc|d => abcd
    // Resegmentation: abcd => abc|d => ab|c|d. ("abcd", "abc" are unsued)
    model_proto.mutable_pieces(3)->set_type(ModelProto::SentencePiece::UNUSED);
    model_proto.mutable_pieces(4)->set_type(ModelProto::SentencePiece::UNUSED);
    model_proto.mutable_pieces(5)->set_type(ModelProto::SentencePiece::NORMAL);
    const Model model(model_proto);
    const auto result = model.Encode("abcd");
    EXPECT_EQ(3, result.size());
    EXPECT_EQ("ab", result[0].first);
    EXPECT_EQ("c", result[1].first);
    EXPECT_EQ("d", result[2].first);
  }
}

TEST(SampleModelTest, EncodeTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "ab", 0.0);
  AddPiece(&model_proto, "cd", -0.1);
  AddPiece(&model_proto, "abc", -0.2);
  AddPiece(&model_proto, "abcd", -0.3);

  // No regularization
  {
    const Model model(model_proto);
    const auto result = model.Encode("abcd");
    EXPECT_EQ(1, result.size());
    EXPECT_EQ("abcd", result[0].first);
  }

  {
    auto get_tokens = [](const EncodeResult& result) {
      std::string out;
      for (const auto& r : result) {
        if (!result.empty()) {
          out += ' ';
        }
        out += std::string(r.first);
      }
      return out;
    };

    const Model model(model_proto);
    const std::vector<double> kAlpha = {0.0, 0.1, 0.5, 0.7, 0.9};
    for (const auto alpha : kAlpha) {
      constexpr int kTrial = 100000;
      std::map<std::string, int> freq;
      for (int n = 0; n < kTrial; ++n) {
        freq[get_tokens(
            model.SampleEncode("abcd", static_cast<float>(alpha)))]++;
      }
      int num = 0;
      if (alpha == 0.0) {
        EXPECT_EQ(1, freq.size());
      } else {
        EXPECT_GT(freq.size(), 1);
      }
      for (const auto& it : freq) {
        num += it.second;
      }
      EXPECT_EQ(num, kTrial);
    }
  }
}

}  // namespace
}  // namespace bpe
}  // namespace sentencepiece
