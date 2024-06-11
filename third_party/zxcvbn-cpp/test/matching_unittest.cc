// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zxcvbn-cpp/native-src/zxcvbn/matching.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/adjacency_graphs.hpp"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/common.hpp"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/frequency_lists.hpp"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace zxcvbn {

namespace {

struct Variation {
  std::string password;
  idx_t i;
  idx_t j;
};

// takes a pattern and list of prefixes/suffixes
// returns a bunch of variants of that pattern embedded
// with each possible prefix/suffix combination, including no prefix/suffix
// returns a list of triplets [variant, i, j] where [i,j] is the start/end of
// the pattern, inclusive
std::vector<Variation> gen_pws(const std::string& pattern,
                               std::vector<std::string> prefixes,
                               std::vector<std::string> suffixes) {
  if (std::find(prefixes.begin(), prefixes.end(), "") == prefixes.end())
    prefixes.insert(prefixes.begin(), "");

  if (std::find(suffixes.begin(), suffixes.end(), "") == suffixes.end())
    suffixes.insert(suffixes.begin(), "");

  std::vector<Variation> result;
  for (const auto& prefix : prefixes) {
    for (const auto& suffix : suffixes) {
      result.push_back({prefix + pattern + suffix, prefix.size(),
                        prefix.size() + pattern.size() - 1});
    }
  }

  return result;
}

struct ExpectedDictionaryMatch {
  idx_t i;
  idx_t j;
  std::string token;

  std::string matched_word;
  rank_t rank;
  bool l33t;
  bool reversed;
  std::unordered_map<std::string, std::string> sub;
};

bool operator==(const Match& lhs, const ExpectedDictionaryMatch& rhs) {
  return lhs.i == rhs.i && lhs.j == rhs.j && lhs.token == rhs.token &&
         lhs.get_pattern() == MatchPattern::DICTIONARY &&
         lhs.get_dictionary().matched_word == rhs.matched_word &&
         lhs.get_dictionary().rank == rhs.rank &&
         lhs.get_dictionary().l33t == rhs.l33t &&
         lhs.get_dictionary().reversed == rhs.reversed &&
         lhs.get_dictionary().sub == rhs.sub;
}

struct ExpectedSpatialMatch {
  idx_t i;
  idx_t j;
  std::string token;

  GraphTag graph;
  unsigned turns;
  idx_t shifted_count;
};

bool operator==(const Match& lhs, const ExpectedSpatialMatch& rhs) {
  return lhs.i == rhs.i && lhs.j == rhs.j && lhs.token == rhs.token &&
         lhs.get_pattern() == MatchPattern::SPATIAL &&
         lhs.get_spatial().graph == rhs.graph &&
         lhs.get_spatial().turns == rhs.turns &&
         lhs.get_spatial().shifted_count == rhs.shifted_count;
}

struct ExpectedSequenceMatch {
  idx_t i;
  idx_t j;
  std::string token;

  SequenceTag sequence_tag;
  bool ascending;
};

bool operator==(const Match& lhs, const ExpectedSequenceMatch& rhs) {
  return lhs.i == rhs.i && lhs.j == rhs.j && lhs.token == rhs.token &&
         lhs.get_pattern() == MatchPattern::SEQUENCE &&
         lhs.get_sequence().sequence_tag == rhs.sequence_tag &&
         lhs.get_sequence().ascending == rhs.ascending;
}

struct ExpectedRepeatMatch {
  idx_t i;
  idx_t j;
  std::string token;

  std::string base_token;
};

bool operator==(const Match& lhs, const ExpectedRepeatMatch& rhs) {
  return lhs.i == rhs.i && lhs.j == rhs.j && lhs.token == rhs.token &&
         lhs.get_pattern() == MatchPattern::REPEAT &&
         lhs.get_repeat().base_token == rhs.base_token;
}

struct ExpectedRegexMatch {
  idx_t i;
  idx_t j;
  std::string token;

