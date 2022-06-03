#ifndef __ZXCVBN__UTIL_HPP
#define __ZXCVBN__UTIL_HPP

#include <string>
#include <type_traits>

#include <cmath>

namespace zxcvbn {

namespace util {

template<typename T, typename T2>
constexpr auto round_div(T a, T2 b)
  -> std::enable_if_t<std::is_integral<decltype(a / b)>::value, long> {
  return (a + (a / 2)) / b;
}

template<typename T, typename T2>
constexpr auto round_div(T a, T2 b)
  -> std::enable_if_t<!std::is_integral<decltype(a / b)>::value, long> {
  return std::lround(a / b);
}

std::string ascii_lower(const std::string &);
std::string reverse_string(const std::string &);
std::string::size_type character_len(const std::string &,
                                     std::string::size_type start,
                                     std::string::size_type end) __attribute__((pure));
std::string::size_type character_len(const std::string &)  __attribute__((pure));

char32_t utf8_decode(const std::string & start,
                     std::string::size_type & idx);

std::string::iterator utf8_iter(std::string::iterator start,
                                std::string::iterator end);
std::string::const_iterator utf8_iter(std::string::const_iterator start,
                                      std::string::const_iterator end);



}

}

#endif
