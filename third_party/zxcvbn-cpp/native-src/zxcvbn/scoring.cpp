#include <zxcvbn/scoring.hpp>

#include <zxcvbn/adjacency_graphs.hpp>
#include <zxcvbn/util.hpp>

#include <numeric>
#include <string>
#include <vector>

#include <cmath>

#include "base/no_destructor.h"

namespace std {

template<class T, class U>
struct hash<std::pair<T, U>> {
  std::size_t operator()(const std::pair<T, U> & v) const {
    return std::hash<T>()(v.first) ^ std::hash<U>()(v.second);
  }
};

}

namespace zxcvbn {

const auto BRUTEFORCE_CARDINALITY = static_cast<guesses_t>(10);
const auto MIN_GUESSES_BEFORE_GROWING_SEQUENCE = static_cast<guesses_t>(10000);
const auto MIN_SUBMATCH_GUESSES_SINGLE_CHAR = static_cast<guesses_t>(10);
const auto MIN_SUBMATCH_GUESSES_MULTI_CHAR = static_cast<guesses_t>(50);

const std::regex& START_UPPER() {
  static base::NoDestructor<std::regex> start_upper(R"(^[A-Z][^A-Z]+$)");
  return *start_upper;
}

const std::regex& END_UPPER() {
  static base::NoDestructor<std::regex> end_upper(R"(^[^A-Z]+[A-Z]$)");
  return *end_upper;
}

const std::regex& ALL_UPPER() {
  static base::NoDestructor<std::regex> all_upper(R"(^[^a-z]+$)");
  return *all_upper;
}

const std::regex& ALL_LOWER() {
  static base::NoDestructor<std::regex> all_lower(R"(^[^A-Z]+$)");
  return *all_lower;
}

template<class Tret, class Tin>
Tret factorial(Tin n) {
  // unoptimized, called only on small n
  if (n < 2) return 1;
  Tret f = 1;
  for (Tin i = 2; i <= n; ++i) {
    f *= i;
  }
  return f;
}

template<class M, class K, class V>
static
void insert_or_assign(M & m, const K & k, V && v) {
  auto p = m.insert(std::make_pair(k, std::forward<V>(v)));
  if (!p.second) {
    p.first->second = std::forward<V>(v);
  }
}

static
std::size_t token_len(const Match & m) __attribute__((pure));

static
std::size_t token_len(const Match & m) {
  std::size_t result = m.j - m.i + 1;
  // Bruteforce matches might be any substring of the original string, which are
  // not necessarily aligned to UTF8 code points, and thus m.token might not be
  // a valid UTF8 string.
  if (m.get_pattern() != MatchPattern::BRUTEFORCE)
    assert(result == util::character_len(m.token));
  return result;
}

// ------------------------------------------------------------------------------
// search --- most guessable match sequence -------------------------------------
// ------------------------------------------------------------------------------
//
// takes a sequence of overlapping matches, returns the non-overlapping sequence with
// minimum guesses. the following is a O(l_max * (n + m)) dynamic programming algorithm
// for a length-n password with m candidate matches. l_max is the maximum optimal
// sequence length spanning each prefix of the password. In practice it rarely exceeds 5 and the
// search terminates rapidly.
//
// the optimal "minimum guesses" sequence is here defined to be the sequence that
// minimizes the following function:
//
//    g = l! * Product(m.guesses for m in sequence) + D^(l - 1)
//
// where l is the length of the sequence.
//
// the factorial term is the number of ways to order l patterns.
//
// the D^(l-1) term is another length penalty, roughly capturing the idea that an
// attacker will try lower-length sequences first before trying length-l sequences.
//
// for example, consider a sequence that is date-repeat-dictionary.
//  - an attacker would need to try other date-repeat-dictionary combinations,
//    hence the product term.
//  - an attacker would need to try repeat-date-dictionary, dictionary-repeat-date,
//    ..., hence the factorial term.
//  - an attacker would also likely try length-1 (dictionary) and length-2 (dictionary-date)
//    sequences before length-3. assuming at minimum D guesses per pattern type,
//    D^(l-1) approximates Sum(D^i for i in [1..l-1]
//
// ------------------------------------------------------------------------------

ScoringResult most_guessable_match_sequence(const std::string & password,
                                            std::vector<Match> & matches,
                                            bool exclude_additive) {
  auto n = password.length();

  // partition matches into sublists according to ending index j
  std::unordered_map<idx_t, std::vector<std::reference_wrapper<Match>>> matches_by_j;
  for (auto & m : matches) {
    matches_by_j[m.j].push_back(m);
  }
  // small detail: for deterministic output, sort each sublist by i
  for (auto & item : matches_by_j) {
    std::sort(item.second.begin(), item.second.end(),
              [&] (const std::reference_wrapper<Match> & a,
                   const std::reference_wrapper<Match> & b) {
                return a.get().i < b.get().i;
              });
  }

  struct {
    // optimal.m[k][l] holds final match in the best length-l match sequence covering the
    // password prefix up to k, inclusive.
    // if there is no length-l sequence that scores better (fewer guesses) than
    // a shorter match sequence spanning the same prefix, optimal.m[k][l] is undefined.
    std::unordered_map<idx_t, std::unordered_map<idx_t, std::reference_wrapper<Match>>> m;

    // same structure as optimal.m -- holds the product term Prod(m.guesses for m in sequence).
    // optimal.pi allows for fast (non-looping) updates to the minimization function.
    std::unordered_map<idx_t, std::unordered_map<idx_t, guesses_t>> pi;

    // same structure as optimal.m -- holds the overall metric.
    std::unordered_map<idx_t, std::unordered_map<idx_t, guesses_t>> g;
  } optimal;

  // helper: considers whether a length-l sequence ending at match m is better (fewer guesses)
  // than previously encountered sequences, updating state if so.
  auto update = [&] (Match & m, idx_t l) {
    auto k = m.j;
    auto pi = estimate_guesses(m, password);
    if (l > 1) {
      // we're considering a length-l sequence ending with match m:
      // obtain the product term in the minimization function by multiplying m's guesses
      // by the product of the length-(l-1) sequence ending just before m, at m.i - 1.
      pi *= optimal.pi[m.i - 1][l - 1];
    }
    // calculate the minimization func
    auto g = factorial<guesses_t>(l) * pi;
    if (!exclude_additive) {
      g += std::pow(MIN_GUESSES_BEFORE_GROWING_SEQUENCE, l - 1);
    }
    // update state if new best.
    // first see if any competing sequences covering this prefix, with l or fewer matches,
    // fare better than this sequence. if so, skip it and return.
    for (const auto & item : optimal.g[k]) {
      auto & competing_l = item.first;
      auto & competing_g = item.second;
      if (competing_l > l) continue;
      if (competing_g <= g) return;
    }
    // this sequence might be part of the final optimal sequence.
    insert_or_assign(optimal.g[k], l, g);
    insert_or_assign(optimal.m[k], l, std::ref(m));
    insert_or_assign(optimal.pi[k], l, pi);
  };

  // helper: make bruteforce match objects spanning i to j, inclusive.
  // TODO: we store bruteforce matches in this vector so that we can
  //       store references in optimal.m, this is arguable hacked, so fix this
  std::unordered_map<std::pair<idx_t, idx_t>, std::unique_ptr<Match>> bruteforces;
  auto make_bruteforce_match = [&] (idx_t i, idx_t j) -> std::reference_wrapper<Match> {
    auto p = bruteforces.insert(std::make_pair(std::make_pair(i, j),
                                               std::make_unique<Match>
                                               (i, j,
                                                password.substr(i, j - i + 1),
                                                BruteforceMatch{})));
    return std::ref(*p.first->second);
  };

  // helper: evaluate bruteforce matches ending at k.
  auto bruteforce_update = [&] (idx_t k) {
    // see if a single bruteforce match spanning the k-prefix is optimal.
    auto m = make_bruteforce_match(0, k);
    update(m, 1);
    for (idx_t i = 1; i <= k; ++i) {
      // generate k bruteforce matches, spanning from (i=1, j=k) up to (i=k, j=k).
      // see if adding these new matches to any of the sequences in optimal[i-1]
      // leads to new bests.
      auto m2 = make_bruteforce_match(i, k);
      for (const auto & item : optimal.m[i - 1]) {
        auto & l = item.first;
        auto & last_m = item.second;
        // corner: an optimal sequence will never have two adjacent bruteforce matches.
        // it is strictly better to have a single bruteforce match spanning the same region:
        // same contribution to the guess product with a lower length.
        // --> safe to skip those cases.
        if (last_m.get().get_pattern() == MatchPattern::BRUTEFORCE) continue;
        // try adding m to this length-l sequence.
        update(m2, l + 1);
      }
    }
  };

  // helper: step backwards through optimal.m starting at the end,
  // constructing the final optimal match sequence.
  auto unwind = [&] (idx_t n) {
    std::vector<std::reference_wrapper<Match>> optimal_match_sequence;
    if (!n) return optimal_match_sequence;
    auto k = n - 1;
    idx_t l = optimal.g[k].begin()->first;
    guesses_t g = optimal.g[k].begin()->second;
    for (const auto & item : optimal.g[k]) {
      auto & candidate_l = item.first;
      auto & candidate_g = item.second;
      if (candidate_g < g) {
        l = candidate_l;
        g = candidate_g;
      }
    }
    while (true) {
      auto it = optimal.m[k].find(l);
      assert(it != optimal.m[k].end());
      auto & m = it->second;
      optimal_match_sequence.push_back(m);
      if (!m.get().i) break;
      k = m.get().i - 1;
      l -= 1;
    }
    std::reverse(optimal_match_sequence.begin(), optimal_match_sequence.end());
    return optimal_match_sequence;
  };

  for (idx_t k = 0; k < n; ++k) {
    for (const auto & m : matches_by_j[k]) {
      if (m.get().i > 0) {
        for (const auto & item : optimal.m[m.get().i - 1]) {
          auto & l = item.first;
          update(m, l + 1);
        }
      }
      else {
        update(m, 1);
      }
    }
    bruteforce_update(k);
  }
  auto optimal_match_sequence = unwind(n);
  auto optimal_l = optimal_match_sequence.size();

  guesses_t guesses;
  // corner: empty password
  if (password.length() == 0) {
    guesses = 1;
  }
  else {
    guesses = optimal.g[n - 1][optimal_l];
  }

  // retrieve referenced bruteforce matches
  std::vector<std::unique_ptr<Match>> bruteforce_matches;
  for (const auto & ref : optimal_match_sequence) {
    auto & m = ref.get();
    if (m.get_pattern() != MatchPattern::BRUTEFORCE) continue;
    auto it = bruteforces.find(std::make_pair(m.i, m.j));
    if (it == bruteforces.end()) continue;
    bruteforce_matches.push_back(std::move(it->second));
  }

  return {
    password,
    guesses,
    static_cast<guesses_log10_t>(std::log10(guesses)),
    std::move(bruteforce_matches),
    std::move(optimal_match_sequence),
  };
}

// ------------------------------------------------------------------------------
// guess estimation -- one function per match pattern ---------------------------
// ------------------------------------------------------------------------------

guesses_t estimate_guesses(Match & match, const std::string & password) {
  if (match.guesses) return match.guesses; // a match's guess estimate doesn't change. cache it.
  guesses_t min_guesses = 1;
  if (match.token.length() < password.length()) {
    min_guesses = (token_len(match) == 1)
      ? MIN_SUBMATCH_GUESSES_SINGLE_CHAR
      : MIN_SUBMATCH_GUESSES_MULTI_CHAR;
  }
#define MATCH_FN(title, upper, lower) \
  : match.get_pattern() == MatchPattern::upper ? lower##_guesses
  guesses_t (*estimation_function)(const Match &) =
    (false) ? nullptr MATCH_RUN() : nullptr;
#undef MATCH_FN
  assert(estimation_function != nullptr);
  auto guesses = estimation_function(match);
  match.guesses = std::max(guesses, min_guesses);
  match.guesses_log10 = static_cast<guesses_log10_t>(std::log10(match.guesses));
  return match.guesses;
}

guesses_t unknown_guesses(const Match & match) {
  assert(match.guesses);
  return match.guesses;
}

guesses_t bruteforce_guesses(const Match & match) {
  auto guesses = std::pow(BRUTEFORCE_CARDINALITY, token_len(match));
  // small detail: make bruteforce matches at minimum one guess bigger than smallest allowed
  // submatch guesses, such that non-bruteforce submatches over the same [i..j] take precedence.
  auto min_guesses = (token_len(match) == 1)
    ? MIN_SUBMATCH_GUESSES_SINGLE_CHAR + 1
    : MIN_SUBMATCH_GUESSES_MULTI_CHAR + 1;
  return std::max(guesses, min_guesses);
}

guesses_t repeat_guesses(const Match & match) {
  return match.get_repeat().base_guesses * match.get_repeat().repeat_count;
}

guesses_t sequence_guesses(const Match & match) {
  auto second_chr_pos = util::utf8_iter(match.token.begin(), match.token.end());
  auto first_chr = std::string(match.token.begin(), second_chr_pos);
  guesses_t base_guesses;
  // lower guesses for obvious starting points
  if (first_chr == "a" || first_chr == "A" || first_chr == "z" ||
      first_chr == "Z" || first_chr == "0" || first_chr == "1" ||
      first_chr == "9") {
    base_guesses = 4;
  }
  else {
    if (std::regex_match(first_chr, std::regex(R"(\d)"))) {
      base_guesses = 10; // digits
    }
    else {
      // could give a higher base for uppercase,
      // assigning 26 to both upper and lower sequences is more conservative.
      base_guesses = 26;
    }
  }
  if (!match.get_sequence().ascending) {
    // need to try a descending sequence in addition to every ascending sequence ->
    // 2x guesses
    base_guesses *= 2;
  }
  return base_guesses * token_len(match);
}

guesses_t regex_guesses(const Match & match) {
  switch (match.get_regex().regex_tag) {
  case RegexTag::RECENT_YEAR:
  {
    // conservative estimate of year space: num years from REFERENCE_YEAR.
    // if year is close to REFERENCE_YEAR, estimate a year space of MIN_YEAR_SPACE.
    auto pre_year_space = std::stoul(match.get_regex().regex_match.matches[0]);
    if (pre_year_space > REFERENCE_YEAR) {
      pre_year_space -= REFERENCE_YEAR;
    }
    else {
      pre_year_space = REFERENCE_YEAR - pre_year_space;
    }
    guesses_t year_space = pre_year_space;
    year_space = std::max(year_space, MIN_YEAR_SPACE);
    return year_space;
  }
  case RegexTag::ALPHA_LOWER: case RegexTag::ALPHANUMERIC: {
    auto base = [&] {
      switch (match.get_regex().regex_tag) {
      case RegexTag::ALPHA_LOWER: return 26;
      case RegexTag::ALPHANUMERIC: return 62;
      default: assert(false); return 0;
      }
    }();
    return std::pow(base, token_len(match));
  }
  default:
    return 0;
  }
}

guesses_t date_guesses(const Match & match) {
  // base guesses: (year distance from REFERENCE_YEAR) * num_days * num_years
  auto pre_year_space = match.get_date().year;
  if (pre_year_space > REFERENCE_YEAR) {
    pre_year_space -= REFERENCE_YEAR;
  }
  else {
    pre_year_space = REFERENCE_YEAR - pre_year_space;
  }

  guesses_t year_space = pre_year_space;
  year_space = std::max(year_space, MIN_YEAR_SPACE);
  auto guesses = year_space * 365;
  // double for four-digit years
  if (match.get_date().has_full_year) guesses *= 2;
  // add factor of 4 for separator selection (one of ~4 choices)
  if (match.get_date().separator.length()) guesses *= 4;
  return guesses;
}

guesses_t spatial_guesses(const Match & match) {
  std::size_t s;
  guesses_t d;
  if (match.get_spatial().graph == GraphTag::QWERTY ||
      match.get_spatial().graph == GraphTag::DVORAK) {
    s = KEYBOARD_STARTING_POSITIONS;
    d = KEYBOARD_AVERAGE_DEGREE;
  }
  else {
    s = KEYPAD_STARTING_POSITIONS;
    d = KEYPAD_AVERAGE_DEGREE;
  }
  guesses_t guesses = 0;
  auto L = token_len(match);
  auto t = static_cast<decltype(L)>(match.get_spatial().turns);
  // estimate the number of possible patterns w/ length L or less with t turns or less.
  for (decltype(L) i = 2; i <= L; ++i) {
    auto possible_turns = std::min(t, i - 1);
    for (decltype(possible_turns) j = 1; j <= possible_turns; ++j) {
      guesses += nCk(i - 1, j - 1) * s * std::pow(d, j);
    }
  }
  // add extra guesses for shifted keys. (% instead of 5, A instead of a.)
  // math is similar to extra guesses of l33t substitutions in dictionary matches.
  if (match.get_spatial().shifted_count) {
    auto S = match.get_spatial().shifted_count;
    decltype(S) U = token_len(match) - match.get_spatial().shifted_count; // unshifted count
    if (S == 0 || U == 0) {
      guesses *= 2;
    }
    else {
      auto shifted_variations = 0;
      for (decltype(S) i = 1; i <= std::min(S, U); ++i) {
        shifted_variations += nCk(S + U, i);
      }
      guesses *= shifted_variations;
    }
  }

  return guesses;
}

guesses_t dictionary_guesses(const Match & match) {
  auto base_guesses = match.get_dictionary().rank; // keep these as properties for display purposes
  auto uppercase_variations_ = uppercase_variations(match);
  auto l33t_variations_ = l33t_variations(match);
  auto reversed_variations = match.get_dictionary().reversed ? 2 : 1;
  return (base_guesses * uppercase_variations_ * l33t_variations_ * reversed_variations);
}

guesses_t uppercase_variations(const Match & match) {
  auto & word = match.token;
  if (std::regex_match(word, ALL_LOWER()) || !word.size())
    return 1;
  // a capitalized word is the most common capitalization scheme,
  // so it only doubles the search space (uncapitalized + capitalized).
  // allcaps and end-capitalized are common enough too, underestimate as 2x factor to be safe.
  for (const auto& regex : {START_UPPER(), END_UPPER(), ALL_UPPER()}) {
    if (std::regex_match(word, regex)) return 2;
  }
  // otherwise calculate the number of ways to capitalize U+L uppercase+lowercase letters
  // with U uppercase letters or less. or, if there's more uppercase than lower (for eg. PASSwORD),
  // the number of ways to lowercase U+L letters with L lowercase letters or less.
  auto match_chr = [] (const std::string & str, const std::regex & regex) {
    decltype(str.length()) toret = 0;
    for (auto it = str.begin(); it != str.end();) {
      auto it2 = util::utf8_iter(it, str.end());
      auto s = std::string(it, it2);
      if (std::regex_match(s, regex)) {
        toret += 1;
      }
      it = it2;
    }
    return toret;
  };
  auto U = match_chr(word, std::regex(R"([A-Z])"));
  auto L = match_chr(word, std::regex(R"([a-z])"));
  guesses_t variations = 0;
  for (decltype(U) i = 1; i <= std::min(U, L); ++i) {
    variations += nCk(U + L, i);
  }
  return variations;
}

guesses_t l33t_variations(const Match & match) {
  auto & dmatch = match.get_dictionary();
  if (!dmatch.l33t) return 1;
  guesses_t variations = 1;
  for (const auto & item : dmatch.sub) {
    auto & subbed = item.first;
    auto & unsubbed = item.second;
    // lower-case match.token before calculating: capitalization shouldn't affect l33t calc.
    idx_t S = 0, U = 0;
    // XXX: using ascii_lower is okay for now since our
    // sub dictionaries are ascii only
    auto ltoken = util::ascii_lower(match.token);
    for (auto it = ltoken.begin(); it != ltoken.end();) {
      auto it2 = util::utf8_iter(it, ltoken.end());
      auto cs = std::string(it, it2);
      if (cs == subbed) S += 1;
      if (cs == unsubbed) U += 1;
      it = it2;
    }
    if (!S || !U) {
      // for this sub, password is either fully subbed (444) or fully unsubbed (aaa)
      // treat that as doubling the space (attacker needs to try fully subbed chars in addition to
      // unsubbed.)
      variations *= 2;
    }
    else {
      // this case is similar to capitalization:
      // with aa44a, U = 3, S = 2, attacker needs to try unsubbed + one sub + two subs
      auto p = std::min(U, S);
      guesses_t possibilities = 0;
      for (decltype(p) i = 1; i <= p; ++i) {
        possibilities += nCk(U + S, i);
      }
      variations *= possibilities;
    }
  }
  return variations;
}

}