  RegexTag regex_tag;
};

bool operator==(const Match& lhs, const ExpectedRegexMatch& rhs) {
  return lhs.i == rhs.i && lhs.j == rhs.j && lhs.token == rhs.token &&
         lhs.get_pattern() == MatchPattern::REGEX &&
         lhs.get_regex().regex_tag == rhs.regex_tag;
}

struct ExpectedDateMatch {
  idx_t i;
  idx_t j;
  std::string token;

  std::string separator;
  unsigned year;
  unsigned month;
  unsigned day;
};

bool operator==(const Match& lhs, const ExpectedDateMatch& rhs) {
  return lhs.i == rhs.i && lhs.j == rhs.j && lhs.token == rhs.token &&
         lhs.get_pattern() == MatchPattern::DATE &&
         lhs.get_date().separator == rhs.separator &&
         lhs.get_date().year == rhs.year && lhs.get_date().month == rhs.month &&
         lhs.get_date().day == rhs.day;
}

}  // namespace

TEST(ZxcvbnTest, DictionaryMatching) {
  std::vector<std::vector<std::string_view>> test_dicts = {
      {"motherboard", "mother", "board", "abcd", "cdef"},
      {"z", "8", "99", "$", "asdf1234&*"},
  };

  {
    // matches words that contain other words
    std::string password = "motherboard";
    RankedDicts test_dicts_processed(test_dicts);
    std::vector<Match> matches =
        dictionary_match(password, test_dicts_processed);
    EXPECT_THAT(matches, ElementsAre(
                             ExpectedDictionaryMatch{
                                 .i = 0,
                                 .j = 5,
                                 .token = "mother",
                                 .matched_word = "mother",
                                 .rank = 2,
                             },
                             ExpectedDictionaryMatch{
                                 .i = 0,
                                 .j = 10,
                                 .token = "motherboard",
                                 .matched_word = "motherboard",
                                 .rank = 1,
                             },
                             ExpectedDictionaryMatch{
                                 .i = 6,
                                 .j = 10,
                                 .token = "board",
                                 .matched_word = "board",
                                 .rank = 3,
                             }));
  }

  {
    // matches multiple words when they overlap
    std::string password = "abcdef";
    std::vector<Match> matches =
        dictionary_match(password, RankedDicts(test_dicts));
    EXPECT_THAT(matches, ElementsAre(
                             ExpectedDictionaryMatch{
                                 .i = 0,
                                 .j = 3,
                                 .token = "abcd",
                                 .matched_word = "abcd",
                                 .rank = 4,
                             },
                             ExpectedDictionaryMatch{
                                 .i = 2,
                                 .j = 5,
                                 .token = "cdef",
                                 .matched_word = "cdef",
                                 .rank = 5,
                             }));
  }

  {
    // ignores uppercasing
    std::string password = "BoaRdZ";
    std::vector<Match> matches =
        dictionary_match(password, RankedDicts(test_dicts));
    EXPECT_THAT(matches, ElementsAre(
                             ExpectedDictionaryMatch{
                                 .i = 0,
                                 .j = 4,
                                 .token = "BoaRd",
                                 .matched_word = "board",
                                 .rank = 3,
                             },
                             ExpectedDictionaryMatch{
                                 .i = 5,
                                 .j = 5,
                                 .token = "Z",
                                 .matched_word = "z",
                                 .rank = 1,
                             }));
  }

  {
    // identifies words surrounded by non-words
    std::string word = "asdf1234&*";
    for (const auto& variation : gen_pws(word, {"q", "%%"}, {"%", "qq"})) {
      std::vector<Match> matches =
          dictionary_match(variation.password, RankedDicts(test_dicts));
      EXPECT_THAT(matches, ElementsAre(ExpectedDictionaryMatch{
                               .i = variation.i,
                               .j = variation.j,
                               .token = word,
                               .matched_word = word,
                               .rank = 5,
                           }));
    }
  }

  {
    // matches against all words in provided dictionaries
    for (const auto& test_dict : test_dicts) {
      rank_t expected_rank = 0;
      for (std::string_view ranked_word : test_dict) {
        expected_rank++;
        // skip words that contain others
        if (ranked_word == "motherboard")
          continue;

        std::vector<Match> matches =
            dictionary_match(std::string(ranked_word), RankedDicts(test_dicts));
        EXPECT_THAT(matches, ElementsAre(ExpectedDictionaryMatch{
                                 .i = 0,
                                 .j = ranked_word.size() - 1,
                                 .token = std::string(ranked_word),
                                 .matched_word = std::string(ranked_word),
                                 .rank = expected_rank,
                             }));
      }
    }
  }

  {
    // default dictionaries
    SetRankedDicts(RankedDicts({{"wow"}}));
    std::vector<Match> matches =
        dictionary_match("wow", default_ranked_dicts());
    EXPECT_THAT(matches, ElementsAre(ExpectedDictionaryMatch{
                             .i = 0,
                             .j = 2,
                             .token = "wow",
                             .matched_word = "wow",
                             .rank = 1,
                         }));
  }
}

