#include <zxcvbn/matching.hpp>

#include <zxcvbn/adjacency_graphs.hpp>
#include <zxcvbn/common.hpp>
#include <zxcvbn/optional.hpp>
#include <zxcvbn/frequency_lists.hpp>
#include <zxcvbn/scoring.hpp>
#include <zxcvbn/util.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <initializer_list>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <unordered_set>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace zxcvbn {

// TODO: make this a constexpr
const std::vector<std::pair<std::string, std::vector<std::string>>>&
L33T_TABLE() {
  static base::NoDestructor<
      std::vector<std::pair<std::string, std::vector<std::string>>>>
      leet_table({
          {"a", {"4", "@"}},
          {"b", {"8"}},
          {"c", {"(", "{", "[", "<"}},
          {"e", {"3"}},
          {"g", {"6", "9"}},
          {"i", {"1", "!", "|"}},
          {"l", {"1", "|", "7"}},
          {"o", {"0"}},
          {"s", {"$", "5"}},
          {"t", {"+", "7"}},
          {"x", {"%"}},
          {"z", {"2"}},
      });

  return *leet_table;
}

// TODO: make this constexpr
const std::vector<std::pair<RegexTag, std::regex>>& REGEXEN() {
  static base::NoDestructor<std::vector<std::pair<RegexTag, std::regex>>>
      regexen({
          {RegexTag::RECENT_YEAR, std::regex(R"(19\d\d|200\d|201\d)")},
      });
  return *regexen;
}

const auto DATE_MAX_YEAR = 2050;
const auto DATE_MIN_YEAR = 1000;
constexpr std::initializer_list<std::pair<int, int>> DATE_SPLITS[] = {
  {      // for length-4 strings, eg 1191 or 9111, two ways to split:
    {1, 2}, // 1 1 91 (2nd split starts at index 1, 3rd at index 2)
    {2, 3}, // 91 1 1
  },
  {
    {1, 3}, // 1 11 91
    {2, 3}, // 11 1 91
  },
  {
    {1, 2}, // 1 1 1991
    {2, 4}, // 11 11 91
    {4, 5}, // 1991 1 1
  },
  {
    {1, 3}, // 1 11 1991
    {2, 3}, // 11 1 1991
    {4, 5}, // 1991 1 11
    {4, 6}, // 1991 11 1
  },
  {
    {2, 4}, // 11 11 1991
    {4, 6}, // 1991 11 11
  },
};

static
std::string translate(const std::string & string,
                      const std::unordered_map<std::string, std::string> & chr_map) {
  std::string toret;
  auto bit = std::back_inserter(toret);
  toret.reserve(string.size());
  for (auto it = string.begin(); it != string.end();) {
    auto nextit = util::utf8_iter(it, string.end());
    auto ch = std::string(it, nextit);
    auto mit = chr_map.find(ch);
    if (mit != chr_map.end()) {
      ch = mit->second;
    }
    std::copy(ch.begin(), ch.end(), bit);
    it = nextit;
  }
  return toret;
}

static
std::vector<Match> & sorted(std::vector<Match> & matches) {
  std::sort(matches.begin(), matches.end(),
            [&] (const Match & m1, const Match & m2) -> bool {
              return std::make_pair(m1.i, m1.j) < std::make_pair(m2.i, m2.j);
            });
  return matches;
}

static
std::string dict_normalize(const std::string & str) {
  // NB: we only have ascii strings in the dictionaries
  // TODO: when we have more complex strings in the dictionaries,
  //       do a more complex normalization
  return util::ascii_lower(str);
}

std::vector<Match> omnimatch(const std::string& password) {
  RankedDicts& ranked_dictionaries = default_ranked_dicts();

  std::vector<Match> matches;
  std::function<std::vector<Match>(const std::string&)> matchers[] = {
      std::bind(dictionary_match, std::placeholders::_1,
                std::cref(ranked_dictionaries)),
      std::bind(reverse_dictionary_match, std::placeholders::_1,
                std::cref(ranked_dictionaries)),
      std::bind(l33t_match, std::placeholders::_1,
                std::cref(ranked_dictionaries), std::cref(L33T_TABLE())),
      std::bind(spatial_match, std::placeholders::_1, std::cref(graphs())),
      repeat_match,
      sequence_match,
      std::bind(regex_match, std::placeholders::_1, std::cref(REGEXEN())),
      date_match,
  };
  for (const auto & matcher : matchers) {
    auto ret = matcher(password);
    std::move(ret.begin(), ret.end(), std::back_inserter(matches));
  }
  return sorted(matches);
}

