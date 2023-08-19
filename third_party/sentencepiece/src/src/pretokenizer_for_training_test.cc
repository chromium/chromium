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
#include "pretokenizer_for_training.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "testharness.h"
#include "trainer_interface.h"

namespace sentencepiece {
namespace pretokenizer {

class MockPretokenizer : public PretokenizerForTrainingInterface {
 public:
  MockPretokenizer() {}
  ~MockPretokenizer() {}

  SentencePieceText Tokenize(absl::string_view text) const override {
    return spt_;
  }

  util::Status status() const override { return util::OkStatus(); }

  void SetOutput(const SentencePieceText &spt) { spt_ = spt; }

 private:
  SentencePieceText spt_;
};

TEST(PretokenizerForTrainingTest, BaseTest) {
  MockPretokenizer mock;

  {
    SentencePieceText spt;
    spt.set_text("I love sentencepiece");
    auto *p1 = spt.add_pieces();
    p1->set_surface("I");
    p1->set_begin(0);
    p1->set_end(1);

    auto *p2 = spt.add_pieces();
    p2->set_surface("love");
    p2->set_begin(2);
    p2->set_end(6);

    auto *p3 = spt.add_pieces();
    p3->set_surface("sentence");
    p3->set_begin(7);
    p3->set_end(15);

    auto *p4 = spt.add_pieces();
    p4->set_surface("piece");
    p4->set_begin(15);
    p4->set_end(20);

    mock.SetOutput(spt);

    const auto expected =
        absl::StrCat("I", TrainerInterface::kWSStr, "love",
                     TrainerInterface::kWSStr, "sentence||||piece");
    EXPECT_EQ(expected,
              absl::StrJoin(mock.PreTokenize("I love sentencepiece"), "||||"));
  }

  {
    SentencePieceText spt;
    spt.set_text("これはペンです");
    auto *p1 = spt.add_pieces();
    p1->set_surface("これ");
    p1->set_begin(0);
    p1->set_end(6);

    auto *p2 = spt.add_pieces();
    p2->set_surface("は");
    p2->set_begin(6);
    p2->set_end(9);

    auto *p3 = spt.add_pieces();
    p3->set_surface("ペン");
    p3->set_begin(9);
    p3->set_end(15);

    auto *p4 = spt.add_pieces();
    p4->set_surface("です");
    p4->set_begin(15);
    p4->set_end(21);

    mock.SetOutput(spt);

    const auto expected = "これ||||は||||ペン||||です";
    EXPECT_EQ(expected,
              absl::StrJoin(mock.PreTokenize("これはペンです"), "||||"));
  }
}

}  // namespace pretokenizer
}  // namespace sentencepiece
