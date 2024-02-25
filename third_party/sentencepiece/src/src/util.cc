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

#include "util.h"

#include <atomic>
#include <iostream>

namespace sentencepiece {

namespace {
constexpr unsigned int kDefaultSeed = static_cast<unsigned int>(-1);
static std::atomic<unsigned int> g_seed = kDefaultSeed;
static std::atomic<int> g_minloglevel = 0;
}  // namespace

void SetRandomGeneratorSeed(unsigned int seed) {
  if (seed != kDefaultSeed) {
    g_seed.store(seed);
  }
}

uint32 GetRandomGeneratorSeed() {
#if !SENTENCEPIECE_DISABLE_EXCEPTIONS
  try {
#endif
    return g_seed == kDefaultSeed ? std::random_device{}() : g_seed.load();
#if !SENTENCEPIECE_DISABLE_EXCEPTIONS
  } catch (...) {
    return g_seed.load();
  }
#endif
}

namespace logging {
int GetMinLogLevel() {
  return g_minloglevel.load();
}
void SetMinLogLevel(int v) {
  g_minloglevel.store(v);
}
}  // namespace logging

namespace string_util {

// mblen sotres the number of bytes consumed after decoding.
char32 DecodeUTF8(const char *begin, const char *end, size_t *mblen) {
  const size_t len = end - begin;

  if (static_cast<unsigned char>(begin[0]) < 0x80) {
    *mblen = 1;
    return static_cast<unsigned char>(begin[0]);
  } else if (len >= 2 && (begin[0] & 0xE0) == 0xC0) {
    const char32 cp = (((begin[0] & 0x1F) << 6) | ((begin[1] & 0x3F)));
    if (IsTrailByte(begin[1]) && cp >= 0x0080 && IsValidCodepoint(cp)) {
      *mblen = 2;
      return cp;
    }
  } else if (len >= 3 && (begin[0] & 0xF0) == 0xE0) {
    const char32 cp = (((begin[0] & 0x0F) << 12) | ((begin[1] & 0x3F) << 6) |
                       ((begin[2] & 0x3F)));
    if (IsTrailByte(begin[1]) && IsTrailByte(begin[2]) && cp >= 0x0800 &&
        IsValidCodepoint(cp)) {
      *mblen = 3;
      return cp;
    }
  } else if (len >= 4 && (begin[0] & 0xf8) == 0xF0) {
    const char32 cp = (((begin[0] & 0x07) << 18) | ((begin[1] & 0x3F) << 12) |
                       ((begin[2] & 0x3F) << 6) | ((begin[3] & 0x3F)));
    if (IsTrailByte(begin[1]) && IsTrailByte(begin[2]) &&
        IsTrailByte(begin[3]) && cp >= 0x10000 && IsValidCodepoint(cp)) {
      *mblen = 4;
      return cp;
    }
  }

  // Invalid UTF-8.
  *mblen = 1;
  return kUnicodeError;
}

bool IsStructurallyValid(absl::string_view str) {
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t mblen = 0;
  while (begin < end) {
    const char32 c = DecodeUTF8(begin, end, &mblen);
    if (c == kUnicodeError && mblen != 3) return false;
    if (!IsValidCodepoint(c)) return false;
    begin += mblen;
  }
  return true;
}

size_t EncodeUTF8(char32 c, char *output) {
  if (c <= 0x7F) {
    *output = static_cast<char>(c);
    return 1;
  }

  if (c <= 0x7FF) {
    output[1] = 0x80 | (c & 0x3F);
    c >>= 6;
    output[0] = 0xC0 | c;
    return 2;
  }

  // if `c` is out-of-range, convert it to REPLACEMENT CHARACTER (U+FFFD).
  // This treatment is the same as the original runetochar.
  if (c > 0x10FFFF) c = kUnicodeError;

  if (c <= 0xFFFF) {
    output[2] = 0x80 | (c & 0x3F);
    c >>= 6;
    output[1] = 0x80 | (c & 0x3F);
    c >>= 6;
    output[0] = 0xE0 | c;
    return 3;
  }

  output[3] = 0x80 | (c & 0x3F);
  c >>= 6;
  output[2] = 0x80 | (c & 0x3F);
  c >>= 6;
  output[1] = 0x80 | (c & 0x3F);
  c >>= 6;
  output[0] = 0xF0 | c;

  return 4;
}

std::string UnicodeCharToUTF8(const char32 c) { return UnicodeTextToUTF8({c}); }

UnicodeText UTF8ToUnicodeText(absl::string_view utf8) {
  UnicodeText uc;
  const char *begin = utf8.data();
  const char *end = utf8.data() + utf8.size();
  while (begin < end) {
    size_t mblen;
    const char32 c = DecodeUTF8(begin, end, &mblen);
    uc.push_back(c);
    begin += mblen;
  }
  return uc;
}

std::string UnicodeTextToUTF8(const UnicodeText &utext) {
  char buf[8];
  std::string result;
  for (const char32 c : utext) {
    const size_t mblen = EncodeUTF8(c, buf);
    result.append(buf, mblen);
  }
  return result;
}
}  // namespace string_util

namespace random {
#ifdef SPM_NO_THREADLOCAL
namespace {
class RandomGeneratorStorage {
 public:
  RandomGeneratorStorage() {
    pthread_key_create(&key_, &RandomGeneratorStorage::Delete);
  }
  virtual ~RandomGeneratorStorage() { pthread_key_delete(key_); }