//-------------------------------------------------------------------------------
//  dictionary match (common passwords, english, last names, etc) ----------------
//-------------------------------------------------------------------------------

std::vector<Match> dictionary_match(const std::string & password,
                                    const RankedDicts & ranked_dictionaries) {
  std::vector<Match> matches;
  size_t len = password.length();
  std::string password_lower = dict_normalize(password);
  for (size_t i = 0, idx = 0; idx < len;
       util::utf8_decode(password, idx), ++i) {
    for (size_t j = i, jdx = idx; jdx < len; ++j) {
      // j is inclusive, but jdx is not so eagerly iterate jdx
      util::utf8_decode(password, jdx);

      std::string word = password_lower.substr(idx, jdx - idx);
      absl::optional<rank_t> result = ranked_dictionaries.Find(word);
      if (result.has_value()) {
        rank_t rank = *result;
        matches.emplace_back(i, j, password.substr(idx, jdx - idx),
                             DictionaryMatch{word, rank, false, false, {}, ""});
        matches.back().idx = idx;
        matches.back().jdx = jdx;
      }
    }
  }
  return sorted(matches);
}

std::vector<Match> reverse_dictionary_match(const std::string & password,
                                            const RankedDicts & ranked_dictionaries) {
  auto clen = util::character_len(password);
  auto reversed_password = util::reverse_string(password);
  auto matches = dictionary_match(reversed_password, ranked_dictionaries);
  for (auto & match : matches) {
    match.token = util::reverse_string(match.token); // reverse back
    match.get_dictionary().reversed = true;
    // map coordinates back to original string
    std::tie(match.i, match.j) = std::make_tuple(
      clen - 1 - match.j,
      clen - 1 - match.i
      );
    std::tie(match.idx, match.jdx) = std::make_tuple(
      password.length() - match.jdx,
      password.length() - match.idx
      );
  }
  return sorted(matches);
}

//-------------------------------------------------------------------------------
// dictionary match with common l33t substitutions ------------------------------
//-------------------------------------------------------------------------------

// makes a pruned copy of l33t_table that only includes password's possible substitutions
std::unordered_map<std::string, std::vector<std::string>> relevant_l33t_subtable(const std::string & password, const std::vector<std::pair<std::string, std::vector<std::string>>> & table) {
  std::unordered_map<std::string, std::vector<std::string>> subtable;
  for (const auto & item : table) {
    auto & letter = item.first;
    auto & subs = item.second;
    std::vector<std::string> relevant_subs;
    for (const auto & sub : subs) {
      if (password.find(sub) != password.npos) {
        relevant_subs.push_back(sub);
      }
    }

    if (relevant_subs.size()) {
      subtable.insert(std::make_pair(letter, relevant_subs));
    }
  }
  return subtable;
}

