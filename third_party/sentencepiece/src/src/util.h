// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#ifndef UTIL_H_
#define UTIL_H_

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "src/common.h"
#include "src/sentencepiece_processor.h"
#include "src/deps/threadpool.h"
#include "src/deps/status_builder.h"

#ifdef SPM_NO_THREADLOCAL
#include <pthread.h>
#endif

#define GTL_LOC (0)

#define CHECK_OR_RETURN(condition)                                 \
  if (condition) {                                                 \
  } else /* NOLINT */                                              \
    return ::util::StatusBuilder(::util::error::INTERNAL, GTL_LOC) \
           << __FILE__ << "(" << __LINE__ << ") [" << #condition << "] "

#define CHECK_EQ_OR_RETURN(a, b) CHECK_OR_RETURN((a) == (b))
#define CHECK_NE_OR_RETURN(a, b) CHECK_OR_RETURN((a) != (b))
#define CHECK_GE_OR_RETURN(a, b) CHECK_OR_RETURN((a) >= (b))
#define CHECK_LE_OR_RETURN(a, b) CHECK_OR_RETURN((a) <= (b))
#define CHECK_GT_OR_RETURN(a, b) CHECK_OR_RETURN((a) > (b))
#define CHECK_LT_OR_RETURN(a, b) CHECK_OR_RETURN((a) < (b))

namespace sentencepiece {

static constexpr uint32 kUnicodeError = 0xFFFD;

template <typename T>
std::ostream &operator<<(std::ostream &out, const std::vector<T> &v) {
  for (const auto n : v) {
    out << " " << n;
  }
  return out;
}

// String utilities
namespace string_util {

struct string_view_hash {
  // DJB hash function.
  inline size_t operator()(const absl::string_view &sp) const {
    size_t hash = 5381;
    for (size_t i = 0; i < sp.size(); ++i) {
      hash = ((hash << 5) + hash) + sp[i];
    }
    return hash;
  }
};

template <typename Target>
inline bool lexical_cast(absl::string_view arg, Target *result) {
  std::stringstream ss;
  return (ss << arg.data() && ss >> *result);
}

template <>
inline bool lexical_cast(absl::string_view arg, bool *result) {
  const char *kTrue[] = {"1", "t", "true", "y", "yes"};
  const char *kFalse[] = {"0", "f", "false", "n", "no"};
  std::string lower_value = std::string(arg);
  std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(),
                 ::tolower);
  for (size_t i = 0; i < 5; ++i) {
    if (lower_value == kTrue[i]) {
      *result = true;
      return true;
    } else if (lower_value == kFalse[i]) {
      *result = false;
      return true;
    }
  }

  return false;
}

template <>
inline bool lexical_cast(absl::string_view arg, std::string *result) {
  *result = std::string(arg);
  return true;
}

template <typename T>
inline bool DecodePOD(absl::string_view str, T *result) {
  if (sizeof(*result) != str.size()) {
    return false;
  }
  memcpy(result, str.data(), sizeof(T));
  return true;
}

template <typename T>
inline std::string EncodePOD(const T &value) {
  std::string s;
  s.resize(sizeof(T));
  memcpy(const_cast<char *>(s.data()), &value, sizeof(T));
  return s;
}

template <typename T>
inline std::string IntToHex(T value) {
  std::ostringstream os;
  os << std::hex << std::uppercase << value;
  return os.str();
}

template <typename T>
inline T HexToInt(absl::string_view value) {
  T n;
  std::istringstream is(value.data());
  is >> std::hex >> n;
  return n;
}

template <typename T>
inline size_t Itoa(T val, char *s) {
  char *org = s;

  if (val < 0) {
    *s++ = '-';
    val = -val;
  }
  char *t = s;

  T mod = 0;
  while (val) {
    mod = val % 10;
    *t++ = static_cast<char>(mod) + '0';
    val /= 10;
  }

  if (s == t) {
    *t++ = '0';
  }

  *t = '\0';
  std::reverse(s, t);
  return static_cast<size_t>(t - org);
}

template <typename T>
std::string SimpleItoa(T val) {
  char buf[32];
  Itoa<T>(val, buf);
  return std::string(buf);
}

// Return length of a single UTF-8 source character
inline size_t OneCharLen(const char *src) {
  return "\1\1\1\1\1\1\1\1\1\1\1\1\2\2\3\4"[(*src & 0xFF) >> 4];
}

// Return (x & 0xC0) == 0x80;
// Since trail bytes are always in [0x80, 0xBF], we can optimize:
inline bool IsTrailByte(char x) { return static_cast<signed char>(x) < -0x40; }

inline bool IsValidCodepoint(char32 c) {
  return (static_cast<uint32>(c) < 0xD800) || (c >= 0xE000 && c <= 0x10FFFF);
}

bool IsStructurallyValid(absl::string_view str);

using UnicodeText = std::vector<char32>;

char32 DecodeUTF8(const char *begin, const char *end, size_t *mblen);

inline char32 DecodeUTF8(absl::string_view input, size_t *mblen) {
  return DecodeUTF8(input.data(), input.data() + input.size(), mblen);
}

inline bool IsValidDecodeUTF8(absl::string_view input, size_t *mblen) {
  const char32 c = DecodeUTF8(input, mblen);
  return c != kUnicodeError || *mblen == 3;
}

size_t EncodeUTF8(char32 c, char *output);

std::string UnicodeCharToUTF8(const char32 c);

UnicodeText UTF8ToUnicodeText(absl::string_view utf8);

std::string UnicodeTextToUTF8(const UnicodeText &utext);

}  // namespace string_util