TEST(ZxcvbnTest, ReverseDictionaryMatching) {
  std::vector<std::vector<std::string_view>> test_dicts = {
      {"123", "321", "456", "654"},
  };

  // matches against reversed words
  std::string password = "0123456789";
  std::vector<Match> matches =
      reverse_dictionary_match(password, RankedDicts(test_dicts));
  EXPECT_THAT(matches, ElementsAre(
                           ExpectedDictionaryMatch{
                               .i = 1,
                               .j = 3,
                               .token = "123",
                               .matched_word = "321",
                               .rank = 2,
                               .reversed = true,
                           },
                           ExpectedDictionaryMatch{
                               .i = 4,
                               .j = 6,
                               .token = "456",
                               .matched_word = "654",
                               .rank = 4,
                               .reversed = true,
                           }));
}

TEST(ZxcvbnTest, L33tMatching) {
  std::vector<std::pair<std::string, std::vector<std::string>>> test_table = {
      {"a", {"4", "@"}},
      {"c", {"(", "{", "[", "<"}},
      {"g", {"6", "9"}},
      {"o", {"0"}},
  };

  {
    // reduces l33t table to only the substitutions that a password might be
    // employing
    struct {
      std::string pw;
      std::unordered_map<std::string, std::vector<std::string>> expected;
    } tests[] = {
        {"", {}},
        {"abcdefgo123578!#$&*)]}>", {}},
        {"a", {}},
        {"4", {{"a", {"4"}}}},
        {"4@", {{"a", {"4", "@"}}}},
        {"4({60",
         {{"a", {"4"}}, {"c", {"(", "{"}}, {"g", {"6"}}, {"o", {"0"}}}},
    };

    for (const auto& test : tests) {
      EXPECT_EQ(relevant_l33t_subtable(test.pw, test_table), test.expected);
    }
  }

  {
    // enumerates the different sets of l33t substitutions a password might be
    // using
    struct {
      std::unordered_map<std::string, std::vector<std::string>> table;
      std::vector<std::unordered_map<std::string, std::string>> subs;
    } tests[] = {
        {{}, {{}}},
        {{{"a", {"@"}}}, {{{"@", "a"}}}},
        {{{"a", {"@", "4"}}}, {{{"@", "a"}}, {{"4", "a"}}}},
        {{{"a", {"@", "4"}}, {"c", {"("}}},
         {{{"@", "a"}, {"(", "c"}}, {{"4", "a"}, {"(", "c"}}}},
    };

    for (const auto& test : tests) {
      EXPECT_EQ(enumerate_l33t_subs(test.table), test.subs);
    }
  }

  {
    std::vector<std::vector<std::string_view>> dicts = {
        {"aac", "password", "paassword", "asdf0"},
        {"cgo"},
    };

    auto lm = [&](const std::string& password) {
      return l33t_match(password, RankedDicts(dicts), test_table);
    };

    // doesn't match ""
    EXPECT_THAT(lm(""), IsEmpty());

    // doesn't match pure dictionary words
    EXPECT_THAT(lm("password"), IsEmpty());

    // matches against common l33t substitutions
    struct {
      std::string password;
      std::string pattern;
      std::string word;
      rank_t rank;
      idx_t i;
      idx_t j;
      std::unordered_map<std::string, std::string> sub;
    } tests[] = {
        {"p4ssword", "p4ssword", "password", 2, 0, 7, {{"4", "a"}}},
        {"p@ssw0rd", "p@ssw0rd", "password", 2, 0, 7, {{"@", "a"}, {"0", "o"}}},
        {"aSdfO{G0asDfO", "{G0", "cgo", 1, 5, 7, {{"{", "c"}, {"0", "o"}}},
    };

    for (const auto& test : tests) {
      EXPECT_THAT(lm(test.password), ElementsAre(ExpectedDictionaryMatch{
                                         .i = test.i,
                                         .j = test.j,
                                         .token = test.pattern,
                                         .matched_word = test.word,
                                         .rank = test.rank,
                                         .l33t = true,
                                         .sub = test.sub,
                                     }));
    }

    // matches against overlapping l33t patterns
    EXPECT_THAT(lm("@a(go{G0"), ElementsAre(
                                    ExpectedDictionaryMatch{
                                        .i = 0,
                                        .j = 2,
                                        .token = "@a(",
                                        .matched_word = "aac",
                                        .rank = 1,
                                        .l33t = true,
                                        .sub = {{"@", "a"}, {"(", "c"}},
                                    },
                                    ExpectedDictionaryMatch{
                                        .i = 2,
                                        .j = 4,
                                        .token = "(go",
                                        .matched_word = "cgo",
                                        .rank = 1,
                                        .l33t = true,
                                        .sub = {{"(", "c"}},
                                    },
                                    ExpectedDictionaryMatch{
                                        .i = 5,
                                        .j = 7,
                                        .token = "{G0",
                                        .matched_word = "cgo",
                                        .rank = 1,
                                        .l33t = true,
                                        .sub = {{"{", "c"}, {"0", "o"}},
                                    }));

    // doesn't match when multiple l33t substitutions are needed for the same
    // letter
    EXPECT_THAT(lm("p4@ssword"), IsEmpty());

    // doesn't match single-character l33ted words
    EXPECT_THAT(l33t_match("4 1 @", {}, {}), IsEmpty());

    // doesn't match with subsets of possible l33t substitutions
    EXPECT_THAT(lm("4sdf0"), IsEmpty());
  }
}

