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

#include "src/trainer_interface.h"

#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/util.h"

namespace sentencepiece {

// Space symbol
#define WS "\xe2\x96\x81"

TEST(TrainerInterfaceTest, IsValidSentencePieceTest) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  trainer_spec.set_model_prefix("model");
  trainer_spec.add_input("input");

  // Calls the default method for better coverage.
  TrainerInterface trainer(trainer_spec, normalizer_spec);
  EXPECT_TRUE(trainer.Train().ok());

  auto IsValid = [&trainer_spec, &normalizer_spec](const std::string &str) {
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    const string_util::UnicodeText text = string_util::UTF8ToUnicodeText(str);
    return trainer.IsValidSentencePiece(text);
  };

  EXPECT_FALSE(trainer.IsValidSentencePiece({0x01, 0x00, 0x01}));
  EXPECT_FALSE(trainer.IsValidSentencePiece({0x01, 0x00}));
  EXPECT_FALSE(trainer.IsValidSentencePiece({0x00, 0x01}));
  EXPECT_FALSE(trainer.IsValidSentencePiece({0x00}));

  // Default trainer spec.
  EXPECT_FALSE(IsValid(""));
  EXPECT_FALSE(IsValid("12345678912345678"));  // too long
  EXPECT_TRUE(IsValid("a"));
  EXPECT_TRUE(IsValid(WS));
  EXPECT_TRUE(IsValid(WS "a"));
  EXPECT_FALSE(IsValid("a" WS));
  EXPECT_FALSE(IsValid(WS "a" WS));
  EXPECT_FALSE(IsValid("a" WS "b"));
  EXPECT_FALSE(IsValid("a" WS "b" WS));
  EXPECT_TRUE(IsValid("あいう"));
  EXPECT_TRUE(IsValid("グーグル"));  // "ー" is a part of Katakana
  EXPECT_TRUE(IsValid("食べる"));
  EXPECT_FALSE(IsValid("漢字ABC"));  // mixed CJK scripts
  EXPECT_FALSE(IsValid("F1"));
  EXPECT_FALSE(IsValid("1F"));
  EXPECT_FALSE(IsValid("1A2"));
  EXPECT_TRUE(IsValid("$10"));  // $ and 1 are both "common" script.
  EXPECT_FALSE(IsValid("$ABC"));
  EXPECT_FALSE(IsValid("ab\tbc"));  // "\t" is UPP boundary.
  EXPECT_FALSE(IsValid("ab cd"));
  EXPECT_FALSE(IsValid("\0\0"));
  EXPECT_FALSE(IsValid("\0"));

  trainer_spec.set_split_by_whitespace(false);
  EXPECT_TRUE(IsValid(WS));
  EXPECT_TRUE(IsValid(WS "a"));
  EXPECT_FALSE(IsValid("a" WS));
  EXPECT_FALSE(IsValid(WS "a" WS));
  EXPECT_TRUE(IsValid("a" WS "b"));  // "a b" is a valid piece.
  EXPECT_TRUE(IsValid(WS "a" WS "b"));
  EXPECT_TRUE(IsValid(WS "a" WS "b" WS "c"));
  EXPECT_FALSE(IsValid("a" WS "b" WS));

  trainer_spec.set_split_by_unicode_script(false);
  EXPECT_TRUE(IsValid("あいう"));
  EXPECT_TRUE(IsValid("グーグル"));
  EXPECT_TRUE(IsValid("食べる"));
  EXPECT_TRUE(IsValid("漢字ABC"));
  EXPECT_TRUE(IsValid("F1"));
  EXPECT_TRUE(IsValid("$10"));
  EXPECT_TRUE(IsValid("$ABC"));

  trainer_spec.set_max_sentencepiece_length(4);
  EXPECT_TRUE(IsValid("1234"));
  EXPECT_FALSE(IsValid("12345"));

  trainer_spec.set_max_sentencepiece_length(10);
  trainer_spec.set_split_by_unicode_script(true);
  trainer_spec.set_split_by_number(false);
  EXPECT_TRUE(IsValid("F1"));
  EXPECT_TRUE(IsValid("11"));
  EXPECT_TRUE(IsValid("1F"));
  EXPECT_TRUE(IsValid("ABC"));
  EXPECT_TRUE(IsValid("1A2"));
  EXPECT_TRUE(IsValid("1a234abc"));
  EXPECT_FALSE(IsValid("9Aあ"));
  EXPECT_TRUE(IsValid("9あい0A"));

  trainer_spec.set_split_by_whitespace(true);
  trainer_spec.set_treat_whitespace_as_suffix(true);
  EXPECT_TRUE(IsValid(WS));
  EXPECT_FALSE(IsValid(WS "a"));
  EXPECT_TRUE(IsValid("a" WS));
  EXPECT_FALSE(IsValid(WS "a" WS));
  EXPECT_FALSE(IsValid("a" WS "b"));
  EXPECT_FALSE(IsValid(WS "a" WS "b"));
  EXPECT_FALSE(IsValid("a" WS "b" WS));