// returns the list of possible 1337 replacement dictionaries for a given password
std::vector<std::unordered_map<std::string, std::string>> enumerate_l33t_subs(const std::unordered_map<std::string, std::vector<std::string>> & table) {
  using SubsType = std::vector<std::vector<std::pair<std::string, std::string>>>;
  SubsType subs = {{}};

  auto dedup = [] (const SubsType & subs) {
    SubsType deduped;
    std::unordered_set<std::string> members;
    for (const auto & sub : subs) {
      auto assoc = sub;
      std::sort(assoc.begin(), assoc.end());
      std::ostringstream label_stream;
      for (const auto & item : assoc) {
        label_stream << item.first << "," << item.second << "-";
      }
      auto label = label_stream.str();
      if (members.find(label) == members.end()) {
        members.insert(std::move(label));
        deduped.push_back(sub);
      }
    }
    return deduped;
  };

  for (const auto & item : table) {
    auto & first_key = item.first;
    SubsType next_subs;
    for (const auto & l33t_chr : item.second) {
      for (const auto & sub : subs) {
        auto sub_alternative = sub;
        auto it = std::find_if(
          sub_alternative.begin(), sub_alternative.end(),
          [&] (const std::pair<std::string, std::string> & sub_elt) {
            return sub_elt.first == l33t_chr;
          });
        if (it == sub_alternative.end()) {
          sub_alternative.push_back(std::make_pair(l33t_chr, first_key));
          next_subs.push_back(std::move(sub_alternative));
        }
        else {
          sub_alternative.erase(it);
          sub_alternative.push_back(std::make_pair(l33t_chr, first_key));
          next_subs.push_back(sub);
          next_subs.push_back(std::move(sub_alternative));
        }
      }
    }
    subs = dedup(next_subs);
  }

  // convert from assoc lists to dicts
  std::vector<std::unordered_map<std::string, std::string>> sub_dicts;
  for (const auto & sub : subs) {
    sub_dicts.push_back(std::unordered_map<std::string, std::string>
                        (sub.begin(), sub.end()));
  }

  return sub_dicts;
}

std::vector<Match> l33t_match(const std::string & password,
                              const RankedDicts & ranked_dictionaries,
                              const std::vector<std::pair<std::string, std::vector<std::string>>> & l33t_table) {
  std::vector<Match> matches;
  for (const auto & sub : enumerate_l33t_subs(relevant_l33t_subtable(password, l33t_table))) {
    if (!sub.size()) break;
    auto subbed_password = translate(password, sub);
    for (auto & match : dictionary_match(subbed_password, ranked_dictionaries)) {
      auto & dmatch = match.get_dictionary();
      auto token = password.substr(match.idx, match.jdx - match.idx);
      if (dict_normalize(token) == dmatch.matched_word) {
        // only return the matches that contain an actual substitution
        continue;
      }
      // subset of mappings in sub that are in use for this match
      std::unordered_map<std::string, std::string> match_sub;
      for (const auto & item : sub) {
        auto & subbed_chr = item.first;
        if (token.find(subbed_chr) == token.npos) continue;
        match_sub.insert(item);
      }
      dmatch.l33t = true;
      match.token = token;
      dmatch.sub = match_sub;
      std::ostringstream os;
      std::string sep = "";
      for (const auto & item : match_sub) {
        os << sep << item.first << " -> " << item.second;
        if (!sep.size()) {
          sep = ", ";
        }
      }
      dmatch.sub_display = os.str();
      matches.push_back(std::move(match));
    }
  }

  matches.erase(std::remove_if(matches.begin(), matches.end(), [] (const Match & a) {
        // filter single-character l33t matches to reduce noise.
        // otherwise '1' matches 'i', '4' matches 'a', both very common English words
        // with low dictionary rank.
        return util::character_len(a.token) <= 1;
      }),
    matches.end());

  return sorted(matches);
}

// ------------------------------------------------------------------------------
// spatial match (qwerty/dvorak/keypad) -----------------------------------------
// ------------------------------------------------------------------------------

static
std::vector<Match> spatial_match_helper(const std::string & password,
                                        const Graph & graph,
                                        GraphTag tag);

std::vector<Match> spatial_match(const std::string & password,
                                 const Graphs & graphs) {
  std::vector<Match> matches;
  for (const auto & item : graphs) {
    auto ret = spatial_match_helper(password, item.second, item.first);
    std::move(ret.begin(), ret.end(), std::back_inserter(matches));
  }
  return matches;
}