TEST(ZxcvbnTest, SpatialMatching) {
  // doesn't match 1- and 2-character spatial patterns
  for (const std::string& password : {"", "/", "qw", "*/"}) {
    EXPECT_THAT(spatial_match(password, {}), IsEmpty());
  }

  // for testing, make a subgraph that contains a single keyboard
  Graphs test_graphs = {{GraphTag::QWERTY, graphs().at(GraphTag::QWERTY)}};
  std::string pattern = "6tfGHJ";
  std::vector<Match> matches =
      spatial_match("rz!" + pattern + "%z", test_graphs);
  EXPECT_THAT(matches, ElementsAre(ExpectedSpatialMatch{
                           .i = 3,
                           .j = 3 + pattern.size() - 1,
                           .token = pattern,
                           .graph = GraphTag::QWERTY,
                           .turns = 2,
                           .shifted_count = 3,
                       }));

  struct {
    std::string pattern;
    GraphTag keyboard;
    unsigned turns;
    idx_t shifts;
  } tests[] = {
      {"12345", GraphTag::QWERTY, 1, 0},
      {"@WSX", GraphTag::QWERTY, 1, 4},
      {"6tfGHJ", GraphTag::QWERTY, 2, 3},
      {"hGFd", GraphTag::QWERTY, 1, 2},
      {"/;p09876yhn", GraphTag::QWERTY, 3, 0},
      {"Xdr%", GraphTag::QWERTY, 1, 2},
      {"159-", GraphTag::KEYPAD, 1, 0},
      {"*84", GraphTag::KEYPAD, 1, 0},
      {"/8520", GraphTag::KEYPAD, 1, 0},
      {"369", GraphTag::KEYPAD, 1, 0},
      {"/963.", GraphTag::MAC_KEYPAD, 1, 0},
      {"*-632.0214", GraphTag::MAC_KEYPAD, 9, 0},
      {"aoEP%yIxkjq:", GraphTag::DVORAK, 4, 5},
      {";qoaOQ:Aoq;a", GraphTag::DVORAK, 11, 4},
  };

  for (const auto& test : tests) {
    Graphs test_graphs = {{test.keyboard, graphs().at(test.keyboard)}};
    std::vector<Match> matches = spatial_match(test.pattern, test_graphs);
    EXPECT_THAT(matches, ElementsAre(ExpectedSpatialMatch{
                             .i = 0,
                             .j = test.pattern.size() - 1,
                             .token = test.pattern,
                             .graph = test.keyboard,
                             .turns = test.turns,
                             .shifted_count = test.shifts,
                         }));
  }
}

