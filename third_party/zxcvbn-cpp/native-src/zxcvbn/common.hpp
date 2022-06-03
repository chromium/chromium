#ifndef __ZXCVBN__COMMON_HPP
#define __ZXCVBN__COMMON_HPP

#include <zxcvbn/frequency_lists.hpp>
#include <zxcvbn/adjacency_graphs.hpp>

#include <regex>
#include <string>

#include <cassert>

namespace zxcvbn {

using guesses_t = double;
using guesses_log10_t = int;
using score_t = unsigned;
using idx_t = std::string::size_type;

// Add new match types here
#define MATCH_RUN() \
  MATCH_FN(Dictionary, DICTIONARY, dictionary) \
  MATCH_FN(Spatial, SPATIAL, spatial) \
  MATCH_FN(Repeat, REPEAT, repeat) \
  MATCH_FN(Sequence, SEQUENCE, sequence) \
  MATCH_FN(Regex, REGEX, regex) \
  MATCH_FN(Date, DATE, date) \
  MATCH_FN(Bruteforce, BRUTEFORCE, bruteforce) \
  MATCH_FN(Unknown, UNKNOWN, unknown)

enum class RegexTag {
  RECENT_YEAR,
  ALPHA_LOWER,
  ALPHANUMERIC,
};

enum class SequenceTag {
  kUpper,
  kLower,
  kDigits,
  kUnicode,
};

struct PortableRegexMatch {
  std::vector<std::string> matches;
  std::size_t index;

  explicit
  PortableRegexMatch(const std::smatch & b) {
    std::copy(b.begin(),  b.end(), std::back_inserter(matches));
    index = b.position();
  }

  PortableRegexMatch(std::vector<std::string> matches_,
                     std::size_t index_)
    : matches(std::move(matches_))
    , index(index_)
    {}
};

#define MATCH_FN(_, e, __) e,
enum class MatchPattern {
  MATCH_RUN()
};
#undef MATCH_FN

struct DictionaryMatch {
  static constexpr auto pattern = MatchPattern::DICTIONARY;

  std::string matched_word;
  rank_t rank;
  bool l33t;
  bool reversed;

  // for l33t matches
  std::unordered_map<std::string, std::string> sub;
  std::string sub_display;
};

struct SpatialMatch {
  static constexpr auto pattern = MatchPattern::SPATIAL;

  GraphTag graph;
  unsigned turns;
  idx_t shifted_count;
};

class Match;

struct RepeatMatch {
  static constexpr auto pattern = MatchPattern::REPEAT;

  std::string base_token;
  guesses_t base_guesses;
  std::vector<Match> base_matches;
  std::size_t repeat_count;
};

struct SequenceMatch {
  static constexpr auto pattern = MatchPattern::SEQUENCE;

  SequenceTag sequence_tag;
  unsigned sequence_space;
  bool ascending;
};

struct RegexMatch {
  static constexpr auto pattern = MatchPattern::REGEX;

  RegexTag regex_tag;
  PortableRegexMatch regex_match;
};

struct DateMatch {
  static constexpr auto pattern = MatchPattern::DATE;

  std::string separator;
  unsigned year, month, day;
  bool has_full_year;
};

struct BruteforceMatch {
  static constexpr auto pattern = MatchPattern::BRUTEFORCE;
};

struct UnknownMatch {
  static constexpr auto pattern = MatchPattern::UNKNOWN;
};

// Define new match types here

class Match {
private:
  MatchPattern _pattern;
#define MATCH_FN(title, upper, lower) title##Match _##lower;
  union {
    MATCH_RUN()
  };
#undef MATCH_FN

  template<class T>
  void _init(T && val) {
    i = val.i;
    j = val.j;
    token = std::forward<T>(val).token;
    guesses = val.guesses;
    guesses_log10 = val.guesses_log10;
    idx = val.idx;
    jdx = val.jdx;
    _pattern = val._pattern;

#define MATCH_FN(title, upper, lower) \
    case MatchPattern::upper:                                           \
    new (&_##lower) title##Match(std::forward<T>(val)._##lower);        \
    break;

    switch (_pattern) {
      MATCH_RUN()
    default:
      assert(false);
    }
#undef MATCH_FN
  }

  template<class T>
  Match & _assign(T && val) {
    this->~Match();
    new (this) Match(std::forward<T>(val));
    return *this;
  }

public:
  // these are character offsets: [i, j]
  idx_t i, j;
  std::string token;
  guesses_t guesses;
  guesses_log10_t guesses_log10;
  // these are byte offsets into original string: [idx, jdx)
  idx_t idx, jdx;

  template<class T>
  Match(idx_t i_, idx_t j_, std::string token,
        T && val);

  Match(const Match & m) {
    _init(m);
  }

  Match(Match && m) {
    _init(std::move(m));
  }

  Match & operator=(const Match & m) {
    return _assign(m);
  }

  Match & operator=(Match && m) {
    return _assign(std::move(m));
  }

  ~Match() {
#define MATCH_FN(title, upper, lower) \
    case MatchPattern::upper:\
      _##lower.~title##Match();                 \
      break;

    switch (_pattern) {
      MATCH_RUN()
    default:
      assert(false);
    }
#undef MATCH_FN
  }

  MatchPattern get_pattern() const {
    return _pattern;
  }

#define MATCH_FN(title, upper, lower)           \
  title##Match & get_##lower() {                \
    assert(get_pattern() == MatchPattern::upper);        \
    return _##lower;                                    \
  }                                             \
                                                \
  const title##Match & get_##lower() const {    \
    assert(get_pattern() == MatchPattern::upper);        \
    return _##lower;                            \
  }

MATCH_RUN()

#undef MATCH_FN

  template<class T>
  friend struct pattern_type_to_pmc;
};

template<class T>
struct pattern_type_to_pmc;

#define MATCH_FN(title, upper, lower)                                   \
  template<>                                                            \
  struct pattern_type_to_pmc<title##Match> :                            \
    public std::integral_constant<decltype(&Match::_##lower), &Match::_##lower> {};

MATCH_RUN()

#undef MATCH_FN

template<class T>
Match::Match(idx_t i_, idx_t j_, std::string token,
             T && val) :
  i(i_), j(j_), token(std::move(token)),
  guesses(), guesses_log10(), idx(), jdx() {
  _pattern = T::pattern;
  new (&(this->*pattern_type_to_pmc<std::decay_t<T>>::value)) std::decay_t<T>(std::forward<T>(val));
}

}

#endif
