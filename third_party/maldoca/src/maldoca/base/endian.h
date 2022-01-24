// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MALDOCA_BASE_ENDIAN_H_
#define MALDOCA_BASE_ENDIAN_H_

#include <cstdint>
#include <cstring>

#ifdef _MSC_VER
#include <stdlib.h>
#elif defined(__FreeBSD__)
#include <sys/endian.h>
#elif defined(__GLIBC__)
#include <byteswap.h>
#endif

#include "absl/base/config.h"

namespace maldoca {
namespace {
template <typename T>
inline T UnalignedLoad(const void* p) {
  T t;
  memcpy(&t, p, sizeof t);
  return t;
}
template <typename T>
inline void UnalignedStore(void* p, T t) {
  memcpy(p, &t, sizeof t);
}
// Use compiler byte-swapping intrinsics if they are available.  32-bit
// and 64-bit versions are available in Clang and GCC as of GCC 4.3.0.
// The 16-bit version is available in Clang and GCC only as of GCC 4.8.0.
// For simplicity, we enable them all only for GCC 4.8.0 or later.
#if defined(__clang__) || \
    (defined(__GNUC__) && \
     ((__GNUC__ == 4 && __GNUC_MINOR__ >= 8) || __GNUC__ >= 5))
inline uint64_t gbswap_64(uint64_t host_int) {
  return __builtin_bswap64(host_int);
}
inline uint32_t gbswap_32(uint32_t host_int) {
  return __builtin_bswap32(host_int);
}
inline uint16_t gbswap_16(uint16_t host_int) {
  return __builtin_bswap16(host_int);
}

#elif defined(_MSC_VER)
inline uint64_t gbswap_64(uint64_t host_int) {
  return _byteswap_uint64(host_int);
}
inline uint32_t gbswap_32(uint32_t host_int) {
  return _byteswap_ulong(host_int);
}
inline uint16_t gbswap_16(uint16_t host_int) {
  return _byteswap_ushort(host_int);
}

#else
inline uint64_t gbswap_64(uint64_t host_int) {
#if defined(__GNUC__) && defined(__x86_64__) && !defined(__APPLE__)
  // Adapted from /usr/include/byteswap.h.  Not available on Mac.
  if (__builtin_constant_p(host_int)) {
    return __bswap_constant_64(host_int);
  } else {
    uint64_t result;
    __asm__("bswap %0" : "=r"(result) : "0"(host_int));
    return result;
  }
#elif defined(__GLIBC__)
  return bswap_64(host_int);
#else
  return (((host_int & uint64_t{0xFF}) << 56) |
          ((host_int & uint64_t{0xFF00}) << 40) |
          ((host_int & uint64_t{0xFF0000}) << 24) |
          ((host_int & uint64_t{0xFF000000}) << 8) |
          ((host_int & uint64_t{0xFF00000000}) >> 8) |
          ((host_int & uint64_t{0xFF0000000000}) >> 24) |
          ((host_int & uint64_t{0xFF000000000000}) >> 40) |
          ((host_int & uint64_t{0xFF00000000000000}) >> 56));
#endif  // bswap_64
}

inline uint32_t gbswap_32(uint32_t host_int) {
#if defined(__GLIBC__)
  return bswap_32(host_int);
#else
  return (((host_int & uint32_t{0xFF}) << 24) |
          ((host_int & uint32_t{0xFF00}) << 8) |
          ((host_int & uint32_t{0xFF0000}) >> 8) |
          ((host_int & uint32_t{0xFF000000}) >> 24));
#endif
}

inline uint16_t gbswap_16(uint16_t host_int) {
#if defined(__GLIBC__)
  return bswap_16(host_int);
#else
  return (((host_int & uint16_t{0xFF}) << 8) |
          ((host_int & uint16_t{0xFF00}) >> 8));
#endif
}

#endif  // intrinsics available
}  // namespace

namespace little_endian {

#ifdef ABSL_IS_LITTLE_ENDIAN

inline uint16_t FromHost16(uint16_t x) { return x; }
inline uint16_t ToHost16(uint16_t x) { return x; }

inline uint32_t FromHost32(uint32_t x) { return x; }
inline uint32_t ToHost32(uint32_t x) { return x; }

inline uint64_t FromHost64(uint64_t x) { return x; }
inline uint64_t ToHost64(uint64_t x) { return x; }

inline constexpr bool IsLittleEndian() { return true; }

#elif defined ABSL_IS_BIG_ENDIAN

inline uint16_t FromHost16(uint16_t x) { return gbswap_16(x); }
inline uint16_t ToHost16(uint16_t x) { return gbswap_16(x); }

inline uint32_t FromHost32(uint32_t x) { return gbswap_32(x); }
inline uint32_t ToHost32(uint32_t x) { return gbswap_32(x); }

inline uint64_t FromHost64(uint64_t x) { return gbswap_64(x); }
inline uint64_t ToHost64(uint64_t x) { return gbswap_64(x); }

inline constexpr bool IsLittleEndian() { return false; }

#else
#error \
    "Unsupported byte order: Either ABSL_IS_BIG_ENDIAN or " \
       "ABSL_IS_LITTLE_ENDIAN must be defined"
#endif  // byte order

inline uint16_t Load16(const void* p) {
  return ToHost16(UnalignedLoad<uint16_t>(p));
}

inline void Store16(void* p, uint16_t v) {
  UnalignedStore<uint16_t>(p, FromHost16(v));
}

inline uint32_t Load32(const void* p) {
  return ToHost32(UnalignedLoad<uint32_t>(p));
}

inline void Store32(void* p, uint32_t v) {
  UnalignedStore<uint32_t>(p, FromHost32(v));
}

inline uint64_t Load64(const void* p) {
  return ToHost64(UnalignedLoad<uint64_t>(p));
}

inline void Store64(void* p, uint64_t v) {
  UnalignedStore<uint64_t>(p, FromHost64(v));
}

}  // namespace little_endian

namespace big_endian {
#ifdef ABSL_IS_LITTLE_ENDIAN

inline uint16_t FromHost16(uint16_t x) { return gbswap_16(x); }
inline uint16_t ToHost16(uint16_t x) { return gbswap_16(x); }

inline uint32_t FromHost32(uint32_t x) { return gbswap_32(x); }
inline uint32_t ToHost32(uint32_t x) { return gbswap_32(x); }

inline uint64_t FromHost64(uint64_t x) { return gbswap_64(x); }
inline uint64_t ToHost64(uint64_t x) { return gbswap_64(x); }

inline constexpr bool IsLittleEndian() { return true; }

#elif defined ABSL_IS_BIG_ENDIAN

inline uint16_t FromHost16(uint16_t x) { return x; }
inline uint16_t ToHost16(uint16_t x) { return x; }

inline uint32_t FromHost32(uint32_t x) { return x; }
inline uint32_t ToHost32(uint32_t x) { return x; }

inline uint64_t FromHost64(uint64_t x) { return x; }
inline uint64_t ToHost64(uint64_t x) { return x; }

inline constexpr bool IsLittleEndian() { return false; }

#endif /* ENDIAN */

inline uint16_t Load16(const void* p) {
  return ToHost16(UnalignedLoad<uint16_t>(p));
}

inline void Store16(void* p, uint16_t v) {
  UnalignedStore<uint16_t>(p, FromHost16(v));
}

inline uint32_t Load32(const void* p) {
  return ToHost32(UnalignedLoad<uint32_t>(p));
}

inline void Store32(void* p, uint32_t v) {
  UnalignedStore<uint32_t>(p, FromHost32(v));
}

inline uint64_t Load64(const void* p) {
  return ToHost64(UnalignedLoad<uint64_t>(p));
}

inline void Store64(void* p, uint64_t v) {
  UnalignedStore<uint64_t>(p, FromHost64(v));
}
}  // namespace big_endian

namespace utils {
// Wrap absl::big_endium namespace static function as a class.
class BigEndian {
 public:
  static inline uint64_t Load16(const void* p) {
    return ::maldoca::big_endian::Load16(p);
  }
  static inline void Store16(void* p, uint16_t v) {
    ::maldoca::big_endian::Store16(p, v);
  }
  static inline uint32_t Load32(const void* p) {
    return ::maldoca::big_endian::Load32(p);
  }
  static inline void Store32(void* p, uint32_t v) {
    ::maldoca::big_endian::Store32(p, v);
  }
  static inline uint64_t Load64(const void* p) {
    return ::maldoca::big_endian::Load64(p);
  }
  static inline void Store64(void* p, uint64_t v) {
    ::maldoca::big_endian::Store64(p, v);
  }
};

// Wrap absl::little_endium namespace static function as a class.
class LittleEndian {
 public:
  static inline uint64_t Load16(const void* p) {
    return ::maldoca::little_endian::Load16(p);
  }
  static inline void Store16(void* p, uint16_t v) {
    ::maldoca::little_endian::Store16(p, v);
  }
  static inline uint32_t Load32(const void* p) {
    return ::maldoca::little_endian::Load32(p);
  }
  static inline void Store32(void* p, uint32_t v) {
    ::maldoca::little_endian::Store32(p, v);
  }
  static inline uint64_t Load64(const void* p) {
    return ::maldoca::little_endian::Load64(p);
  }
  static inline void Store64(void* p, uint64_t v) {
    ::maldoca::little_endian::Store64(p, v);
  }
};
}  // namespace utils
}  // namespace maldoca

#endif  // MALDOCA_BASE_ENDIAN_H_
