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

#include "trainer_interface.h"

#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "filesystem.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {

// Space symbol
#define WS "\xe2\x96\x81"

// Converts the 1 unicode string to the code point.
static char32 ToChar32(absl::string_view str) {
  string_util::UnicodeText utext = string_util::UTF8ToUnicodeText(str);
  return !utext.empty() ? *utext.begin() : 0;
}

TEST(TrainerInterfaceTest, IsValidSentencePieceTest) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;
  trainer_spec.set_model_prefix("model");
  trainer_spec.add_input("input");

  // Calls the default method for better coverage.
  TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
  EXPECT_TRUE(trainer.Train().ok());

  auto IsValid = [&trainer_spec, &normalizer_spec,
                  &denormalizer_spec](const std::string& str) {
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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
  EXPECT_TRUE(IsValid("proteïni"));  // Combining Diaeresis should inherit
                                     // script from base character.
  EXPECT_TRUE(IsValid("ثَبَّتَ"));  // Arabic Fatha and Shadda should inherit script
                                // from base character.

  trainer_spec.set_split_by_whitespace(false);
  EXPECT_TRUE(IsValid(WS));
  EXPECT_TRUE(IsValid(WS WS WS "a"));
  EXPECT_TRUE(IsValid(WS "a"));
  EXPECT_FALSE(IsValid("a" WS));
  EXPECT_FALSE(IsValid(WS "a" WS));
  EXPECT_TRUE(IsValid("a" WS "b"));  // "a b" is a valid piece.
  EXPECT_TRUE(IsValid(WS "a" WS "b"));
  EXPECT_TRUE(IsValid(WS "a" WS "b" WS "c"));
  EXPECT_FALSE(IsValid("a" WS "b" WS));
  EXPECT_FALSE(IsValid(WS WS));
  EXPECT_FALSE(IsValid(WS WS WS));

  trainer_spec.set_allow_whitespace_only_pieces(true);
  EXPECT_TRUE(IsValid(WS));
  EXPECT_TRUE(IsValid(WS WS));
  EXPECT_TRUE(IsValid(WS WS WS));
  EXPECT_TRUE(IsValid(WS WS "a"));
  EXPECT_FALSE(IsValid("a" WS WS));  // suffix whitespace illegal without flag

  trainer_spec.set_allow_whitespace_only_pieces(false);
  trainer_spec.set_split_by_unicode_script(false);
  EXPECT_TRUE(IsValid("あいう"));
  EXPECT_TRUE(IsValid("グーグル"));
  EXPECT_TRUE(IsValid("食べる"));
  EXPECT_TRUE(IsValid("漢字ABC"));
  EXPECT_TRUE(IsValid("F1"));
  EXPECT_TRUE(IsValid("$10"));
  EXPECT_TRUE(IsValid("$ABC"));

  trainer_spec.set_split_by_unicode_script(true);
  trainer_spec.set_split_by_number(true);
  EXPECT_FALSE(IsValid("F1"));
  EXPECT_TRUE(IsValid("$10"));

  trainer_spec.set_split_by_unicode_script(true);
  trainer_spec.set_split_by_number(false);
  EXPECT_TRUE(IsValid("F1"));
  EXPECT_TRUE(IsValid("$10"));

  trainer_spec.set_split_by_unicode_script(false);
  trainer_spec.set_split_by_number(true);
  EXPECT_TRUE(IsValid("F1"));
  EXPECT_TRUE(IsValid("$10"));

  trainer_spec.set_split_by_unicode_script(false);
  trainer_spec.set_split_by_number(false);
  EXPECT_TRUE(IsValid("F1"));
  EXPECT_TRUE(IsValid("$10"));

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

  trainer_spec.set_allow_whitespace_only_pieces(true);
  EXPECT_TRUE(IsValid(WS));
  EXPECT_TRUE(IsValid(WS WS));
  EXPECT_FALSE(IsValid(WS "a" WS));
  EXPECT_FALSE(IsValid("a" WS "b"));
  EXPECT_FALSE(IsValid(WS "a" WS "b"));
  EXPECT_FALSE(IsValid("a" WS "b" WS));

  trainer_spec.set_allow_whitespace_only_pieces(false);
  trainer_spec.set_split_by_whitespace(false);
  EXPECT_TRUE(IsValid(WS));
  EXPECT_FALSE(IsValid(WS "a"));
  EXPECT_TRUE(IsValid("a" WS));
  EXPECT_FALSE(IsValid(WS "a" WS));
  EXPECT_TRUE(IsValid("a" WS "b"));
  EXPECT_FALSE(IsValid(WS "a" WS "b"));
  EXPECT_TRUE(IsValid("a" WS "b" WS));

  trainer_spec.set_split_digits(false);
  EXPECT_TRUE(IsValid("1"));
  EXPECT_TRUE(IsValid("59"));
  EXPECT_TRUE(IsValid("2007"));
  EXPECT_TRUE(IsValid("x1"));
  EXPECT_TRUE(IsValid("2x"));

  trainer_spec.set_split_digits(true);
  EXPECT_TRUE(IsValid("1"));
  EXPECT_FALSE(IsValid("59"));
  EXPECT_FALSE(IsValid("2007"));
  EXPECT_FALSE(IsValid("x1"));
  EXPECT_FALSE(IsValid("2x"));
  // Fullwidth digits.
  EXPECT_TRUE(IsValid("１"));
  EXPECT_FALSE(IsValid("５９"));
  EXPECT_FALSE(IsValid("２００７"));
  EXPECT_FALSE(IsValid("＊１"));
  EXPECT_FALSE(IsValid("２＊"));
}