// other map/ptr utilties
namespace port {

template <class Collection, class Key>
bool ContainsKey(const Collection &collection, const Key &key) {
  return collection.find(key) != collection.end();
}

template <class Collection>
const typename Collection::value_type::second_type &FindOrDie(
    const Collection &collection,
    const typename Collection::value_type::first_type &key) {
  typename Collection::const_iterator it = collection.find(key);
  CHECK(it != collection.end()) << "Map key not found: " << key;
  return it->second;
}

template <class Collection>
const typename Collection::value_type::second_type &FindWithDefault(
    const Collection &collection,
    const typename Collection::value_type::first_type &key,
    const typename Collection::value_type::second_type &value) {
  typename Collection::const_iterator it = collection.find(key);
  if (it == collection.end()) {
    return value;
  }
  return it->second;
}

template <class Collection>
bool InsertIfNotPresent(Collection *const collection,
                        const typename Collection::value_type &vt) {
  return collection->insert(vt).second;
}

template <class Collection>
bool InsertIfNotPresent(
    Collection *const collection,
    const typename Collection::value_type::first_type &key,
    const typename Collection::value_type::second_type &value) {
  return InsertIfNotPresent(collection,
                            typename Collection::value_type(key, value));
}

template <class Collection>
void InsertOrDie(Collection *const collection,
                 const typename Collection::value_type::first_type &key,
                 const typename Collection::value_type::second_type &data) {
  CHECK(InsertIfNotPresent(collection, key, data)) << "duplicate key";
}

// hash
inline void mix(uint64 &a, uint64 &b, uint64 &c) {  // 64bit version
  a -= b;
  a -= c;
  a ^= (c >> 43);
  b -= c;
  b -= a;
  b ^= (a << 9);
  c -= a;
  c -= b;
  c ^= (b >> 8);
  a -= b;
  a -= c;
  a ^= (c >> 38);
  b -= c;
  b -= a;
  b ^= (a << 23);
  c -= a;
  c -= b;
  c ^= (b >> 5);
  a -= b;
  a -= c;
  a ^= (c >> 35);
  b -= c;
  b -= a;
  b ^= (a << 49);
  c -= a;
  c -= b;
  c ^= (b >> 11);
  a -= b;
  a -= c;
  a ^= (c >> 12);
  b -= c;
  b -= a;
  b ^= (a << 18);
  c -= a;
  c -= b;
  c ^= (b >> 22);
}

inline uint64 FingerprintCat(uint64 x, uint64 y) {
  uint64 b = 0xe08c1d668b756f82;  // more of the golden ratio
  mix(x, b, y);
  return y;
}

template <typename T>
void STLDeleteElements(std::vector<T *> *vec) {
  for (auto item : *vec) {
    delete item;
  }
  vec->clear();
}
}  // namespace port

namespace random {

std::mt19937 *GetRandomGenerator();

template <typename T>
class ReservoirSampler {
 public:
  explicit ReservoirSampler(std::vector<T> *sampled, size_t size)
      : sampled_(sampled), size_(size), engine_(std::random_device{}()) {}
  explicit ReservoirSampler(std::vector<T> *sampled, size_t size, size_t seed)
      : sampled_(sampled), size_(size), engine_(seed) {}
  virtual ~ReservoirSampler() {}

  void Add(const T &item) {
    if (size_ == 0) return;

    ++total_;
    if (sampled_->size() < size_) {
      sampled_->push_back(item);
    } else {
      std::uniform_int_distribution<size_t> dist(0, total_ - 1);
      const size_t n = dist(engine_);
      if (n < sampled_->size()) (*sampled_)[n] = item;
    }
  }

  size_t total_size() const { return total_; }

 private:
  std::vector<T> *sampled_ = nullptr;
  size_t size_ = 0;
  size_t total_ = 0;
  std::mt19937 engine_;
};

}  // namespace random

namespace util {

inline std::string JoinPath(absl::string_view path) {
  return std::string(path.data(), path.size());
}

template <typename... T>
inline std::string JoinPath(absl::string_view first, const T &... rest) {
#ifdef OS_WIN
  return JoinPath(first) + "\\" + JoinPath(rest...);
#else
  return JoinPath(first) + "/" + JoinPath(rest...);
#endif
}

std::string StrError(int errnum);
}  // namespace util
}  // namespace sentencepiece

#endif  // UTIL_H_