static
std::vector<Match> spatial_match_helper(const std::string & password,
                                        const Graph & graph,
                                        GraphTag graph_tag) {
  const auto SHIFTED_RX =
      std::regex("[~!@#$%^&*()_+QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?]");

  std::vector<Match> matches;
  if (!password.length()) return matches;
  idx_t idx = 0;
  idx_t i = 0;
  auto clen = util::character_len(password);
  while (i < clen - 1) {
    auto jdx = idx;
    util::utf8_decode(password, jdx);
    auto j = i + 1;
    auto last_direction = -1;
    unsigned turns = 0;
    unsigned shifted_count;
    if ((graph_tag == GraphTag::QWERTY ||
         graph_tag == GraphTag::DVORAK) &&
        std::regex_search(password.substr(idx, jdx - idx), SHIFTED_RX)) {
      shifted_count = 1;
    }
    else {
      shifted_count = 0;
    }
    auto prev_jdx = idx;
    while (true) {
      auto prev_char = password.substr(prev_jdx, jdx - prev_jdx);
      auto found = false;
      auto found_direction = -1;
      auto cur_direction = -1;
      const auto & adjacents = [&] {
        auto it = graph.find(prev_char);
        if (it != graph.end()) {
          return it->second;
        }
        return Graph::mapped_type();
      }();
      // consider growing pattern by one character if j hasn't gone over the edge.
      if (j < clen) {
        auto next_jdx = jdx;
        util::utf8_decode(password, next_jdx);
        auto cur_char = password.substr(jdx, next_jdx - jdx);
        for (auto & adj : adjacents) {
          cur_direction += 1;
          if (adj.find(cur_char) != adj.npos) {
            found = true;
            found_direction = cur_direction;
            if (adj.find(cur_char) == 1) {
              // index 1 in the adjacency means the key is shifted,
              // 0 means unshifted: A vs a, % vs 5, etc.
              // for example, 'q' is adjacent to the entry '2@'.
              // @ is shifted w/ index 1, 2 is unshifted.
              shifted_count += 1;
            }
            if (last_direction != found_direction) {
              // adding a turn is correct even in the initial case when last_direction is null:
              // every spatial pattern starts with a turn.
              turns += 1;
              last_direction = found_direction;
            }
            break;
          }
        }
      }
      // if the current pattern continued, extend j and try to grow again
      if (found) {
        j += 1;
        prev_jdx = jdx;
        util::utf8_decode(password, jdx);
      }
      // otherwise push the pattern discovered so far, if any...
      else {
        if (j - i > 2) { // don't consider length 1 or 2 chains.
          matches.push_back(Match(i, j - 1, password.substr(idx, jdx - idx),
                                  SpatialMatch{
                                    graph_tag, turns, shifted_count,
                                  }));
          matches.back().idx = idx;
          matches.back().jdx = jdx;
        }
        // ...and then start a new search for the rest of the password.
        i = j;
        idx = jdx;
        break;
      }
    }
  }
  return matches;
}

//-------------------------------------------------------------------------------
// repeats (aaa, abcabcabc) and sequences (abcdef) ------------------------------
//-------------------------------------------------------------------------------