  std::mt19937 *Get() {
    auto *result = static_cast<std::mt19937 *>(pthread_getspecific(key_));
    if (result == nullptr) {
      result = new std::mt19937(GetRandomGeneratorSeed());
      pthread_setspecific(key_, result);
    }
    return result;
  }

 private:
  static void Delete(void *value) { delete static_cast<std::mt19937 *>(value); }
  pthread_key_t key_;
};
}  // namespace

std::mt19937 *GetRandomGenerator() {
  static RandomGeneratorStorage *storage = new RandomGeneratorStorage;
  return storage->Get();
}
#else
std::mt19937 *GetRandomGenerator() {
  thread_local static std::mt19937 mt(GetRandomGeneratorSeed());
  return &mt;
}
#endif
}  // namespace random

namespace util {

std::string StrError(int errnum) {
  constexpr int kStrErrorSize = 1024;
  char buffer[kStrErrorSize];
  char *str = nullptr;
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
  str = strerror_r(errnum, buffer, kStrErrorSize - 1);
#elif defined(_WIN32)
  strerror_s(buffer, kStrErrorSize - 1, errnum);
  str = buffer;
#else
  strerror_r(errnum, buffer, kStrErrorSize - 1);
  str = buffer;
#endif
  std::ostringstream os;
  os << str << " Error #" << errnum;
  return os.str();
}

std::vector<std::string> StrSplitAsCSV(absl::string_view text) {
  std::string buf = std::string(text);
  char* str = const_cast<char*>(buf.data());
  char* eos = str + text.size();
  char* start = nullptr;
  char* end = nullptr;

  std::vector<std::string> result;
  for (; str < eos; ++str) {
    if (*str == '"') {
      start = ++str;
      end = start;
      for (; str < eos; ++str) {
        if (*str == '"') {
          str++;
          if (*str != '"') {
            break;
          }
        }
        *end++ = *str;
      }
      str = std::find(str, eos, ',');
    } else {
      start = str;
      str = std::find(str, eos, ',');
      end = str;
    }
    *end = '\0';
    result.push_back(start);
  }

  return result;
}

#ifdef OS_WIN
std::wstring Utf8ToWide(absl::string_view input) {
  int output_length = ::MultiByteToWideChar(
      CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
  if (output_length == 0) {
    return L"";
  }
  std::wstring output(output_length, 0);
  const int result = ::MultiByteToWideChar(CP_UTF8, 0, input.data(),
                                           static_cast<int>(input.size()),
                                           output.data(), output.size());
  return result == output_length ? output : L"";
}
#endif
}  // namespace util
}  // namespace sentencepiece
