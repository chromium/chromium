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

#include "normalizer.h"

#include <vector>

#include "builder.h"
#include "sentencepiece_trainer.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {
namespace normalizer {
namespace {
// Space symbol
#define WS "\xe2\x96\x81"

// Replacement char
#define RC "\xEF\xBF\xBD"

NormalizerSpec MakeDefaultSpec() {
  return SentencePieceTrainer::GetNormalizerSpec("nmt_nfkc");
}
}  // namespace

TEST(NormalizerTest, NormalizeTest) {
  auto spec = MakeDefaultSpec();
  const Normalizer normalizer(spec);

  // Empty strings.
  EXPECT_EQ("", normalizer.Normalize(""));
  EXPECT_EQ("", normalizer.Normalize("      "));
  EXPECT_EQ("", normalizer.Normalize("　"));

  // Sentence with heading/tailing/redundant spaces.
  EXPECT_EQ(WS "ABC", normalizer.Normalize("ABC"));
  EXPECT_EQ(WS "ABC", normalizer.Normalize(" ABC "));
  EXPECT_EQ(WS "A" WS "B" WS "C", normalizer.Normalize(" A  B  C "));
  EXPECT_EQ(WS "ABC", normalizer.Normalize("   ABC   "));
  EXPECT_EQ(WS "ABC", normalizer.Normalize("   ＡＢＣ   "));
  EXPECT_EQ(WS "ABC", normalizer.Normalize("　　ABC"));
  EXPECT_EQ(WS "ABC", normalizer.Normalize("　　ABC　　"));

  // NFKC char to char normalization.
  EXPECT_EQ(WS "123", normalizer.Normalize("①②③"));

  // NFKC char to multi-char normalization.
  EXPECT_EQ(WS "株式会社", normalizer.Normalize("㍿"));

  // Half width katakana, character composition happens.
  EXPECT_EQ(WS "グーグル", normalizer.Normalize(" ｸﾞｰｸﾞﾙ "));

  EXPECT_EQ(WS "I" WS "saw" WS "a" WS "girl",
            normalizer.Normalize(" I  saw a　 　girl　　"));

  // Remove control chars.
  EXPECT_EQ("", normalizer.Normalize(string_util::UnicodeCharToUTF8(0x7F)));
  EXPECT_EQ("", normalizer.Normalize(string_util::UnicodeCharToUTF8(0x8F)));
  EXPECT_EQ("", normalizer.Normalize(string_util::UnicodeCharToUTF8(0x9F)));
  EXPECT_EQ("", normalizer.Normalize(string_util::UnicodeCharToUTF8(0x0B)));
  for (char32 c = 0x10; c <= 0x1F; ++c) {
    EXPECT_EQ("", normalizer.Normalize(string_util::UnicodeCharToUTF8(c)));
  }
}

TEST(NormalizerTest, NormalizeWithoutDummyPrefixTest) {
  auto spec = MakeDefaultSpec();
  spec.set_add_dummy_prefix(false);
  const Normalizer normalizer(spec);

  // Empty strings.
  EXPECT_EQ("", normalizer.Normalize(""));
  EXPECT_EQ("", normalizer.Normalize("      "));
  EXPECT_EQ("", normalizer.Normalize("　"));

  // Sentence with heading/tailing/redundant spaces.
  EXPECT_EQ("ABC", normalizer.Normalize("ABC"));
  EXPECT_EQ("ABC", normalizer.Normalize(" ABC "));
  EXPECT_EQ("A" WS "B" WS "C", normalizer.Normalize(" A  B  C "));
  EXPECT_EQ("ABC", normalizer.Normalize("   ABC   "));
  EXPECT_EQ("ABC", normalizer.Normalize("   ＡＢＣ   "));
  EXPECT_EQ("ABC", normalizer.Normalize("　　ABC"));
  EXPECT_EQ("ABC", normalizer.Normalize("　　ABC　　"));
}

TEST(NormalizerTest, NormalizeTreatWSAsSuffixTest) {
  auto spec = MakeDefaultSpec();
  TrainerSpec trainer_spec;
  trainer_spec.set_treat_whitespace_as_suffix(true);
  const Normalizer normalizer(spec, trainer_spec);

  EXPECT_EQ("", normalizer.Normalize(""));
  EXPECT_EQ("", normalizer.Normalize("      "));
  EXPECT_EQ("", normalizer.Normalize("　"));

  EXPECT_EQ("ABC" WS, normalizer.Normalize("ABC"));
  EXPECT_EQ("ABC" WS, normalizer.Normalize(" ABC "));
  EXPECT_EQ("A" WS "B" WS "C" WS, normalizer.Normalize(" A  B  C "));
  EXPECT_EQ("ABC" WS, normalizer.Normalize("   ABC   "));
}

TEST(NormalizerTest, NormalizeWithoutRemoveExtraWhitespacesTest) {
  auto spec = MakeDefaultSpec();
  spec.set_remove_extra_whitespaces(false);
  const Normalizer normalizer(spec);

  // Empty strings.
  EXPECT_EQ("", normalizer.Normalize(""));
  EXPECT_EQ(WS WS WS WS WS WS WS, normalizer.Normalize("      "));
  EXPECT_EQ(WS WS, normalizer.Normalize("　"));

  // Sentence with heading/tailing/redundant spaces.
  EXPECT_EQ(WS "ABC", normalizer.Normalize("ABC"));
  EXPECT_EQ(WS WS "ABC" WS, normalizer.Normalize(" ABC "));
  EXPECT_EQ(WS WS WS "A" WS WS "B" WS WS "C" WS WS,
            normalizer.Normalize("  A  B  C  "));
}

TEST(NormalizerTest, NormalizeWithoutEscapeWhitespacesTest) {
  auto spec = MakeDefaultSpec();
  spec.set_add_dummy_prefix(false);
  spec.set_remove_extra_whitespaces(true);
  spec.set_escape_whitespaces(false);
  const Normalizer normalizer(spec);

  // Empty strings.
  EXPECT_EQ("", normalizer.Normalize(""));
  EXPECT_EQ("", normalizer.Normalize("      "));
  EXPECT_EQ("", normalizer.Normalize("　"));

  // Sentence with heading/tailing/redundant spaces.
  EXPECT_EQ("ABC", normalizer.Normalize("ABC"));
  EXPECT_EQ("ABC", normalizer.Normalize(" ABC "));
  EXPECT_EQ("A B C", normalizer.Normalize("  A  B  C  "));
  EXPECT_EQ("A B C", normalizer.Normalize("A　 B　 C"));
}

TEST(NormalizeTest, NomalizeWithSpaceContainedRules) {
  Builder::CharsMap charsmap;

  auto AddRule = [&](const std::string &src, const std::string &trg) {
    Builder::Chars src_chars, trg_chars;
    for (const char32 c : string_util::UTF8ToUnicodeText(src)) {
      src_chars.push_back(c);
    }
    for (const char32 c : string_util::UTF8ToUnicodeText(trg)) {
      trg_chars.push_back(c);
    }
    charsmap[src_chars] = trg_chars;
  };

  // Adds rules containing whitespaes.
  AddRule("a", " A");
  AddRule("b", "B");
  AddRule("c", "D E");
  AddRule("d", " F G ");

  NormalizerSpec spec;
  EXPECT_TRUE(
      Builder::CompileCharsMap(charsmap, spec.mutable_precompiled_charsmap())
          .ok());

  // Test default behavior
  {
    const Normalizer normalizer(spec);
    EXPECT_EQ(WS "A", normalizer.Normalize("a"));
    EXPECT_EQ(WS "B" WS "A", normalizer.Normalize("ba"));
    EXPECT_EQ(WS "D" WS "E", normalizer.Normalize("c"));
    EXPECT_EQ(WS "F" WS "G" WS "A", normalizer.Normalize("da"));
    EXPECT_EQ(WS "A" WS "F" WS "G", normalizer.Normalize("ad"));
    EXPECT_EQ(WS "A" WS "F" WS "G" WS "B", normalizer.Normalize("adb"));
  }

  spec.set_escape_whitespaces(false);
  {
    spec.set_add_dummy_prefix(false);
    spec.set_remove_extra_whitespaces(true);

    const Normalizer normalizer(spec);
    EXPECT_EQ("A", normalizer.Normalize("a"));
    EXPECT_EQ("B A", normalizer.Normalize("ba"));
    EXPECT_EQ("D E", normalizer.Normalize("c"));
    EXPECT_EQ("F G A", normalizer.Normalize("da"));
    EXPECT_EQ("A F G", normalizer.Normalize("ad"));
    EXPECT_EQ("A F G B", normalizer.Normalize("adb"));
  }

  {
    spec.set_add_dummy_prefix(false);
    spec.set_remove_extra_whitespaces(false);

    const Normalizer normalizer(spec);
    EXPECT_EQ(" A", normalizer.Normalize("a"));
    EXPECT_EQ("B A", normalizer.Normalize("ba"));
    EXPECT_EQ("D E", normalizer.Normalize("c"));
    EXPECT_EQ(" F G  A", normalizer.Normalize("da"));
    EXPECT_EQ(" A F G ", normalizer.Normalize("ad"));
    EXPECT_EQ(" A F G B", normalizer.Normalize("adb"));
  }

  {
    spec.set_add_dummy_prefix(true);
    spec.set_remove_extra_whitespaces(true);

    const Normalizer normalizer(spec);
    EXPECT_EQ(" A", normalizer.Normalize("a"));
    EXPECT_EQ(" B A", normalizer.Normalize("ba"));
    EXPECT_EQ(" D E", normalizer.Normalize("c"));
    EXPECT_EQ(" F G A", normalizer.Normalize("da"));
    EXPECT_EQ(" A F G", normalizer.Normalize("ad"));
    EXPECT_EQ(" A F G B", normalizer.Normalize("adb"));
  }

  {
    spec.set_add_dummy_prefix(true);
    spec.set_remove_extra_whitespaces(false);

    const Normalizer normalizer(spec);
    EXPECT_EQ("  A", normalizer.Normalize("a"));
    EXPECT_EQ(" B A", normalizer.Normalize("ba"));
    EXPECT_EQ(" D E", normalizer.Normalize("c"));
    EXPECT_EQ("  F G  A", normalizer.Normalize("da"));
    EXPECT_EQ("  A F G ", normalizer.Normalize("ad"));
    EXPECT_EQ("  A F G B", normalizer.Normalize("adb"));
  }

  // Added several corner cases around spaces.
  struct SpacePattern {
    bool add_dummy_prefix;
    bool remove_extra_whitespaces;
    bool escape_whitespaces;
    const char *input;
    const char *expected;
  };

  constexpr SpacePattern kSpacePatternData[] = {
      {false, false, false, WS, WS},    {false, false, true, WS, WS},
      {false, true, false, WS, WS},     {false, true, true, WS, ""},
      {true, false, false, WS, " " WS}, {true, false, true, WS, WS WS},
      {true, true, false, WS, " " WS},  {true, true, true, WS, ""},
      {false, false, false, " ", " "},  {false, false, true, " ", WS},
      {false, true, false, " ", ""},    {false, true, true, " ", ""},
      {true, false, false, " ", "  "},  {true, false, true, " ", WS WS},
      {true, true, false, " ", ""},     {true, true, true, " ", ""}};

  for (const auto &c : kSpacePatternData) {
    spec.set_add_dummy_prefix(c.add_dummy_prefix);
    spec.set_remove_extra_whitespaces(c.remove_extra_whitespaces);
    spec.set_escape_whitespaces(c.escape_whitespaces);
    const Normalizer normalizer(spec);
    EXPECT_EQ(c.expected, normalizer.Normalize(c.input));
  }
}

TEST(NormalizerTest, NormalizeReplacementChar) {
  auto spec = MakeDefaultSpec();
  spec.set_add_dummy_prefix(false);
  const Normalizer normalizer(spec);
  EXPECT_EQ("abc" RC "xy", normalizer.Normalize("abc\x80xy"));
  EXPECT_EQ("abc" RC, normalizer.Normalize("abc\xc3"));
  EXPECT_EQ("ab" RC RC "xy", normalizer.Normalize("ab\xe3\x81xy"));
  EXPECT_EQ("a" RC RC RC "xy", normalizer.Normalize("a\xf3\x81\x81xy"));
  EXPECT_EQ("ab" RC RC "xy", normalizer.Normalize("ab\xc0\x82xy"));
}

TEST(NormalizerTest, NormalizeFullTest) {
  std::vector<size_t> n2i;
  std::string output;

  auto spec = MakeDefaultSpec();
  const Normalizer normalizer(spec);

  {
    const std::string input = "I saw a girl";
    EXPECT_TRUE(normalizer.Normalize(input, &output, &n2i).ok());
    EXPECT_EQ(WS "I" WS "saw" WS "a" WS "girl", output);
    const std::vector<size_t> expected = {0, 0, 0,       // WS (3byte)
                                          0,             // I
                                          1, 1, 1,       // WS
                                          2, 3, 4,       // saw
                                          5, 5, 5,       // WS
                                          6,             // a
                                          7, 7, 7,       // WS
                                          8, 9, 10, 11,  // girl
                                          12};
    EXPECT_EQ(expected, n2i);
  }

  {
    const std::string input = " I   saw a　 　girl　　";
    EXPECT_TRUE(normalizer.Normalize(input, &output, &n2i).ok());
    ABSL_LOG(INFO) << output;
    EXPECT_EQ(WS "I" WS "saw" WS "a" WS "girl", output);
    const std::vector<size_t> expected = {1,  1,  1,       // WS (3byte)
                                          1,               // I
                                          2,  2,  2,       // WS
                                          5,  6,  7,       // saw
                                          8,  8,  8,       // WS
                                          9,               // a
                                          10, 10, 10,      // WS
                                          17, 18, 19, 20,  // girl
                                          21};
    EXPECT_EQ(expected, n2i);
  }

  {
    const std::string input = " ｸﾞｰｸﾞﾙ ";  // halfwidth katakana
    EXPECT_TRUE(normalizer.Normalize(input, &output, &n2i).ok());
    EXPECT_EQ(WS "グーグル", output);
    const std::vector<size_t> expected = {1,  1,  1,   // WS (3byte)
                                          1,  1,  1,   // グ
                                          7,  7,  7,   // ー
                                          10, 10, 10,  // グ
                                          16, 16, 16,  // ル
                                          19};
    EXPECT_EQ(expected, n2i);
  }

  {
    const std::string input = "①②③";
    EXPECT_TRUE(normalizer.Normalize(input, &output, &n2i).ok());
    EXPECT_EQ(WS "123", output);
    const std::vector<size_t> expected = {0, 0, 0,  // WS (3byte)
                                          0,        // 1
                                          3,        // 2
                                          6,        // 3
                                          9};
    EXPECT_EQ(expected, n2i);
  }

  {
    const std::string input = "㍿";
    EXPECT_TRUE(normalizer.Normalize(input, &output, &n2i).ok());
    EXPECT_EQ(WS "株式会社", output);
    const std::vector<size_t> expected = {0, 0, 0,  // WS (3byte)
                                          0, 0, 0,  // 株
                                          0, 0, 0,  // 式
                                          0, 0, 0,  // 会
                                          0, 0, 0,  // 社
                                          3};
    // When "株式" is one piece, this has no alignment to the input.
    // Sentencepieces which includes the last character ("会社" or "社")
    // have the alignment to the input.
    EXPECT_EQ(expected, n2i);
  }
}

TEST(NormalizerTest, EncodeDecodePrecompiledCharsMapTest) {
  // some string of 256 4-byte units
  const std::string test_trie_blob =
      (" 000 001 002 003 004 005 006 007 008 009 010 011 012 013 014 015"
       " 016 017 018 019 020 021 022 023 024 025 026 027 028 029 030 031"
       " 032 033 034 035 036 037 038 039 040 041 042 043 044 045 046 047"
       " 048 049 050 051 052 053 054 055 056 057 058 059 060 061 062 063"
       " 064 065 066 067 068 069 070 071 072 073 074 075 076 077 078 079"
       " 080 081 082 083 084 085 086 087 088 089 090 091 092 093 094 095"
       " 096 097 098 099 100 101 102 103 104 105 106 107 108 109 110 111"
       " 112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127"
       " 128 129 130 131 132 133 134 135 136 137 138 139 140 141 142 143"
       " 144 145 146 147 148 149 150 151 152 153 154 155 156 157 158 159"
       " 160 161 162 163 164 165 166 167 168 169 170 171 172 173 174 175"
       " 176 177 178 179 180 181 182 183 184 185 186 187 188 189 190 191"
       " 192 193 194 195 196 197 198 199 200 201 202 203 204 205 206 207"
       " 208 209 210 211 212 213 214 215 216 217 218 219 220 221 222 223"
       " 224 225 226 227 228 229 230 231 232 233 234 235 236 237 238 239"
       " 240 241 242 243 244 245 246 247 248 249 250 251 252 253 254 255");
  // some string of arbitrary length
  std::string test_normalized_blob = "<some normalizer data>";
  test_normalized_blob += '\0';  // normalized blob must be null terminated.
  const std::string blob = Normalizer::EncodePrecompiledCharsMap(
      test_trie_blob, test_normalized_blob);
  std::string buf;
  absl::string_view trie_blob, normalized_blob;
  util::Status status = Normalizer::DecodePrecompiledCharsMap(
      blob, &trie_blob, &normalized_blob, &buf);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(test_trie_blob, trie_blob);
  EXPECT_EQ(test_normalized_blob, normalized_blob);
  EXPECT_FALSE(Normalizer::DecodePrecompiledCharsMap("", &trie_blob,
                                                     &normalized_blob, &buf)
                   .ok());
}

TEST(NormalizerTest, StatusTest) {
  NormalizerSpec spec;
  {
    const Normalizer normalizer(spec);
    EXPECT_TRUE(normalizer.status().ok());  // fallback to identity.
  }

  {
    spec.set_precompiled_charsmap("x");
    const Normalizer normalizer(spec);
    EXPECT_FALSE(normalizer.status().ok());
  }

  spec = MakeDefaultSpec();
  {
    const Normalizer normalizer(spec);
    EXPECT_TRUE(normalizer.status().ok());
  }
}

TEST(NormalizerTest, PrefixMatcherTest) {
  const PrefixMatcher matcher({"abc", "ab", "xy", "京都"});
  bool found;
  EXPECT_EQ(1, matcher.PrefixMatch("test", &found));
  EXPECT_FALSE(found);
  EXPECT_EQ(3, matcher.PrefixMatch("abcd", &found));
  EXPECT_TRUE(found);
  EXPECT_EQ(2, matcher.PrefixMatch("abxy", &found));
  EXPECT_TRUE(found);
  EXPECT_EQ(1, matcher.PrefixMatch("x", &found));
  EXPECT_FALSE(found);
  EXPECT_EQ(2, matcher.PrefixMatch("xyz", &found));
  EXPECT_TRUE(found);
  EXPECT_EQ(6, matcher.PrefixMatch("京都大学", &found));
  EXPECT_TRUE(found);
  EXPECT_EQ(3, matcher.PrefixMatch("東京大学", &found));
  EXPECT_FALSE(found);

  EXPECT_EQ("", matcher.GlobalReplace("", ""));
  EXPECT_EQ("", matcher.GlobalReplace("abc", ""));
  EXPECT_EQ("--de-pqr", matcher.GlobalReplace("xyabcdeabpqr", "-"));
}

TEST(NormalizerTest, PrefixMatcherWithEmptyTest) {
  const PrefixMatcher matcher({});
  bool found;
  EXPECT_EQ(1, matcher.PrefixMatch("test", &found));
  EXPECT_FALSE(found);
  EXPECT_EQ(1, matcher.PrefixMatch("abcd", &found));
  EXPECT_FALSE(found);
  EXPECT_EQ(1, matcher.PrefixMatch("abxy", &found));
  EXPECT_FALSE(found);
  EXPECT_EQ(1, matcher.PrefixMatch("x", &found));
  EXPECT_FALSE(found);
  EXPECT_EQ(1, matcher.PrefixMatch("xyz", &found));
  EXPECT_FALSE(found);
  EXPECT_EQ(3, matcher.PrefixMatch("京都大学", &found));
  EXPECT_FALSE(found);
  EXPECT_EQ(3, matcher.PrefixMatch("東京大学", &found));
  EXPECT_FALSE(found);

  EXPECT_EQ("", matcher.GlobalReplace("", ""));
  EXPECT_EQ("abc", matcher.GlobalReplace("abc", ""));
}

}  // namespace normalizer
}  // namespace sentencepiece