std::vector<Match> repeat_match(const std::string& password) {
  std::vector<Match> matches;

  auto unicode_password = icu::UnicodeString::fromUTF8(password);

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::RegexPattern> greedy_pattern(icu::RegexPattern::compile(
      icu::UnicodeString::fromUTF8(R"((.+)\1+)"), 0, status));
  std::unique_ptr<icu::RegexMatcher> greedy_matcher(
      greedy_pattern->matcher(unicode_password, status));

  std::unique_ptr<icu::RegexPattern> lazy_pattern(icu::RegexPattern::compile(
      icu::UnicodeString::fromUTF8(R"((.+?)\1+)"), 0, status));
  std::unique_ptr<icu::RegexMatcher> lazy_matcher(
      lazy_pattern->matcher(unicode_password, status));

  std::unique_ptr<icu::RegexPattern> lazy_anchored_pattern(
      icu::RegexPattern::compile(icu::UnicodeString::fromUTF8(R"(^(.+?)\1+$)"),
                                 0, status));

  int lastUnicodeIndex = 0;
  size_t lastIndex = 0;
  while (lastIndex < password.length()) {
    if (!greedy_matcher->find(lastUnicodeIndex, status) ||
        !lazy_matcher->find(lastUnicodeIndex, status)) {
      break;
    }

    icu::RegexMatcher* matcher = nullptr;
    icu::UnicodeString base_token;
    if (greedy_matcher->group(status).length() >
        lazy_matcher->group(status).length()) {
      // greedy beats lazy for 'aabaab'
      //   greedy: [aabaab, aab]
      //   lazy:   [aa,     a]
      matcher = greedy_matcher.get();
      // greedy's repeated string might itself be repeated, eg.
      // aabaab in aabaabaabaab.
      // run an anchored lazy match on greedy's repeated string
      // to find the shortest repeated string
      auto greedy_found = matcher->group(status);

      std::unique_ptr<icu::RegexMatcher> lazy_anchored_matcher(
          lazy_anchored_pattern->matcher(greedy_found, status));
      auto ret = lazy_anchored_matcher->find(status);
      assert(ret);
      (void) ret;
      base_token = lazy_anchored_matcher->group(1, status);
    } else {
      // lazy beats greedy for 'aaaaa'
      //   greedy: [aaaa,  aa]
      //   lazy:   [aaaaa, a]
      matcher = lazy_matcher.get();
      base_token = matcher->group(1, status);
    }

    std::string matched_string;
    matcher->group(status).toUTF8String(matched_string);

    auto idx = password.find(matched_string, lastIndex);
    auto jdx = idx + matched_string.size();

    auto i = util::character_len(password, 0, idx);
    auto j = i + util::character_len(password, idx, jdx) - 1;
    // recursively match and score the base string
    std::string base_string;
    base_token.toUTF8String(base_string);
    auto sub_matches = omnimatch(base_string);
    auto base_analysis =
        most_guessable_match_sequence(base_string, sub_matches, false);
    std::vector<Match> base_matches;
    std::move(base_analysis.sequence.begin(), base_analysis.sequence.end(),
              std::back_inserter(base_matches));
    auto& base_guesses = base_analysis.guesses;
    matches.push_back(Match(i, j, matched_string,
                            RepeatMatch{
                                base_string,
                                base_guesses,
                                std::move(base_matches),
                                matched_string.size() / base_string.size(),
                            }));
    matches.back().idx = idx;
    matches.back().jdx = jdx;
    lastUnicodeIndex = matcher->end(status);
    lastIndex = jdx;
  }
  return matches;
}

const auto MAX_DELTA = 5;
std::vector<Match> sequence_match(const std::string & password) {
  // Identifies sequences by looking for repeated differences in unicode codepoint.
  // this allows skipping, such as 9753, and also matches some extended unicode sequences
  // such as Greek and Cyrillic alphabets.
  //
  // for example, consider the input 'abcdb975zy'
  //
  // password: a   b   c   d   b    9   7   5   z   y
  // index:    0   1   2   3   4    5   6   7   8   9
  // delta:      1   1   1  -2  -41  -2  -2  69   1
  //
  // expected result:
  // [(i, j, delta), ...] = [(0, 3, 1), (5, 7, -2), (8, 9, 1)]

  if (util::character_len(password) == 1) return {};

  std::vector<Match> result;

  using delta_t = std::int32_t;

  auto update = [&] (idx_t i, idx_t j, idx_t idx, idx_t jdx, delta_t delta) {
    if (j - i > 1 || std::abs(delta) == 1) {
      if (0 < std::abs(delta) && std::abs(delta) <= MAX_DELTA) {
        auto token = password.substr(idx, jdx - idx);
        SequenceTag sequence_name;
        unsigned sequence_space;
        if (std::regex_search(token, std::regex(R"(^[a-z]+$)"))) {
          sequence_name = SequenceTag::kLower;
          sequence_space = 26;
        }
        else if (std::regex_search(token, std::regex(R"(^[A-Z]+$)"))) {
          sequence_name = SequenceTag::kUpper;
          sequence_space = 26;
        }
        else if (std::regex_search(token, std::regex(R"(^\d+$)"))) {
          sequence_name = SequenceTag::kDigits;
          sequence_space = 10;
        }
        else {
          sequence_name = SequenceTag::kUnicode;
          sequence_space = 26;
        }
        result.push_back(Match(i, j, token,
                               SequenceMatch{sequence_name, sequence_space,
                                   delta > 0}));
        result.back().idx = idx;
        result.back().jdx = jdx;
      }
    }
  };

  if (!password.size()) return result;

  decltype(password.length()) i = 0;
  decltype(password.length()) idx = 0;
  optional::optional<delta_t> maybe_last_delta;
  decltype(password.length()) kdx = 0;
  auto last_kdx = kdx;
  auto last_cp = util::utf8_decode(password, kdx);
  for (idx_t k = 1; kdx < password.length(); ++k) {
    auto next_kdx = kdx;
    auto cp = util::utf8_decode(password, next_kdx);
    assert(kdx != next_kdx);
    delta_t delta = cp - last_cp;
    if (!maybe_last_delta) {
      maybe_last_delta = delta;
    }
    if (delta != *maybe_last_delta) {
      auto jdx = kdx;
      auto j = k - 1;
      update(i, j, idx, jdx, *maybe_last_delta);
      i = j;
      idx = last_kdx;
      maybe_last_delta = delta;
    }
    last_kdx = kdx;
    kdx = next_kdx;
    last_cp = cp;
  }
  if (maybe_last_delta) {
    update(i, util::character_len(password) - 1,
           idx, password.size(), *maybe_last_delta);
  }
  return result;
}


