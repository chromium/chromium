#include <zxcvbn/util.hpp>

#include <algorithm>
#include <string>
#include <utility>

#include <cassert>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"

namespace zxcvbn {

namespace util {

bool utf8_valid(std::string::const_iterator start,
                std::string::const_iterator end) {
  return base::IsStringUTF8(base::MakeStringPiece(start, end));
}

bool utf8_valid(const std::string & str) {
  return utf8_valid(str.begin(), str.end());
}

std::string ascii_lower(const std::string & in) {
  return base::ToLowerASCII(in);
}

std::string reverse_string(const std::string & in) {
  if (!utf8_valid(in))
    return std::string(in.rbegin(), in.rend());

  std::wstring ret = base::UTF8ToWide(in);
  std::reverse(ret.begin(), ret.end());
  return base::WideToUTF8(ret);
}

template<class It>
std::pair<char32_t, It> _utf8_decode(It it, It end) {
  assert(it != end);
  const char* src = &*it;
  size_t src_len = static_cast<size_t>(std::distance(it, end));
  size_t char_index = 0;
  base_icu::UChar32 code_point_out;

  base::ReadUnicodeCharacter(src, src_len, &char_index, &code_point_out);
  return {code_point_out, it + ++char_index};
}


template<class It>
It _utf8_iter(It start, It end) {
  return _utf8_decode(start, end).second;
}

std::string::iterator utf8_iter(std::string::iterator start,
                                std::string::iterator end) {
  return _utf8_iter(start, end);
}

std::string::const_iterator utf8_iter(std::string::const_iterator start,
                                      std::string::const_iterator end) {
  return _utf8_iter(start, end);
}

std::string::size_type character_len(const std::string & str,
                                     std::string::size_type start,
                                     std::string::size_type end) {
  assert(utf8_valid(str.begin() + start, str.begin() + end));

  std::string::size_type clen = 0;
  for (auto it = str.begin() + start;
        it != str.begin() + end;
        it = utf8_iter(it, str.begin() + end)) {
    clen += 1;
  }
  return clen;
}

std::string::size_type character_len(const std::string & str) {
  return character_len(str, 0, str.size());
}

char32_t utf8_decode(const std::string & start,
                     std::string::size_type & idx) {
  auto ret = _utf8_decode(start.begin() + idx, start.end());
  idx += ret.second - (start.begin() + idx);
  return ret.first;
}

}

}
