// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zxcvbn-cpp/native-src/zxcvbn/scoring.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/adjacency_graphs.hpp"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/common.hpp"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/matching.hpp"

namespace zxcvbn {

namespace {

// Utility function expecting that `lhs` and `rhs` reference the same object.
template <typename T, typename U>
void EXPECT_REFEQ(const std::reference_wrapper<T>& lhs, const U& rhs) {
  EXPECT_EQ(std::addressof(lhs.get()), std::addressof(rhs));
}

}  // namespace

TEST(ZxcvbnTest, nCk) {
  EXPECT_EQ(nCk(0, 0), 1);
  EXPECT_EQ(nCk(1, 0), 1);
  EXPECT_EQ(nCk(5, 0), 1);
  EXPECT_EQ(nCk(0, 1), 0);
  EXPECT_EQ(nCk(0, 5), 0);
  EXPECT_EQ(nCk(2, 1), 2);
  EXPECT_EQ(nCk(4, 2), 6);
  EXPECT_EQ(nCk(33, 7), 4272048);

  const int64_t n = 49;
  const int64_t k = 12;
  EXPECT_EQ(nCk(n, k), nCk(n, n - k));
  EXPECT_EQ(nCk(n, k), nCk(n - 1, k - 1) + nCk(n - 1, k));
}

TEST(ZxcvbnTest, Search) {
  auto make_match = [](auto i, auto j, auto guesses) {
    Match m(i, j, "", DictionaryMatch());
    m.guesses = guesses;
    return m;
  };

  std::string password = "0123456789";
  bool exclude_additive = true;

  {
    // returns one bruteforce match given an empty match sequence
    std::vector<Match> matches;
    ScoringResult result = most_guessable_match_sequence(password, matches);
    EXPECT_EQ(result.sequence.size(), 1u);
    const Match& m0 = result.sequence[0];
    EXPECT_EQ(m0.get_pattern(), MatchPattern::BRUTEFORCE);
    EXPECT_EQ(m0.token, password);
    EXPECT_EQ(m0.i, 0u);
    EXPECT_EQ(m0.j, 9u);
  }

  {
    // returns match + bruteforce when match covers a prefix of password
    std::vector<Match> matches = {make_match(0, 5, 1)};
    const Match& m0 = matches[0];
    ScoringResult result =
        most_guessable_match_sequence(password, matches, exclude_additive);
    EXPECT_EQ(result.sequence.size(), 2u);
    EXPECT_REFEQ(result.sequence[0], m0);
    const Match& m1 = result.sequence[1];
    EXPECT_EQ(m1.get_pattern(), MatchPattern::BRUTEFORCE);
    EXPECT_EQ(m1.i, 6u);
    EXPECT_EQ(m1.j, 9u);
  }

  {
    // returns bruteforce + match when match covers a suffix
    std::vector<Match> matches = {make_match(3, 9, 1)};
    const Match& m1 = matches[0];
    ScoringResult result =
        most_guessable_match_sequence(password, matches, exclude_additive);
    EXPECT_EQ(result.sequence.size(), 2u);
    const Match& m0 = result.sequence[0];
    EXPECT_EQ(m0.get_pattern(), MatchPattern::BRUTEFORCE);
    EXPECT_EQ(m0.i, 0u);
    EXPECT_EQ(m0.j, 2u);
    EXPECT_REFEQ(result.sequence[1], m1);
  }

  {
    // returns bruteforce + match + bruteforce when match covers an infix
    std::vector<Match> matches = {make_match(1, 8, 1)};
    const Match& m1 = matches[0];
    ScoringResult result =
        most_guessable_match_sequence(password, matches, exclude_additive);
    EXPECT_EQ(result.sequence.size(), 3u);
    EXPECT_REFEQ(result.sequence[1], m1);
    const Match& m0 = result.sequence[0];
    const Match& m2 = result.sequence[2];
    EXPECT_EQ(m0.get_pattern(), MatchPattern::BRUTEFORCE);
    EXPECT_EQ(m2.get_pattern(), MatchPattern::BRUTEFORCE);
    EXPECT_EQ(m0.i, 0u);
    EXPECT_EQ(m0.j, 0u);
    EXPECT_EQ(m2.i, 9u);
    EXPECT_EQ(m2.j, 9u);
  }

  {
    // chooses lower-guesses match given two matches of the same span
    std::vector<Match> matches = {make_match(0, 9, 1), make_match(0, 9, 2)};
    Match& m0 = matches[0];
    const Match& m1 = matches[1];
    ScoringResult result =
        most_guessable_match_sequence(password, matches, exclude_additive);
    EXPECT_EQ(result.sequence.size(), 1u);
    EXPECT_REFEQ(result.sequence[0], m0);

    // make sure ordering doesn't matter
    m0.guesses = 3;
    result = most_guessable_match_sequence(password, matches, exclude_additive);
    EXPECT_EQ(result.sequence.size(), 1u);
    EXPECT_REFEQ(result.sequence[0], m1);
  }

  {
    // when m0 covers m1 and m2, choose [m0] when m0 < m1 * m2 * fact(2)
    std::vector<Match> matches = {make_match(0, 9, 3), make_match(0, 3, 2),
                                  make_match(4, 9, 1)};
    Match& m0 = matches[0];
    const Match& m1 = matches[1];
    const Match& m2 = matches[2];
    ScoringResult result =
        most_guessable_match_sequence(password, matches, exclude_additive);
    EXPECT_EQ(result.guesses, 3);
    EXPECT_EQ(result.sequence.size(), 1u);
    EXPECT_REFEQ(result.sequence[0], m0);

    // when m0 covers m1 and m2, choose [m1, m2] when m0 > m1 * m2 * fact(2)
    m0.guesses = 5;
    result = most_guessable_match_sequence(password, matches, exclude_additive);
    EXPECT_EQ(result.guesses, 4);
    EXPECT_EQ(result.sequence.size(), 2u);
    EXPECT_REFEQ(result.sequence[0], m1);
    EXPECT_REFEQ(result.sequence[1], m2);
  }
}

TEST(ZxcvbnTest, CalcGuesses) {
  {
    // estimate_guesses returns cached guesses when available
    Match match(0, 0, "", DictionaryMatch());
    match.guesses = 1;
    EXPECT_EQ(estimate_guesses(match, ""), 1);
  }

  {
    // estimate_guesses delegates based on pattern
    Match match(0, 3, "1977", DateMatch{.year = 1977, .month = 7, .day = 14});
    EXPECT_EQ(estimate_guesses(match, "1977"), date_guesses(match));
  }
}

TEST(ZxcvbnTest, RepeatGuesses) {
  struct {
    std::string token;
    std::string base_token;
    size_t repeat_count;
  } tests[] = {
      {"aa", "a", 2},
      {"999", "9", 3},
      {"$$$$", "$", 4},
      {"abab", "ab", 2},
      {"batterystaplebatterystaplebatterystaple", "batterystaple", 3},
  };

  for (const auto& test : tests) {
    std::vector<Match> omni_matches = omnimatch(test.base_token);
    double base_guesses =
        most_guessable_match_sequence(test.base_token, omni_matches).guesses;
    Match match(0, test.token.size() - 1, test.token,
                RepeatMatch{
                    .base_token = test.base_token,
                    .base_guesses = base_guesses,
                    .repeat_count = test.repeat_count,
                });

    double expected_guesses = base_guesses * test.repeat_count;
    EXPECT_EQ(repeat_guesses(match), expected_guesses);
  }
}

TEST(ZxcvbnTest, SequenceGuesses) {
  struct {
    std::string token;
    bool ascending;
    int guesses;
  } tests[] = {
      {"ab", true, 4 * 2},          // obvious start * len-2
      {"XYZ", true, 26 * 3},        // base26 * len-3
      {"4567", true, 10 * 4},       // base10 * len-4
      {"7654", false, 10 * 4 * 2},  // base10 * len 4 * descending
      {"ZYX", false, 4 * 3 * 2},    // obvious start * len-3 * descending
  };

  for (const auto& test : tests) {
    Match match(0, test.token.size() - 1, test.token,
                SequenceMatch{.ascending = test.ascending});
    EXPECT_EQ(sequence_guesses(match), test.guesses);
  }
}

TEST(ZxcvbnTest, RegexGuesses) {
  {
    // guesses of 26^7 for 7-char lowercase regex
    Match match(0, 6, "aizocdk",
                RegexMatch{
                    .regex_tag = RegexTag::ALPHA_LOWER,
                    .regex_match = {{"aizocdk"}, 0},
                });
    EXPECT_EQ(regex_guesses(match), pow(26., 7.));
  }

  {
    // guesses of 62^5 for 5-char alphanumeric regex
    Match match(0, 4, "ag7C8",
                RegexMatch{
                    .regex_tag = RegexTag::ALPHANUMERIC,
                    .regex_match = {{"ag7C8"}, 0},
                });
    EXPECT_EQ(regex_guesses(match), pow(62., 5.));
  }

  {
    // guesses of |year - REFERENCE_YEAR| for distant year matches
    Match match(0, 3, "1972",
                RegexMatch{
                    .regex_tag = RegexTag::RECENT_YEAR,
                    .regex_match = {{"1972"}, 0},
                });
    EXPECT_EQ(regex_guesses(match), REFERENCE_YEAR - 1972);
  }

  {
    // guesses of MIN_YEAR_SPACE for a year close to REFERENCE_YEAR
    Match match(0, 3, "2005",
                RegexMatch{
                    .regex_tag = RegexTag::RECENT_YEAR,
                    .regex_match = {{"2005"}, 0},
                });
    EXPECT_EQ(regex_guesses(match), MIN_YEAR_SPACE);
  }
}

TEST(ZxcvbnTest, DateGuesses) {
  {
    // guesses for 1123 is 365 * distance_from_ref_year
    Match match(0, 3, "1123",
                DateMatch{
                    .separator = "",
                    .year = 1923,
                    .month = 1,
                    .day = 1,
                    .has_full_year = false,
                });
    EXPECT_EQ(date_guesses(match),
              365 * (REFERENCE_YEAR - match.get_date().year));
  }

  {
    // recent years assume MIN_YEAR_SPACE.
    // extra guesses are added for separators and a 4-digit year.
    Match match(0, 7, "1/1/2010",
                DateMatch{
                    .separator = "/",
                    .year = 2010,
                    .month = 1,
                    .day = 1,
                    .has_full_year = true,
                });
    EXPECT_EQ(date_guesses(match), 365 * MIN_YEAR_SPACE * 4 * 2);
  }
}

TEST(ZxcvbnTest, SpatialGuesses) {
  // with no turns or shifts, guesses is starts * degree * (len-1)
  Match match(0, 5, "zxcvbn",
              SpatialMatch{
                  .graph = GraphTag::QWERTY,
                  .turns = 1,
                  .shifted_count = 0,
              });
  // - 1 term because: not counting spatial patterns of length 1 eg for
  // length==6, multiplier is 5 for needing to try len2,len3,..,len6
  guesses_t base_guesses = KEYBOARD_STARTING_POSITIONS *
                           KEYBOARD_AVERAGE_DEGREE * (match.token.size() - 1);
  EXPECT_EQ(spatial_guesses(match), base_guesses);

  // guesses is added for shifted keys, similar to capitals in dictionary
  // matching
  match.token = "ZxCvbn";
  match.get_spatial().shifted_count = 2;
  guesses_t shifted_guesses = base_guesses * (nCk(6, 2) + nCk(6, 1));
  EXPECT_EQ(spatial_guesses(match), shifted_guesses);

  // msg = "when everything is shifted, guesses are doubled"
  match.token = "ZXCVBN";
  match.get_spatial().shifted_count = 6;
  shifted_guesses = base_guesses * 2;
  EXPECT_EQ(spatial_guesses(match), shifted_guesses);

  // spatial guesses accounts for turn positions, directions and starting keys
  match = Match(0, 7, "zxcft6yh",
                SpatialMatch{
                    .graph = GraphTag::QWERTY,
                    .turns = 3,
                    .shifted_count = 0,
                });
  guesses_t guesses = 0;
  for (auto i = 2u; i <= match.token.size(); ++i) {
    for (auto j = 1u; j <= std::min(match.get_spatial().turns, i - 1); ++j) {
      guesses += nCk(i - 1, j - 1) * KEYBOARD_STARTING_POSITIONS *
                 pow(KEYBOARD_AVERAGE_DEGREE, j);
    }
  }

  EXPECT_EQ(spatial_guesses(match), guesses);
}

TEST(ZxcvbnTest, DictionaryGuesses) {
  {  // base guesses == the rank
    Match match(0, 4, "aaaaa", DictionaryMatch{.rank = 32});
    EXPECT_EQ(dictionary_guesses(match), 32);
  }  // namespace zxcvbn

  {
    // extra guesses are added for capitalization
    Match match(0, 5, "AAAaaa", DictionaryMatch{.rank = 32});
    EXPECT_EQ(dictionary_guesses(match), 32 * uppercase_variations(match));
  }

  {
    // guesses are doubled when word is reversed
    Match match(0, 2, "aaa", DictionaryMatch{.rank = 32, .reversed = true});
    EXPECT_EQ(dictionary_guesses(match), 32 * 2);
  }

  {
    // extra guesses are added for common l33t substitutions
    Match match(0, 5, "aaa@@@",
                DictionaryMatch{
                    .rank = 32,
                    .l33t = true,
                    .sub = {{"@", "a"}},
                });
    EXPECT_EQ(dictionary_guesses(match), 32 * l33t_variations(match));
  }

  {
    // extra guesses are added for both capitalization and common l33t
    // substitutions
    Match match(0, 5, "AaA@@@",
                DictionaryMatch{
                    .rank = 32,
                    .l33t = true,
                    .sub = {{"@", "a"}},
                });
    EXPECT_EQ(dictionary_guesses(match),
              32 * l33t_variations(match) * uppercase_variations(match));
  }
}

TEST(ZxcvbnTest, UppercaseVariants) {
  struct {
    std::string word;
    int variants;
  } tests[] = {
      {"", 1},
      {"a", 1},
      {"A", 2},
      {"abcdef", 1},
      {"Abcdef", 2},
      {"abcdeF", 2},
      {"ABCDEF", 2},
      {"aBcdef", nCk(6, 1)},
      {"aBcDef", nCk(6, 1) + nCk(6, 2)},
      {"ABCDEf", nCk(6, 1)},
      {"aBCDEf", nCk(6, 1) + nCk(6, 2)},
      {"ABCdef", nCk(6, 1) + nCk(6, 2) + nCk(6, 3)},
  };

  for (const auto& test : tests) {
    Match match(0, test.word.size() - 1, test.word, DictionaryMatch{});
    EXPECT_EQ(uppercase_variations(match), test.variants);
  }
}

TEST(ZxcvbnTest, L33tVariations) {
  {
    // 1 variant for non-l33t matches
    Match match(0, 0, "", DictionaryMatch{.l33t = false});
    EXPECT_EQ(l33t_variations(match), 1);
  }

  struct {
    std::string word;
    int variants;
    std::unordered_map<std::string, std::string> sub;
  } tests[] = {
      {"", 1, {}},
      {"a", 1, {}},
      {"4", 2, {{"4", "a"}}},
      {"4pple", 2, {{"4", "a"}}},
      {"abcet", 1, {}},
      {"4bcet", 2, {{"4", "a"}}},
      {"a8cet", 2, {{"8", "b"}}},
      {"abce+", 2, {{"+", "t"}}},
      {"48cet", 4, {{"4", "a"}, {"8", "b"}}},
      {"a4a4aa", nCk(6, 2) + nCk(6, 1), {{"4", "a"}}},
      {"4a4a44", nCk(6, 2) + nCk(6, 1), {{"4", "a"}}},
      {"a44att+",
       (nCk(4, 2) + nCk(4, 1)) * nCk(3, 1),
       {{"4", "a"}, {"+", "t"}}},
  };

  for (const auto& test : tests) {
    Match match(0, test.word.size() - 1, test.word,
                DictionaryMatch{
                    .l33t = !test.sub.empty(),
                    .sub = test.sub,
                });

    EXPECT_EQ(l33t_variations(match), test.variants);
  }

  {
    // capitalization doesn't affect extra l33t guesses calc
    Match match(0, 5, "Aa44aA",
                DictionaryMatch{
                    .l33t = true,
                    .sub = {{"4", "a"}},
                });

    int variants = nCk(6, 2) + nCk(6, 1);
    EXPECT_EQ(l33t_variations(match), variants);
  }
}

}  // namespace zxcvbn
