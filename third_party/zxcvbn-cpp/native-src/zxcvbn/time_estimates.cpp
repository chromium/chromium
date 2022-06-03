#include <zxcvbn/time_estimates.hpp>

#include <zxcvbn/common.hpp>
#include <zxcvbn/util.hpp>

#include <sstream>
#include <vector>
#include <tuple>

#include <cmath>

namespace zxcvbn {

static
std::string display_time(time_t seconds);

static
score_t guesses_to_score(guesses_t guesses);

AttackTimes estimate_attack_times(guesses_t guesses) {
  AttackTimes toret;

#define SET_CRACK_TIME(a, val)                                          \
  do {                                                                  \
    toret.crack_times_seconds.a = val;                                   \
    toret.crack_times_display.a = display_time(toret.crack_times_seconds.a); \
  }                                                                     \
  while (false)

  SET_CRACK_TIME(online_throttling_100_per_hour, guesses / (100.0 / 3600));
  SET_CRACK_TIME(online_no_throttling_10_per_second, guesses / 10);
  SET_CRACK_TIME(offline_slow_hashing_1e4_per_second, guesses / 1e4);
  SET_CRACK_TIME(offline_fast_hashing_1e10_per_second, guesses / 1e10);

#undef SET_CRACK_TIME

  toret.score = guesses_to_score(guesses);

  return toret;
}

static
score_t guesses_to_score(guesses_t guesses) {
  auto DELTA = 5;
  if (guesses < 1e3 + DELTA) {
    // risky password: "too guessable"
    return 0;
  }
  else if (guesses < 1e6 + DELTA) {
    // modest protection from throttled online attacks: "very guessable"
    return 1;
  }
  else if (guesses < 1e8 + DELTA) {
    // modest protection from unthrottled online attacks: "somewhat guessable"
    return 2;
  }
  else if (guesses < 1e10 + DELTA) {
    // modest protection from offline attacks: "safely unguessable"
    //  assuming a salted, slow hash function like bcrypt, scrypt, PBKDF2, argon, etc
    return 3;
  }
  else {
    // strong protection from offline attacks under same scenario: "very unguessable"
    return 4;
  }
}

static
std::string display_time(time_t seconds) {
  auto minute = static_cast<time_t>(60);
  auto hour = minute * 60;
  auto day = hour * 24;
  auto month = day * 31;
  auto year = month * 12;
  auto century = year * 100;

  time_t display_num;
  std::string display_str;

  std::tie(display_num, display_str) = [&] () -> std::pair<time_t, std::string> {
    if (seconds < 1) {
      return {0, "less than a second"};
    }
    if (seconds < minute) {
      auto base = util::round_div(seconds, 1);
      return {base, "second"};
    }
    else if (seconds < hour) {
      auto base = util::round_div(seconds, minute);
      return {base, "minute"};
    }
    else if (seconds < day) {
      auto base = util::round_div(seconds, hour);
      return {base, "hour"};
    }
    else if (seconds < month) {
      auto base = util::round_div(seconds, day);
      return {base, "day"};
    }
    else if (seconds < year) {
      auto base = util::round_div(seconds, month);
      return {base, "month"};
    }
    else if (seconds < century) {
      auto base = util::round_div(seconds, year);
      return {base, "year"};
    }
    else {
      return {0, "centuries"};
    }
  }();

  if (display_num) {
    std::ostringstream os;
    os << display_num << " " << display_str;
    display_str = os.str();

    if (display_num != 1) {
      display_str += "s";
    }
  }


  return display_str;
}

}