TEST(ZxcvbnTest, SequenceMatching) {
  // doesn't match 0- and 1-character sequences
  for (const std::string& password : {"", "a", "1"}) {
    EXPECT_THAT(sequence_match(password), IsEmpty());
  }

  {
    // matches overlapping patterns
    std::vector<Match> matches = sequence_match("abcbabc");
    EXPECT_THAT(matches, ElementsAre(
                             ExpectedSequenceMatch{
                                 .i = 0,
                                 .j = 2,
                                 .token = "abc",
                                 .sequence_tag = SequenceTag::kLower,
                                 .ascending = true,
                             },
                             ExpectedSequenceMatch{
                                 .i = 2,
                                 .j = 4,
                                 .token = "cba",
                                 .sequence_tag = SequenceTag::kLower,
                                 .ascending = false,
                             },
                             ExpectedSequenceMatch{
                                 .i = 4,
                                 .j = 6,
                                 .token = "abc",
                                 .sequence_tag = SequenceTag::kLower,
                                 .ascending = true,
                             }));
  }

  {
    // matches embedded sequence patterns
    std::string pattern = "jihg";
    for (const auto& variation : gen_pws(pattern, {"!", "22"}, {"!", "22"})) {
      std::vector<Match> matches = sequence_match(variation.password);
      EXPECT_THAT(matches, ElementsAre(ExpectedSequenceMatch{
                               .i = variation.i,
                               .j = variation.j,
                               .token = pattern,
                               .sequence_tag = SequenceTag::kLower,
                               .ascending = false,
                           }));
    }
  }

  {
    struct {
      std::string pattern;
      SequenceTag sequence_tag;
      bool ascending;
    } tests[] = {
        {"ABC", SequenceTag::kUpper, true},
        {"CBA", SequenceTag::kUpper, false},
        {"PQR", SequenceTag::kUpper, true},
        {"RQP", SequenceTag::kUpper, false},
        {"XYZ", SequenceTag::kUpper, true},
        {"ZYX", SequenceTag::kUpper, false},
        {"abcd", SequenceTag::kLower, true},
        {"dcba", SequenceTag::kLower, false},
        {"jihg", SequenceTag::kLower, false},
        {"wxyz", SequenceTag::kLower, true},
        {"zxvt", SequenceTag::kLower, false},
        {"0369", SequenceTag::kDigits, true},
        {"97531", SequenceTag::kDigits, false},
    };

    for (const auto& test : tests) {
      std::vector<Match> matches = sequence_match(test.pattern);
      EXPECT_THAT(matches, ElementsAre(ExpectedSequenceMatch{
                               .i = 0,
                               .j = test.pattern.size() - 1,
                               .token = test.pattern,
                               .sequence_tag = test.sequence_tag,
                               .ascending = test.ascending,
                           }));
    }
  }
}