TEST(TrainerInterfaceTest, OverrideSpecialPiecesTest) {
  TrainerSpec base_trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;
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

    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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

    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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

    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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

    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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

    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_TRUE(trainer.status().ok());
  }

  {
    // UNK is not defined.
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(-1);
    trainer_spec.set_bos_id(0);
    trainer_spec.set_eos_id(1);
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }

  {
    // UNK is out-of-range.
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(640000);
    trainer_spec.set_bos_id(0);
    trainer_spec.set_eos_id(1);
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_vocab_size(32000);
    trainer_spec.set_unk_id(32000 - 1);
    trainer_spec.set_bos_id(32000 - 100);
    trainer_spec.set_eos_id(32000 - 200);
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_TRUE(trainer.status().ok());
  }

  {
    // Cannot assign <unk> as control symbol.
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_id(0);
    trainer_spec.set_bos_id(1);
    trainer_spec.set_eos_id(2);
    trainer_spec.add_control_symbols("<unk>");
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }

  {
    // Dup.
    auto trainer_spec = base_trainer_spec;
    trainer_spec.add_control_symbols("<foo>");
    trainer_spec.add_control_symbols("<foo>");
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }

  {
    auto trainer_spec = base_trainer_spec;
    trainer_spec.set_unk_piece("");
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_FALSE(trainer.status().ok());
  }
}

TEST(TrainerInterfaceTest, BytePiecesTest) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;
  trainer_spec.set_model_prefix("model");
  trainer_spec.add_input("input");

  trainer_spec.add_control_symbols("<c1>");
  trainer_spec.add_control_symbols("<c2>");
  trainer_spec.add_user_defined_symbols("<u1>");
  trainer_spec.add_user_defined_symbols("<u2>");

  trainer_spec.set_byte_fallback(true);

  TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
  EXPECT_TRUE(trainer.status().ok());

  // Byte pieces come after control symbols and user-defined symbols.
  for (int i = 0; i < 256; ++i) {
    const auto& piece = trainer.meta_pieces_[i + 7];
    EXPECT_EQ(absl::StrFormat("<0x%02X>", i), piece.first);
    EXPECT_EQ(ModelProto::SentencePiece::BYTE, piece.second);
  }
}