//-------------------------------------------------------------------------------
// regex matching ---------------------------------------------------------------
//-------------------------------------------------------------------------------

std::vector<Match> regex_match(const std::string & password,
                               const std::vector<std::pair<RegexTag, std::regex>> & regexen) {
  std::vector<Match> matches;
  for (const auto & item : regexen) {
    auto tag = item.first;
    auto & regex = item.second;
    std::smatch rx_match;
    std::size_t lastIndex = 0;
    while (std::regex_match(lastIndex + password.begin(), password.end(),
                            rx_match, regex)) {
      auto token = rx_match.str(0);
      auto idx = lastIndex + rx_match.position();
      auto jdx = lastIndex + rx_match.position() + rx_match[0].length();
      assert(token == password.substr(idx, jdx - idx));
      auto i = util::character_len(password, 0, idx);
      auto j = i + util::character_len(password, idx, jdx) - 1;
      matches.push_back(Match(i, j,
                              std::move(token),
                              RegexMatch{tag, PortableRegexMatch(rx_match)}));
      matches.back().idx = idx;
      matches.back().jdx = jdx;
      lastIndex += rx_match[0].length();
    }
  }
  return sorted(matches);
}

//-------------------------------------------------------------------------------
// date matching ----------------------------------------------------------------
//-------------------------------------------------------------------------------

using date_t = unsigned;

struct DMY {
  date_t year, month, day;
};

static
optional::optional<DMY> map_ints_to_dmy(const std::array<date_t, 3> & vals);

static
date_t stou(const std::string & a) {
  return static_cast<date_t>(std::stoul(a));
}

