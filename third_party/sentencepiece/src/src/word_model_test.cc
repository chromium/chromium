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

#include "sentencepiece_model.pb.h"
#include "testharness.h"
#include "util.h"
#include "word_model.h"

namespace sentencepiece {
namespace word {
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

TEST(WordModelTest, EncodeTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, WS "ab");
  AddPiece(&model_proto, WS "cd");
  AddPiece(&model_proto, WS "abc");
  AddPiece(&model_proto, WS "a", 0.1);
  AddPiece(&model_proto, WS "b", 0.2);
  AddPiece(&model_proto, WS "c", 0.3);
  AddPiece(&model_proto, WS "d", 0.4);

  const Model model(model_proto);

  EncodeResult result;

  result = model.Encode("");
  EXPECT_TRUE(result.empty());

  result = model.Encode(WS "a" WS "b" WS "c");
  EXPECT_EQ(3, result.size());
  EXPECT_EQ(WS "a", result[0].first);
  EXPECT_EQ(WS "b", result[1].first);
  EXPECT_EQ(WS "c", result[2].first);

  result = model.Encode(WS "ab" WS "cd" WS "abc");
  EXPECT_EQ(3, result.size());
  EXPECT_EQ(WS "ab", result[0].first);
  EXPECT_EQ(WS "cd", result[1].first);
  EXPECT_EQ(WS "abc", result[2].first);
}

TEST(WordModelTest, NotSupportedTest) {
  ModelProto model_proto = MakeBaseModelProto();
  const Model model(model_proto);
  EXPECT_EQ(NBestEncodeResult(), model.NBestEncode("test", 10));
  EXPECT_EQ(EncodeResult(), model.SampleEncode("test", 0.1));
}

}  // namespace
}  // namespace word
}  // namespace sentencepiece