TEST(TrainerInterfaceTest, SerializeTest) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;
  trainer_spec.set_model_prefix("model");
  trainer_spec.add_input("input");

  EXPECT_TRUE(trainer_spec.hard_vocab_limit());

  std::vector<std::pair<std::string, float>> final_pieces = {
      {"a", 0.1}, {"b", 0.2}, {"c", 0.3}};

  {
    trainer_spec.set_vocab_size(10);
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    trainer.final_pieces_ = final_pieces;
    ModelProto model_proto;
    EXPECT_FALSE(trainer.Serialize(&model_proto).ok());
  }

  {
    trainer_spec.set_vocab_size(10);
    trainer_spec.set_hard_vocab_limit(false);
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
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

TEST(TrainerInterfaceTest, CharactersTest) {
  const std::string input_file =
      util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "input");
  {
    auto output = filesystem::NewWritableFile(input_file);
    // Make a single line with 50 "a", 49 "あ", and 1 "b".
    std::string line;
    for (int i = 0; i < 100; i++) {
      if (i < 50) {
        line += "a";
      } else if (i < 99) {
        line += "あ";
      } else {
        line += "b";
      }
    }
    line += "\n";
    output->WriteLine(line);
  }
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;
  trainer_spec.add_input(input_file);
  trainer_spec.set_model_prefix("model");
  trainer_spec.set_character_coverage(0.98);

  using E = absl::flat_hash_map<char32, int64>;
  {
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_OK(trainer.LoadSentences());
    // Because --character_coverage=0.98, "a" and "あ" are chosen, but "b" is
    // dropped.
    EXPECT_EQ(trainer.required_chars_,
              E({{ToChar32("a"), 50}, {ToChar32("あ"), 49}}));
  }
  {
    trainer_spec.set_required_chars("漢字");
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_OK(trainer.LoadSentences());
    // 漢 and 字 do not occur in the line, but they are added.
    EXPECT_EQ(trainer.required_chars_, E({{ToChar32("a"), 50},
                                          {ToChar32("あ"), 49},
                                          {ToChar32("漢"), 0},
                                          {ToChar32("字"), 0}}));
  }
  {
    trainer_spec.set_required_chars("aあ");
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_OK(trainer.LoadSentences());
    // Adding characters that frequently occur do not change the result.
    EXPECT_EQ(trainer.required_chars_,
              E({{ToChar32("a"), 50}, {ToChar32("あ"), 49}}));
  }
  {
    trainer_spec.set_required_chars("b");
    TrainerInterface trainer(trainer_spec, normalizer_spec, denormalizer_spec);
    EXPECT_OK(trainer.LoadSentences());
    // "b" is added with the correct frequency.
    EXPECT_EQ(
        trainer.required_chars_,
        E({{ToChar32("a"), 50}, {ToChar32("あ"), 49}, {ToChar32("b"), 1}}));
  }
}

TEST(TrainerInterfaceTest, MultiFileSentenceIteratorTest) {
  std::vector<std::string> files;
  std::vector<std::string> expected;
  for (int i = 0; i < 10; ++i) {
    const std::string file = util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir),
                                            absl::StrCat("input", i));
    auto output = filesystem::NewWritableFile(file);
    int num_line = (rand() % 100) + 1;
    for (int n = 0; n < num_line; ++n) {
      const auto value = absl::StrCat(rand());
      expected.emplace_back(value);
      output->WriteLine(value);
    }
    files.push_back(file);
  }

  std::vector<std::string> results;
  MultiFileSentenceIterator it(files);
  for (; !it.done(); it.Next()) {
    results.emplace_back(it.value());
  }
  EXPECT_OK(it.status());
  EXPECT_EQ(expected, results);
}

TEST(TrainerInterfaceTest, MultiFileSentenceIteratorErrorTest) {
  std::vector<std::string> files;
  for (int i = 0; i < 10; ++i) {
    const std::string file = util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir),
                                            absl::StrCat("input_not_exist", i));
    files.push_back(file);
  }

  MultiFileSentenceIterator it(files);
  EXPECT_TRUE(it.done());  // no files can be loaded.
  EXPECT_FALSE(it.status().ok());
}

}  // namespace sentencepiece