std::vector<Match> date_match(const std::string & password) {
  // a "date" is recognized as:
  //   any 3-tuple that starts or ends with a 2- or 4-digit year,
  //   with 2 or 0 separator chars (1.1.91 or 1191),
  //   maybe zero-padded (01-01-91 vs 1-1-91),
  //   a month between 1 and 12,
  //   a day between 1 and 31.
  //
  // note: this isn't true date parsing in that "feb 31st" is allowed,
  // this doesn't check for leap years, etc.
  //
  // recipe:
  // start with regex to find maybe-dates, then attempt to map the integers
  // onto month-day-year to filter the maybe-dates into dates.
  // finally, remove matches that are substrings of other matches to reduce noise.
  //
  // note: instead of using a lazy or greedy regex to find many dates over the full string,
  // this uses a ^...$ regex against every substring of the password -- less performant but leads
  // to every possible date match.
  std::vector<Match> matches;
  std::regex maybe_date_no_separator(R"(^\d{4,8}$)");
  std::regex maybe_date_with_separator(R"(^(\d{1,4})([\s/\\_.-])(\d{1,2})\2(\d{1,4})$)");

  // dates without separators are between length 4 '1191' and 8 '11111991'
  std::vector<std::string::size_type> offsets;
  offsets.reserve(9);
  idx_t idx_dot = 0;
  for (auto i = 0; i < 9; ++i) {
    offsets.push_back(idx_dot);
    if (idx_dot >= password.length()) {
      break;
    }
    util::utf8_decode(password, idx_dot);
  }
  assert(offsets.size());
  for (decltype(password.length()) i = 0; offsets.size() - 1 >= 4; ++i) {
    auto idx = offsets[0];
    for (decltype(i) offset = 3; offset <= 7 && offset < offsets.size() - 1; ++offset) {
      auto j = i + offset;
      auto jdx = offsets[offset + 1];

      auto token = password.substr(idx, jdx - idx);
      auto token_chr_len = j - i + 1;
      assert(util::character_len(token) == token_chr_len);
      if (!std::regex_search(token, maybe_date_no_separator)) continue;
      std::vector<DMY> candidates;
      for (const auto & item : DATE_SPLITS[token_chr_len - 4]) {
        auto k = item.first;
        auto l = item.second;
        auto kdx = offsets[k] - idx;
        auto ldx = offsets[l] - idx;

        auto dmy = map_ints_to_dmy(std::array<date_t, 3>{{
              stou(token.substr(0, kdx)),
              stou(token.substr(kdx, ldx - kdx)),
              stou(token.substr(ldx))}});
        if (dmy) candidates.push_back(*dmy);
      }
      if (!candidates.size()) continue;
      // at this point: different possible dmy mappings for the same i,j substring.
      // match the candidate date that likely takes the fewest guesses: a year closest to 2000.
      // (scoring.REFERENCE_YEAR).
      //
      // ie, considering '111504', prefer 11-15-04 to 1-1-1504
      // (interpreting '04' as 2004)
      auto metric = [] (const DMY & candidate) {
        if (candidate.year >= REFERENCE_YEAR) {
          return candidate.year - REFERENCE_YEAR;
        }
        else {
          return REFERENCE_YEAR - candidate.year;
        }
      };
      auto best_candidate = *std::min_element(candidates.begin(), candidates.end(),
                                              [=] (const DMY & a, const DMY & b) {
                                                return metric(a) < metric(b);
                                              });
      matches.push_back(Match(i, j, token,
                              DateMatch{"",
                                  best_candidate.year,
                                  best_candidate.month,
                                  best_candidate.day,
                                  false,
                                  }));
      matches.back().idx = idx;
      matches.back().jdx = jdx;
    }

    offsets.erase(offsets.begin());
    if (offsets.back() < password.length()) {
      auto idx2 = offsets.back();
      util::utf8_decode(password, idx2);
      offsets.push_back(idx2);
    }
  }

  // dates with separators are between length 6 '1/1/91' and 10 '11/11/1991'
  offsets.clear();
  offsets.reserve(11);
  idx_dot = 0;
  for (auto i = 0; i < 11; ++i) {
    offsets.push_back(idx_dot);
    if (idx_dot >= password.length()) {
      break;
    }
    util::utf8_decode(password, idx_dot);
  }
  assert(offsets.size());
  for (decltype(password.length()) i = 0; offsets.size() - 1 >= 6; ++i) {
    auto idx = offsets[0];
    for (decltype(password.length()) offset = 5; offset <= 9 && offset < offsets.size() - 1; ++offset) {
      auto j = offset + i;
      auto jdx = offsets[offset + 1];
      auto token = password.substr(idx, jdx - idx);
      std::smatch rx_match;
      if (!std::regex_match(token, rx_match, maybe_date_with_separator)) {
        continue;
      }
      auto dmy = map_ints_to_dmy(std::array<date_t, 3>{{
          stou(rx_match[1]),
          stou(rx_match[3]),
          stou(rx_match[4])}});
      if (!dmy) continue;
      matches.push_back(Match(i, j, token,
                              DateMatch{rx_match[2],
                                  dmy->year,
                                  dmy->month,
                                  dmy->day,
                                  false,
                                  }));
      matches.back().idx = idx;
      matches.back().jdx = jdx;
    }

    offsets.erase(offsets.begin());
    if (offsets.back() < password.length()) {
      auto idx2 = offsets.back();
      util::utf8_decode(password, idx2);
      offsets.push_back(idx2);
    }
  }

  // matches now contains all valid date strings in a way that is tricky to capture
  // with regexes only. while thorough, it will contain some unintuitive noise:
  //
  // '2015_06_04', in addition to matching 2015_06_04, will also contain
  // 5(!) other date matches: 15_06_04, 5_06_04, ..., even 2015 (matched as 5/1/2020)
  //
  // to reduce noise, remove date matches that are strict substrings of others
  matches.erase(std::remove_if(matches.begin(), matches.end(), [&] (const Match & match) {
        for (auto & other_match : matches) {
          if (other_match.i == match.i && other_match.j == match.j) continue;
          if (other_match.i <= match.i && other_match.j >= match.j) {
            return true;
          }
        }
        return false;
      }),
    matches.end());

  return sorted(matches);
}