  trainer_spec.set_split_by_whitespace(false);
  EXPECT_TRUE(IsValid(WS));
  EXPECT_FALSE(IsValid(WS "a"));
  EXPECT_TRUE(IsValid("a" WS));
  EXPECT_FALSE(IsValid(WS "a" WS));
  EXPECT_TRUE(IsValid("a" WS "b"));
  EXPECT_FALSE(IsValid(WS "a" WS "b"));
  EXPECT_TRUE(IsValid("a" WS "b" WS));
}

TEST(TrainerInterfaceTest, OverrideSpecialPiecesTest) {
  TrainerSpec base_trainer_spec;
  NormalizerSpec normalizer_spec;
  base_trainer_spec.set_model_prefix("model");
  base_trainer_spec.add_input("input");

  auto trainer_spec = base_trainer_spec;

  // Check default values.
  EXPECT_EQ(0, trainer_spec.unk_id());
  EXPECT_EQ(1, trainer_spec.bos_id());
  EXPECT_EQ(2, trainer_spec.eos_id());
  EXPECT_EQ(-1, trainer_spec.pad_id());

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(0);
    trainer_spec.set_bos_id(1);
    trainer_spec.set_eos_id(2);
    trainer_spec.set_pad_id(3);

    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_EQ(4, trainer.meta_pieces_.size());
    EXPECT_EQ("<unk>", trainer.meta_pieces_[0].first);
    EXPECT_EQ("<s>", trainer.meta_pieces_[1].first);
    EXPECT_EQ("</s>", trainer.meta_pieces_[2].first);
    EXPECT_EQ("<pad>", trainer.meta_pieces_[3].first);
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(0);
    trainer_spec.set_bos_id(3);
    trainer_spec.set_eos_id(2);
    trainer_spec.set_pad_id(1);

    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_EQ(4, trainer.meta_pieces_.size());
    EXPECT_EQ("<unk>", trainer.meta_pieces_[0].first);
    EXPECT_EQ("<pad>", trainer.meta_pieces_[1].first);
    EXPECT_EQ("</s>", trainer.meta_pieces_[2].first);
    EXPECT_EQ("<s>", trainer.meta_pieces_[3].first);
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(0);
    trainer_spec.set_bos_id(-1);
    trainer_spec.set_eos_id(1);
    trainer_spec.set_pad_id(-1);

    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_EQ(2, trainer.meta_pieces_.size());
    EXPECT_EQ("<unk>", trainer.meta_pieces_[0].first);
    EXPECT_EQ("</s>", trainer.meta_pieces_[1].first);
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(0);
    trainer_spec.set_bos_id(-1);
    trainer_spec.set_eos_id(-1);
    trainer_spec.set_pad_id(-1);

    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_EQ(1, trainer.meta_pieces_.size());
    EXPECT_EQ("<unk>", trainer.meta_pieces_[0].first);
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(0);
    trainer_spec.set_bos_id(1);
    trainer_spec.set_eos_id(2);
    trainer_spec.set_pad_id(-1);

    trainer_spec.add_control_symbols("<c1>");
    trainer_spec.add_control_symbols("<c2>");
    trainer_spec.add_user_defined_symbols("<u1>");
    trainer_spec.add_user_defined_symbols("<u2>");

    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_EQ(7, trainer.meta_pieces_.size());
    EXPECT_EQ("<unk>", trainer.meta_pieces_[0].first);
    EXPECT_EQ("<s>", trainer.meta_pieces_[1].first);
    EXPECT_EQ("</s>", trainer.meta_pieces_[2].first);
    EXPECT_EQ("<c1>", trainer.meta_pieces_[3].first);
    EXPECT_EQ("<c2>", trainer.meta_pieces_[4].first);
    EXPECT_EQ("<u1>", trainer.meta_pieces_[5].first);
    EXPECT_EQ("<u2>", trainer.meta_pieces_[6].first);
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(0);
    trainer_spec.set_bos_id(-1);
    trainer_spec.set_eos_id(2);
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_TRUE(trainer.status().ok());
  }

  {
    // UNK is not defined.
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(-1);
    trainer_spec.set_bos_id(0);
    trainer_spec.set_eos_id(1);
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }

  {
    // UNK is out-of-range.
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(640000);
    trainer_spec.set_bos_id(0);
    trainer_spec.set_eos_id(1);
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_vocab_size(32000);
    trainer_spec.set_unk_id(32000 - 1);
    trainer_spec.set_bos_id(32000 - 100);
    trainer_spec.set_eos_id(32000 - 200);
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_TRUE(trainer.status().ok());
  }

  {
    // Cannot assign <unk> as control symbol.
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(0);
    trainer_spec.set_bos_id(1);
    trainer_spec.set_eos_id(2);
    trainer_spec.add_control_symbols("<unk>");
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }

  {
    // Dup.
    auto trainer_spec = base_trainer_spec;
    trainer_spec.add_control_symbols("<foo>");
    trainer_spec.add_control_symbols("<foo>");
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(0);
    trainer_spec.set_bos_id(10);
    trainer_spec.set_eos_id(20);
    trainer_spec.set_pad_id(30);

    // <s>, <pad> are treated as USER_DEFIEND,
    // </s> is CONTROL.
    trainer_spec.add_user_defined_symbols("<s>");
    trainer_spec.add_user_defined_symbols("<pad>");
    trainer_spec.add_user_defined_symbols("foo");
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_TRUE(trainer.status().ok());

    EXPECT_EQ(5, trainer.meta_pieces_.size());
    EXPECT_EQ("<unk>", trainer.meta_pieces_[0].first);
    EXPECT_EQ("<s>", trainer.meta_pieces_[10].first);
    EXPECT_EQ("</s>", trainer.meta_pieces_[20].first);
    EXPECT_EQ("<pad>", trainer.meta_pieces_[30].first);
    EXPECT_EQ("foo", trainer.meta_pieces_[1].first);

    EXPECT_EQ(ModelProto::SentencePiece::UNKNOWN,
              trainer.meta_pieces_[0].second);
    EXPECT_EQ(ModelProto::SentencePiece::USER_DEFINED,
              trainer.meta_pieces_[10].second);
    EXPECT_EQ(ModelProto::SentencePiece::CONTROL,
              trainer.meta_pieces_[20].second);
    EXPECT_EQ(ModelProto::SentencePiece::USER_DEFINED,
              trainer.meta_pieces_[30].second);
    EXPECT_EQ(ModelProto::SentencePiece::USER_DEFINED,
              trainer.meta_pieces_[1].second);
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_piece("__UNK__");
    trainer_spec.set_bos_piece("__BOS__");
    trainer_spec.set_eos_piece("__EOS__");
    trainer_spec.set_pad_piece("__PAD__");
    trainer_spec.set_pad_id(3);
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_TRUE(trainer.status().ok());
    EXPECT_EQ("__UNK__", trainer.meta_pieces_[0].first);
    EXPECT_EQ("__BOS__", trainer.meta_pieces_[1].first);
    EXPECT_EQ("__EOS__", trainer.meta_pieces_[2].first);
    EXPECT_EQ("__PAD__", trainer.meta_pieces_[3].first);
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_piece("__UNK__");
    trainer_spec.set_bos_piece("__UNK__");
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_piece("");
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }
}