TEST(ZxcvbnTest, RepeatMatching) {
  // doesn't match 0- and 1-character repeat patterns
  for (const std::string& password : {"", "#"}) {
    EXPECT_THAT(repeat_match(password), IsEmpty());
  }

  {
    // matches embedded repeat patterns
    std::string pattern = "&&&&";
    for (const auto& variation : gen_pws(pattern, {"@", "y4@"}, {"u", "u%7"})) {
      std::vector<Match> matches = repeat_match(variation.password);
      EXPECT_THAT(matches, ElementsAre(ExpectedRepeatMatch{
                               .i = variation.i,
                               .j = variation.j,
                               .token = pattern,
                               .base_token = "&",
                           }));
    }
  }

  {
    // matches repeats with base character
    for (size_t length : {3, 12}) {
      for (char chr : {'a', 'Z', '4', '&'}) {
        std::string pattern(length, chr);
        std::vector<Match> matches = repeat_match(pattern);
        EXPECT_THAT(matches, ElementsAre(ExpectedRepeatMatch{
                                 .i = 0,
                                 .j = pattern.size() - 1,
                                 .token = pattern,
                                 .base_token = std::string(1, chr),
                             }));
      }
    }
  }

  {
    // matches multiple adjacent repeats
    std::vector<Match> matches = repeat_match("BBB1111aaaaa@@@@@@");
    EXPECT_THAT(matches, ElementsAre(
                             ExpectedRepeatMatch{
                                 .i = 0,
                                 .j = 2,
                                 .token = "BBB",
                                 .base_token = "B",
                             },
                             ExpectedRepeatMatch{
                                 .i = 3,
                                 .j = 6,
                                 .token = "1111",
                                 .base_token = "1",
                             },
                             ExpectedRepeatMatch{
                                 .i = 7,
                                 .j = 11,
                                 .token = "aaaaa",
                                 .base_token = "a",
                             },
                             ExpectedRepeatMatch{
                                 .i = 12,
                                 .j = 17,
                                 .token = "@@@@@@",
                                 .base_token = "@",
                             }));
  }

  {
    // matches multiple repeats with non-repeats in-between
    std::vector<Match> matches =
        repeat_match("2818BBBbzsdf1111@*&@!aaaaaEUDA@@@@@@1729");
    EXPECT_THAT(matches, ElementsAre(
                             ExpectedRepeatMatch{
                                 .i = 4,
                                 .j = 6,
                                 .token = "BBB",
                                 .base_token = "B",
                             },
                             ExpectedRepeatMatch{
                                 .i = 12,
                                 .j = 15,
                                 .token = "1111",
                                 .base_token = "1",
                             },
                             ExpectedRepeatMatch{
                                 .i = 21,
                                 .j = 25,
                                 .token = "aaaaa",
                                 .base_token = "a",
                             },
                             ExpectedRepeatMatch{
                                 .i = 30,
                                 .j = 35,
                                 .token = "@@@@@@",
                                 .base_token = "@",
                             }));
  }

  {
    // matches multi-character repeat pattern
    std::string pattern = "abab";
    std::vector<Match> matches = repeat_match(pattern);
    EXPECT_THAT(matches, ElementsAre(ExpectedRepeatMatch{
                             .i = 0,
                             .j = pattern.size() - 1,
                             .token = pattern,
                             .base_token = "ab",
                         }));
  }

  {
    // matches aabaab as a repeat instead of the aa prefix
    std::string pattern = "aabaab";
    std::vector<Match> matches = repeat_match(pattern);
    EXPECT_THAT(matches, ElementsAre(ExpectedRepeatMatch{
                             .i = 0,
                             .j = pattern.size() - 1,
                             .token = pattern,
                             .base_token = "aab",
                         }));
  }

  {
    // identifies ab as repeat string, even though abab is also repeated
    std::string pattern = "abababab";
    std::vector<Match> matches = repeat_match(pattern);
    EXPECT_THAT(matches, ElementsAre(ExpectedRepeatMatch{
                             .i = 0,
                             .j = pattern.size() - 1,
                             .token = pattern,
                             .base_token = "ab",
                         }));
  }

  {
    // identifies äö as repeat string, even though äöäö is also repeated.
    // verifies that match.i and match.j operate in code point counts, and not
    // in bytes.
    std::string pattern = "\u00E4\u00F6\u00E4\u00F6\u00E4\u00F6\u00E4\u00F6";
    std::vector<Match> matches = repeat_match(pattern);
    EXPECT_THAT(matches, ElementsAre(ExpectedRepeatMatch{
                             .i = 0,
                             .j = 7,
                             .token = pattern,
                             .base_token = "\u00E4\u00F6",
                         }));
  }
}

