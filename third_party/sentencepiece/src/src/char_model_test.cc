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

#include "char_model.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {
namespace character {
namespace {

// Space symbol (U+2581)
#define WS "\xe2\x96\x81"

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

TEST(ModelTest, EncodeTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, WS, 0.0);
  AddPiece(&model_proto, "a", 0.1);
  AddPiece(&model_proto, "b", 0.2);
  AddPiece(&model_proto, "c", 0.3);
  AddPiece(&model_proto, "d", 0.4);
  AddPiece(&model_proto, "ABC", 0.4);
  model_proto.mutable_pieces(8)->set_type(
      ModelProto::SentencePiece::USER_DEFINED);

  const Model model(model_proto);

  EncodeResult result;

  result = model.Encode("");
  EXPECT_TRUE(result.empty());

  result = model.Encode(WS "a" WS "b" WS "c");
  EXPECT_EQ(6, result.size());
  EXPECT_EQ(WS, result[0].first);
  EXPECT_EQ("a", result[1].first);
  EXPECT_EQ(WS, result[2].first);
  EXPECT_EQ("b", result[3].first);
  EXPECT_EQ(WS, result[4].first);
  EXPECT_EQ("c", result[5].first);

  result = model.Encode(WS "ab" WS "cd" WS "abc");
  EXPECT_EQ(10, result.size());
  EXPECT_EQ(WS, result[0].first);
  EXPECT_EQ("a", result[1].first);
  EXPECT_EQ("b", result[2].first);
  EXPECT_EQ(WS, result[3].first);
  EXPECT_EQ("c", result[4].first);
  EXPECT_EQ("d", result[5].first);
  EXPECT_EQ(WS, result[6].first);
  EXPECT_EQ("a", result[7].first);
  EXPECT_EQ("b", result[8].first);
  EXPECT_EQ("c", result[9].first);

  // makes a broken utf-8
  const std::string broken_utf8 = std::string("あ").substr(0, 1);
  result = model.Encode(broken_utf8);
  EXPECT_EQ(1, result.size());
  EXPECT_EQ(broken_utf8, result[0].first);

  // "ABC" is treated as one piece, as it is USER_DEFINED.
  result = model.Encode(WS "abABCcd");
  EXPECT_EQ(6, result.size());
  EXPECT_EQ(WS, result[0].first);
  EXPECT_EQ("a", result[1].first);
  EXPECT_EQ("b", result[2].first);
  EXPECT_EQ("ABC", result[3].first);
  EXPECT_EQ("c", result[4].first);
  EXPECT_EQ("d", result[5].first);
}

TEST(CharModelTest, NotSupportedTest) {
  ModelProto model_proto = MakeBaseModelProto();
  const Model model(model_proto);
  EXPECT_EQ(NBestEncodeResult(), model.NBestEncode("test", 10));
  EXPECT_EQ(EncodeResult(), model.SampleEncode("test", 0.1));
}

}  // namespace
}  // namespace character
}  // namespace sentencepiece
