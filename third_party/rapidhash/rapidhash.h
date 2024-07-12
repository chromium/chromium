/*
 * rapidhash - Very fast, high quality, platform-independent hashing algorithm.
 * Copyright (C) 2024 Nicolas De Carli
 *
 * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - rapidhash source repository: https://github.com/Nicoshev/rapidhash
 */

#ifndef _THIRD_PARTY_RAPIDHASH_H
#define _THIRD_PARTY_RAPIDHASH_H 1

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <tuple>
#include <utility>

#include "base/compiler_specific.h"

/*
 *  Likely and unlikely macros.
 */
#define _likely_(x) __builtin_expect(x, 1)
#define _unlikely_(x) __builtin_expect(x, 0)

/*
 *  Default seed.
 */
static constexpr uint64_t RAPID_SEED = 0xbdd89aa982704029ull;

// Default secret parameters. If we wanted to, we could generate our own
// versions of these at renderer startup in order to perturb the hash
// and make it more DoS-resistant (similar to what base/hash.h does),
// but generating new ones takes a little bit of time (~200 µs on a desktop
// machine as of 2024), and good-quality random numbers may not be copious
// from within the sandbox. The secret concept is inherited from wyhash,
// described by the wyhash author here:
//
//   https://github.com/wangyi-fudan/wyhash/issues/139
//
// The rules are:
//
//   1. Each byte must be “balanced”, i.e., have exactly 4 bits set.
//      (This is trivially done by just precompting a LUT with the
//      possible bytes and picking from those.)
//
//   2. Each 64-bit group should have a Hamming distance of 32 to
//      all the others (i.e., popcount(secret[i] ^ secret[j]) == 32).
//      This is just done by rejection sampling.
//
//   3. Each 64-bit group should be prime. It's not obvious that this
//      is really needed for the hash, as opposed to wyrand which also
//      uses the same secret, but according to the author, it is
//      “a feeling to be perfect”. This naturally means the last byte
//      cannot be divisible by 2, but apart from that, is easiest
//      checked by testing a few small factors and then the Miller-Rabin
//      test, which rejects nearly all bad candidates in the first iteration
//      and is fast as long as we have 64x64 -> 128 bit muls and modulos.
//
// For now, we just use the rapidhash-supplied standard.
static constexpr uint64_t rapid_secret[3] = {
    0x2d358dccaa6c78a5ull, 0x8bb84b93962eacc9ull, 0x4b33a62ed433d4a3ull};

/*
 *  64*64 -> 128bit multiply function.
 *
 *  @param A  Address of 64-bit number.
 *  @param B  Address of 64-bit number.
 *
 *  Calculates 128-bit C = *A * *B.
 */
inline std::pair<uint64_t, uint64_t> rapid_mul128(uint64_t A, uint64_t B) {
#if defined(__SIZEOF_INT128__)
  __uint128_t r = A;
  r *= B;
  return {static_cast<uint64_t>(r), static_cast<uint64_t>(r >> 64)};
#else
  // High and low 32 bits of A and B.
  uint64_t a_high = A >> 32, b_high = B >> 32, a_low = (uint32_t)A,
           b_low = (uint32_t)B;

  // Intermediate products.
  uint64_t result_high = a_high * b_high;
  uint64_t result_m0 = a_high * b_low;
  uint64_t result_m1 = b_high * a_low;
  uint64_t result_low = a_low * b_low;

  // Final sum. The lower 64-bit addition can overflow twice,
  // so accumulate carry as we go.
  uint64_t high = result_high + (result_m0 >> 32) + (result_m1 >> 32);
  uint64_t t = result_low + (result_m0 << 32);
  high += (t < result_low);  // Carry.
  uint64_t low = t + (result_m1 << 32);
  high += (low < t);  // Carry.

  return {low, high};
#endif
}

/*
 *  Multiply and xor mix function.
 *
 *  @param A  64-bit number.
 *  @param B  64-bit number.
 *
 *  Calculates 128-bit C = A * B.
 *  Returns 64-bit xor between high and low 64 bits of C.
 */