TEST(TrainerInterfaceTest, SerializeTest) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  trainer_spec.set_model_prefix("model");
  trainer_spec.add_input("input");

  EXPECT_TRUE(trainer_spec.hard_vocab_limit());

  std::vector<std::pair<std::string, float>> final_pieces = {
      {"a", 0.1}, {"b", 0.2}, {"c", 0.3}};

  {
    trainer_spec.set_vocab_size(10);
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    trainer.final_pieces_ = final_pieces;
    ModelProto model_proto;
    EXPECT_FALSE(trainer.Serialize(&model_proto).ok());
  }

  {
    trainer_spec.set_vocab_size(10);
    trainer_spec.set_hard_vocab_limit(false);
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    trainer.final_pieces_ = final_pieces;
    ModelProto model_proto;
    EXPECT_TRUE(trainer.Serialize(&model_proto).ok());
    EXPECT_EQ(6, model_proto.trainer_spec().vocab_size());
    for (int i = 3; i < 6; ++i) {
      EXPECT_EQ(final_pieces[i - 3].first, model_proto.pieces(i).piece());
      EXPECT_EQ(final_pieces[i - 3].second, model_proto.pieces(i).score());
    }
  }

  {
    trainer_spec.set_vocab_size(10);
    trainer_spec.set_model_type(TrainerSpec::CHAR);
    trainer_spec.set_hard_vocab_limit(true);
    TrainerInterface trainer(trainer_spec, normalizer_spec);
    trainer.final_pieces_ = final_pieces;
    ModelProto model_proto;
    EXPECT_TRUE(trainer.Serialize(&model_proto).ok());
    EXPECT_EQ(6, model_proto.trainer_spec().vocab_size());
    for (int i = 3; i < 6; ++i) {
      EXPECT_EQ(final_pieces[i - 3].first, model_proto.pieces(i).piece());
      EXPECT_EQ(final_pieces[i - 3].second, model_proto.pieces(i).score());
    }
  }
}

}  // namespace sentencepiece
