#ifndef __ZXCVBN__TIME_ESTIMATES_HPP
#define __ZXCVBN__TIME_ESTIMATES_HPP

#include <zxcvbn/common.hpp>

#include <string>

namespace zxcvbn {

using time_t = double;

struct AttackTimes {
  struct {
    time_t online_throttling_100_per_hour;
    time_t online_no_throttling_10_per_second;
    time_t offline_slow_hashing_1e4_per_second;
    time_t offline_fast_hashing_1e10_per_second;
  } crack_times_seconds;

  struct {
    std::string online_throttling_100_per_hour;
    std::string online_no_throttling_10_per_second;
    std::string offline_slow_hashing_1e4_per_second;
    std::string offline_fast_hashing_1e10_per_second;
  } crack_times_display;

  score_t score;
};

AttackTimes estimate_attack_times(guesses_t guesses);

}

#endif
