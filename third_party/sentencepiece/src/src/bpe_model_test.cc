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

#include "bpe_model.h"

#include <cstdio>
#include <string>

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
    auto get_tokens = [](const EncodeResult &result) {
      std::string out;
      for (const auto &r : result) {
        if (!result.empty()) out += ' ';
        out += std::string(r.first);
      }
      return out;
    };

    const Model model(model_proto);
    const std::vector<double> kAlpha = {0.0, 0.1, 0.5, 0.7, 0.9};
    for (const auto alpha : kAlpha) {
      constexpr int kTrial = 100000;
      std::map<std::string, int> freq;
      for (int n = 0; n < kTrial; ++n)
        freq[get_tokens(
            model.SampleEncode("abcd", static_cast<float>(alpha)))]++;
      int num = 0;
      if (alpha == 0.0)
        EXPECT_EQ(1, freq.size());
      else
        EXPECT_GT(freq.size(), 1);
      for (const auto &it : freq) num += it.second;
      EXPECT_EQ(num, kTrial);
    }
  }
}

TEST(BPEModelTest, EncodeWithDeepUnusedMergeChainTest) {
  ModelProto model_proto = MakeBaseModelProto();

  const int kDepth = 101;  // Should exceed kMaxBpeResegmentDepth (100)

  // Add base characters
  std::vector<std::string> chars;
  for (int i = 0; i <= kDepth + 1; ++i) {
    std::string c = string_util::UnicodeCharToUTF8(0x1000 + i);
    chars.push_back(c);
    AddPiece(&model_proto, c, 0.0);
  }

  // Add merged pieces
  std::string current_merge = chars[0] + chars[1];
  AddPiece(&model_proto, current_merge, -1.0);
  int merge_start_index = 3 + kDepth + 2;

  for (int i = 1; i <= kDepth; ++i) {
    current_merge += chars[i + 1];
    AddPiece(&model_proto, current_merge, -static_cast<float>(i + 1));
  }

  // Set all merges to UNUSED
  for (int i = 0; i <= kDepth; ++i) {
    model_proto.mutable_pieces(merge_start_index + i)
        ->set_type(ModelProto::SentencePiece::UNUSED);
  }

  const Model model(model_proto);

  std::string input;
  for (const auto &c : chars) {
    input += c;
  }

  const auto result = model.Encode(input);

  EXPECT_EQ(kDepth + 1, result.size());
  EXPECT_EQ(chars[0] + chars[1], result[0].first);
  for (int i = 1; i <= kDepth; ++i) {
    EXPECT_EQ(chars[i + 1], result[i].first);
  }
}

TEST(BPEModelTest, ControlSymbolsNoMergeTest) {
  ModelProto model_proto = MakeBaseModelProto();

  // For <s> (BOS)
  AddPiece(&model_proto, "<", -1.0);  // ID 3
  AddPiece(&model_proto, "s", -1.0);  // ID 4
  AddPiece(&model_proto, ">", -1.0);  // ID 5
  AddPiece(&model_proto, "<s", 0.0);  // ID 6

  // For <unk> (UNKNOWN)
  AddPiece(&model_proto, "u", -1.0);   // ID 7
  AddPiece(&model_proto, "n", -1.0);   // ID 8
  AddPiece(&model_proto, "k", -1.0);   // ID 9
  AddPiece(&model_proto, "<u", 0.0);   // ID 10
  AddPiece(&model_proto, "nk", 0.0);   // ID 11 (Intermediate for BPE)
  AddPiece(&model_proto, "nk>", 0.0);  // ID 12

  const Model model(model_proto);

  {
    EncodeResult result = model.Encode("<s>");
    EXPECT_EQ(2, result.size());
    EXPECT_EQ("<s", result[0].first);
    EXPECT_EQ(">", result[1].first);
  }

  {
    EncodeResult result = model.Encode("<unk>");
    EXPECT_EQ(2, result.size());
    EXPECT_EQ("<u", result[0].first);
    EXPECT_EQ("nk>", result[1].first);
  }
}