inline uint64_t rapid_mix(uint64_t A, uint64_t B) {
  std::tie(A, B) = rapid_mul128(A, B);
  return A ^ B;
}

/*
 *  Read functions.
 */
inline uint64_t rapid_read64(const uint8_t* p) {
  uint64_t v;
  memcpy(&v, p, sizeof(uint64_t));
  return v;
}
inline uint64_t rapid_read32(const uint8_t* p) {
  uint32_t v;
  memcpy(&v, p, sizeof(uint32_t));
  return v;
}

/*
 *  Reads and combines 3 bytes of input.
 *
 *  @param p  Buffer to read from.
 *  @param k  Length of @p, in bytes.
 *
 *  Always reads and combines 3 bytes from memory.
 *  Guarantees to read each buffer position at least once.
 *
 *  Returns a 64-bit value containing all three bytes read.
 */
inline uint64_t rapid_readSmall(const uint8_t* p, size_t k) {
  return (((uint64_t)p[0]) << 56) | (((uint64_t)p[k >> 1]) << 32) | p[k - 1];
}

/*
 *  rapidhash main function.
 *
 *  @param p       Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *  @param secret  Triplet of 64-bit secrets used to alter hash result
 *                 predictably.
 *
 *  Returns a 64-bit hash.
 *
 *  The data flow is separated so that we never mix input data with pointers;
 *
 *  a, b, seed, secret[0], secret[1], secret[2], see1 and see2 are affected
 *  by the input data.
 *
 *  p is only ever indexed by len, delta (comes from len only), i (comes from
 *  len only) or integral constants. len is const, so no data can flow into it.
 *
 *  No other reads from memory take place. No writes to memory take place.
 */
inline uint64_t rapidhash_internal(const uint8_t* p,
                                   const size_t len,
                                   uint64_t seed,
                                   const uint64_t secret[3]) {
  seed ^= rapid_mix(seed ^ secret[0], secret[1]) ^ len;
  uint64_t a, b;
  if (_likely_(len <= 16)) {
    if (_likely_(len >= 4)) {
      // Read the first and last 32 bits (they may overlap).
      const uint8_t* plast = p + len - 4;
      a = (rapid_read32(p) << 32) | rapid_read32(plast);

      // This is equivalent to: delta = (len >= 8) ? 4 : 0;
      const uint64_t delta = ((len & 24) >> (len >> 3));
      b = ((rapid_read32(p + delta) << 32) | rapid_read32(plast - delta));
    } else if (_likely_(len > 0)) {
      // 1, 2 or 3 bytes.
      a = rapid_readSmall(p, len);
      b = 0;
    } else {
      a = b = 0;
    }
  } else {
    size_t i = len;
    if (_unlikely_(i > 48)) {
      uint64_t see1 = seed, see2 = seed;
      do {
        seed =
            rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
        see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1],
                         rapid_read64(p + 24) ^ see1);
        see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2],
                         rapid_read64(p + 40) ^ see2);
        p += 48;
        i -= 48;
      } while (_likely_(i >= 48));
      seed ^= see1 ^ see2;
    }
    if (i > 16) {
      seed = rapid_mix(rapid_read64(p) ^ secret[2],
                       rapid_read64(p + 8) ^ seed ^ secret[1]);
      if (i > 32) {
        seed = rapid_mix(rapid_read64(p + 16) ^ secret[2],
                         rapid_read64(p + 24) ^ seed);
      }
    }
    a = rapid_read64(p + i - 16);
    b = rapid_read64(p + i - 8);
  }
  a ^= secret[1];
  b ^= seed;
  std::tie(a, b) = rapid_mul128(a, b);
  return rapid_mix(a ^ secret[0] ^ len, b ^ secret[1]);
}

/*
 *  rapidhash default seeded hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *
 *  Calls rapidhash_internal using provided parameters and default secrets.
 *
 *  Returns a 64-bit hash.
 */
inline uint64_t rapidhash(const uint8_t* key,
                          size_t len,
                          uint64_t seed = RAPID_SEED) {
  return rapidhash_internal(key, len, seed, rapid_secret);
}

#endif  // _THIRD_PARTY_RAPIDHASH_H
