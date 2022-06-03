#ifndef __ZXCVBN__SCORING_HPP
#define __ZXCVBN__SCORING_HPP

#include <zxcvbn/common.hpp>

#include <functional>
#include <memory>
#include <string>
#include <regex>
#include <vector>

namespace zxcvbn {

const std::regex& START_UPPER();
const std::regex& END_UPPER();
const std::regex& ALL_UPPER();
const std::regex& ALL_LOWER();

const guesses_t MIN_YEAR_SPACE = 20;
const auto REFERENCE_YEAR = 2016;

struct ScoringResult {
  std::string password;
  guesses_t guesses;
  guesses_log10_t guesses_log10;
  std::vector<std::unique_ptr<Match>> bruteforce_matches;
  std::vector<std::reference_wrapper<Match>> sequence;
};

template<class T>
T nCk(T n, T k) {
  // http://blog.plover.com/math/choose.html
  if (k > n) return 0;
  if (k == 0) return 1;
  T r = 1;
  for (T d = 1; d <= k; ++d) {
    r *= n;
    r /= d;
    n -= 1;
  }
  return r;
}

ScoringResult most_guessable_match_sequence(const std::string & password,
                                            std::vector<Match> & matches,
                                            bool exclude_additive = false);

guesses_t estimate_guesses(Match & match, const std::string & password);

#define MATCH_FN(title, upper, lower) \
  guesses_t lower##_guesses(const Match &);
MATCH_RUN()
#undef MATCH_FN

guesses_t uppercase_variations(const Match & match);
guesses_t l33t_variations(const Match & match);

}

#endif