static
optional::optional<DMY> map_ints_to_dm(const std::array<date_t, 2> & vals);

static
date_t two_to_four_digit_year(date_t val);

static
optional::optional<DMY> map_ints_to_dmy(const std::array<date_t, 3> & vals) {
  // given a 3-tuple, discard if:
  //   middle int is over 31 (for all dmy formats, years are never allowed in the middle)
  //   middle int is zero
  //   any int is over the max allowable year
  //   any int is over two digits but under the min allowable year
  //   2 ints are over 31, the max allowable day
  //   2 ints are zero
  //   all ints are over 12, the max allowable month
  if (vals[1] > 31 || vals[1] == 0) return optional::nullopt;
  auto over_12 = 0;
  auto over_31 = 0;
  auto under_1 = 0;
  for (auto val : vals) {
    if ((99 < val && val < DATE_MIN_YEAR) || val > DATE_MAX_YEAR) return optional::nullopt;
    if (val > 31) over_31 += 1;
    if (val > 12) over_12 += 1;
    if (val <= 0) under_1 += 1;
  }
  if (over_31 >= 2 || over_12 == 3 || under_1 >= 2) return optional::nullopt;

  // first look for a four digit year: yyyy + daymonth or daymonth + yyyy
  std::pair<date_t, std::array<date_t, 2>> possible_year_splits[] = {
    {vals[2], {{vals[0], vals[1]}}}, // year last
    {vals[0], {{vals[1], vals[2]}}}, // year first
  };
  for (const auto & item : possible_year_splits) {
    auto & y = item.first;
    auto & rest = item.second;
    if (DATE_MIN_YEAR <= y && y <= DATE_MAX_YEAR) {
      auto dm = map_ints_to_dm(rest);
      if (dm) {
        return DMY{y, dm->month, dm->day};
      }
      else {
        // for a candidate that includes a four-digit year,
        // when the remaining ints don't match to a day and month,
        // it is not a date.
        return optional::nullopt;
      }
    }
  }

  // given no four-digit year, two digit years are the most flexible int to match, so
  // try to parse a day-month out of ints[0..1] or ints[1..0]
  for (const auto & item : possible_year_splits) {
    auto y = item.first;
    auto & rest = item.second;
    auto dm = map_ints_to_dm(rest);
    if (dm) {
      y = two_to_four_digit_year(y);
      return DMY{y, dm->month, dm->day};
    }
  }

  return optional::nullopt;
}

static
optional::optional<DMY> map_ints_to_dm(const std::array<date_t, 2> & vals) {
  for (const auto & item : {vals, {{vals[1], vals[0]}}}) {
    auto d = item[0], m = item[1];
    if (1 <= d && d <= 31 && 1 <= m && m <= 12) {
      return DMY{0, m, d};
    }
  }
  return optional::nullopt;
}

static
date_t two_to_four_digit_year(date_t year) {
  if (year > 99) {
    return year;
  }
  else if (year > 50) {
    // 87 -> 1987
    return year + 1900;
  }
  else {
    // 15 -> 2015
    return year + 2000;
  }
}

}