TEST(ZxcvbnTest, RegexMatching) {
  struct {
    std::string pattern;
    RegexTag regex_tag;
  } tests[] = {
      {"1922", RegexTag::RECENT_YEAR},
      {"2017", RegexTag::RECENT_YEAR},
  };

  for (const auto& test : tests) {
    std::vector<Match> matches = regex_match(test.pattern, REGEXEN());
    EXPECT_THAT(matches, ElementsAre(ExpectedRegexMatch{
                             .i = 0,
                             .j = test.pattern.size() - 1,
                             .token = test.pattern,
                             .regex_tag = test.regex_tag,
                         }));
  }
}

TEST(ZxcvbnTest, DateMatching) {
  // matches dates that use `sep` as a separator
  for (const std::string& sep : {"", " ", "-", "/", "\\", "_", "."}) {
    std::string password = "13" + sep + "2" + sep + "1921";
    std::vector<Match> matches = date_match(password);
    EXPECT_THAT(matches, ElementsAre(ExpectedDateMatch{
                             .i = 0,
                             .j = password.size() - 1,
                             .token = password,
                             .separator = sep,
                             .year = 1921,
                             .month = 2,
                             .day = 13,
                         }));
  }

  // matches dates with `order` format
  for (std::string order : {"mdy", "dmy", "ymd", "ydm"}) {
    order.replace(order.find('y'), 1, "88");
    order.replace(order.find('m'), 1, "8");
    order.replace(order.find('d'), 1, "8");

    std::vector<Match> matches = date_match(order);
    EXPECT_THAT(matches, ElementsAre(ExpectedDateMatch{
                             .i = 0,
                             .j = order.size() - 1,
                             .token = order,
                             .separator = "",
                             .year = 1988,
                             .month = 8,
                             .day = 8,
                         }));
  }

  {
    // matches the date with year closest to REFERENCE_YEAR when ambiguous
    std::string password = "111504";
    std::vector<Match> matches = date_match(password);
    EXPECT_THAT(matches, ElementsAre(ExpectedDateMatch{
                             .i = 0,
                             .j = password.size() - 1,
                             .token = password,
                             .separator = "",
                             .year = 2004,
                             .month = 11,
                             .day = 15,
                         }));
  }

  {
    struct {
      unsigned day;
      unsigned month;
      unsigned year;
    } tests[] = {
        {1, 1, 1999},
        {21, 8, 2000},
        {19, 12, 2005},
        {22, 11, 1551},
    };

    for (const auto& test : tests) {
      std::string password = std::to_string(test.year) +
                             std::to_string(test.month) +
                             std::to_string(test.day);
      std::vector<Match> matches = date_match(password);
      EXPECT_THAT(matches, ElementsAre(ExpectedDateMatch{
                               .i = 0,
                               .j = password.size() - 1,
                               .token = password,
                               .separator = "",
                               .year = test.year,
                               .month = test.month,
                               .day = test.day,
                           }));
    }

    for (const auto& test : tests) {
      std::string password = std::to_string(test.year) + "." +
                             std::to_string(test.month) + "." +
                             std::to_string(test.day);
      std::vector<Match> matches = date_match(password);
      EXPECT_THAT(matches, ElementsAre(ExpectedDateMatch{
                               .i = 0,
                               .j = password.size() - 1,
                               .token = password,
                               .separator = ".",
                               .year = test.year,
                               .month = test.month,
                               .day = test.day,
                           }));
    }
  }

  {
    // matches zero-padded dates
    std::string password = "02/02/02";
    std::vector<Match> matches = date_match(password);
    EXPECT_THAT(matches, ElementsAre(ExpectedDateMatch{
                             .i = 0,
                             .j = password.size() - 1,
                             .token = password,
                             .separator = "/",
                             .year = 2002,
                             .month = 2,
                             .day = 2,
                         }));
  }

  {
    // matches embedded dates
    std::string pattern = "1/1/91";
    for (const auto& variation : gen_pws(pattern, {"a", "ab"}, {"!"})) {
      std::vector<Match> matches = date_match(variation.password);
      EXPECT_THAT(matches, ElementsAre(ExpectedDateMatch{
                               .i = variation.i,
                               .j = variation.j,
                               .token = pattern,
                               .separator = "/",
                               .year = 1991,
                               .month = 1,
                               .day = 1,
                           }));
    }
  }

  {
    // matches overlapping dates
    std::string password = "12/20/1991.12.20";
    std::vector<Match> matches = date_match(password);
    EXPECT_THAT(matches, ElementsAre(
                             ExpectedDateMatch{
                                 .i = 0,
                                 .j = 9,
                                 .token = "12/20/1991",
                                 .separator = "/",
                                 .year = 1991,
                                 .month = 12,
                                 .day = 20,
                             },
                             ExpectedDateMatch{
                                 .i = 6,
                                 .j = 15,
                                 .token = "1991.12.20",
                                 .separator = ".",
                                 .year = 1991,
                                 .month = 12,
                                 .day = 20,
                             }));
  }

  {
    // matches dates padded by non-ambiguous digits
    std::string password = "912/20/919";
    std::vector<Match> matches = date_match(password);
    EXPECT_THAT(matches, ElementsAre(ExpectedDateMatch{
                             .i = 1,
                             .j = 8,
                             .token = "12/20/91",
                             .separator = "/",
                             .year = 1991,
                             .month = 12,
                             .day = 20,
                         }));
  }
}

TEST(ZxcvbnTest, Omnimatch) {
  EXPECT_THAT(omnimatch(""), IsEmpty());

  SetRankedDicts(RankedDicts({{"rosebud", "maelstrom"}}));
  std::string password = "r0sebudmaelstrom11/20/91aaaa";
  std::vector<Match> matches = omnimatch(password);

  struct {
    MatchPattern pattern;
    idx_t i;
    idx_t j;
  } tests[] = {
      {MatchPattern::DICTIONARY, 0, 6},
      {MatchPattern::DICTIONARY, 7, 15},
      {MatchPattern::DATE, 16, 23},
      {MatchPattern::REPEAT, 24, 27},
  };

  for (const auto& test : tests) {
    bool included =
        std::any_of(matches.begin(), matches.end(), [&test](const auto& match) {
          return std::make_tuple(match.get_pattern(), match.i, match.j) ==
                 std::make_tuple(test.pattern, test.i, test.j);
        });
    EXPECT_TRUE(included);
  }
}

}  // namespace zxcvbn