TEST(BPEModelTest, ControlTokenMergeTest) {
  ModelProto model_proto;
  auto AddPieceWithType = [](ModelProto *proto, const std::string &piece,
                             float score,
                             ModelProto::SentencePiece::Type type) {
    auto *sp = proto->add_pieces();
    sp->set_piece(piece);
    sp->set_score(score);
    sp->set_type(type);
  };

  AddPieceWithType(&model_proto, "<unk>", 0.0,
                   ModelProto::SentencePiece::UNKNOWN);
  AddPieceWithType(&model_proto, "<s>", 0.0,
                   ModelProto::SentencePiece::CONTROL);
  AddPieceWithType(&model_proto, "</s>", 0.0,
                   ModelProto::SentencePiece::CONTROL);
  AddPieceWithType(&model_proto, "<", -1.0, ModelProto::SentencePiece::NORMAL);
  AddPieceWithType(&model_proto, "s", -1.0, ModelProto::SentencePiece::NORMAL);
  AddPieceWithType(&model_proto, ">", -1.0, ModelProto::SentencePiece::NORMAL);
  AddPieceWithType(&model_proto, "<s", 0.0, ModelProto::SentencePiece::NORMAL);

  const Model model(model_proto);
  const auto result = model.Encode("<s>");

  // Expected behavior: do NOT merge characters into CONTROL symbol <s>.
  // Instead, it should segment into "<s" (ID 6) and ">" (ID 5).
  EXPECT_EQ(2, result.size());
  EXPECT_EQ("<s", result[0].first);
  EXPECT_EQ(6, result[0].second);
  EXPECT_EQ(">", result[1].first);
  EXPECT_EQ(5, result[1].second);
}

TEST(BPEModelTest, SpecialSymbolsNoSegmentTest) {
  ModelProto model_proto = MakeBaseModelProto();
  model_proto.mutable_trainer_spec()->set_byte_fallback(true);

  // Add 256 byte pieces to satisfy InitializePieces check.
  for (int i = 0; i < 256; ++i) {
    auto *sp = model_proto.add_pieces();
    sp->set_piece(ByteToPiece(i));
    sp->set_type(ModelProto::SentencePiece::BYTE);
  }

  // Add an UNUSED piece.
  // We use "abc" as UNUSED piece.
  // And we add its characters so it can be split.
  AddPiece(&model_proto, "a", -1.0);
  AddPiece(&model_proto, "b", -1.0);
  AddPiece(&model_proto, "c", -1.0);
  AddPiece(&model_proto, "abc", 0.0);
  model_proto.mutable_pieces(model_proto.pieces_size() - 1)
      ->set_type(ModelProto::SentencePiece::UNUSED);

  // Add character pieces for "<unk>" and "<s>"
  AddPiece(&model_proto, "<", -1.0);
  AddPiece(&model_proto, "u", -1.0);
  AddPiece(&model_proto, "n", -1.0);
  AddPiece(&model_proto, "k", -1.0);
  AddPiece(&model_proto, "s", -1.0);
  AddPiece(&model_proto, ">", -1.0);

  // Add intermediate pieces that could form "<s>" and "<unk>"
  AddPiece(&model_proto, "<s", 0.0);
  AddPiece(&model_proto, "<u", 0.0);
  AddPiece(&model_proto, "nk", 0.0);

  // Add intermediate pieces for byte piece "<0x0A>" (newline)
  // Byte 10 is 0x0A. ByteToPiece(10) is "<0x0A>".
  // Characters: "<", "0", "x", "0", "A", ">"
  AddPiece(&model_proto, "0", -1.0);
  AddPiece(&model_proto, "x", -1.0);
  AddPiece(&model_proto, "A", -1.0);
  AddPiece(&model_proto, "<0", 0.0);
  AddPiece(&model_proto, "x0", 0.0);
  AddPiece(&model_proto, "A>", 0.0);

  Model model(model_proto);

  // 1. Test CONTROL symbol "<s>".
  // Expected: split to "<s" and ">".
  {
    auto result = model.Encode("<s>");
    ASSERT_EQ(2, result.size());
    EXPECT_EQ("<s", result[0].first);
    EXPECT_EQ(">", result[1].first);
  }

  // 2. Test UNKNOWN symbol "<unk>".
  // Expected: split and not matched as "<unk>".
  {
    auto result = model.Encode("<unk>");
    EXPECT_GT(result.size(), 1);
    for (const auto &part : result) {
      EXPECT_NE("<unk>", part.first);
    }
  }

  // 3. Test UNUSED symbol "abc".
  // Expected: split to "a", "b", "c".
  {
    auto result = model.Encode("abc");
    ASSERT_EQ(3, result.size());
    EXPECT_EQ("a", result[0].first);
    EXPECT_EQ("b", result[1].first);
    EXPECT_EQ("c", result[2].first);
  }

  // 4. Test BYTE symbol "<0x0A>".
  // Expected: NOT matched as "<0x0A>".
  {
    std::string byte_piece = ByteToPiece(10);
    auto result = model.Encode(byte_piece);
    EXPECT_GT(result.size(), 1);
    for (const auto &part : result) {
      EXPECT_NE(byte_piece, part.first);
    }
  }
}

}  // namespace
}  // namespace bpe
}  // namespace sentencepiece
