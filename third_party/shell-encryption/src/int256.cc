/*
 * Copyright 2018 Google LLC.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "int256.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include <glog/logging.h>
#include "absl/numeric/int128.h"

namespace rlwe {

// Returns the 0-based position of the last set bit (i.e., most significant bit)
// in the given uint64. The argument may not be 0.
//
// For example:
//   Given: 5 (decimal) == 101 (binary)
//   Returns: 2
#define STEP(T, n, pos, sh)                   \
  do {                                        \
    if ((n) >= (static_cast<T>(1) << (sh))) { \
      (n) = (n) >> (sh);                      \
      (pos) |= (sh);                          \
    }                                         \
  } while (0)
static inline int Fls64(Uint64 n) {
  //DCHECK_NE(0, n);
  int pos = 0;
  STEP(Uint64, n, pos, 0x20);
  Uint32 n32 = n;
  STEP(Uint32, n32, pos, 0x10);
  STEP(Uint32, n32, pos, 0x08);
  STEP(Uint32, n32, pos, 0x04);
  return pos + ((static_cast<Uint64>(0x3333333322221100) >> (n32 << 2)) & 0x3);
}
#undef STEP

// Like Fls64() above, but returns the 0-based position of the last set bit
// (i.e., most significant bit) in the given uint128. The argument may not be 0.
static inline int Fls128(absl::uint128 n) {
  if (Uint64 hi = absl::Uint128High64(n)) {
    return Fls64(hi) + 64;
  }
  return Fls64(absl::Uint128Low64(n));
}

// Like Fls128() above, but returns the 0-based position of the last set bit
// (i.e., most significant bit) in the given uint256. The argument may not be 0.
static inline int Fls256(uint256 n) {
  absl::uint128 hi = Uint256High128(n);
  if (hi != 0) {
    return Fls128(hi) + 128;
  }
  return Fls128(Uint256Low128(n));
}

// Long division/modulo for uint256 implemented using the shift-subtract
// division algorithm adapted from:
// http://stackoverflow.com/questions/5386377/division-without-using
void uint256::DivModImpl(uint256 dividend, uint256 divisor,
                         uint256* quotient_ret, uint256* remainder_ret) {
  if (divisor == static_cast<uint256>(0)) {
    LOG(FATAL) << "Division or mod by zero: dividend.hi=" << dividend.hi_
               << ", lo=" << dividend.lo_;
  }

  if (divisor > dividend) {
    *quotient_ret = 0;
    *remainder_ret = dividend;
    return;
  }

  if (divisor == dividend) {
    *quotient_ret = 1;
    *remainder_ret = 0;
    return;
  }

  uint256 denominator = divisor;
  uint256 quotient = 0;

  // Left aligns the MSB of the denominator and the dividend.
  const int shift = Fls256(dividend) - Fls256(denominator);
  denominator <<= shift;

  // Uses shift-subtract algorithm to divide dividend by denominator. The
  // remainder will be left in dividend.
  for (int i = 0; i <= shift; ++i) {
    quotient <<= 1;
    if (dividend >= denominator) {
      dividend -= denominator;
      quotient |= 1;
    }
    denominator >>= 1;
  }

  *quotient_ret = quotient;
  *remainder_ret = dividend;
}

uint256& uint256::operator/=(const uint256& divisor) {
  uint256 quotient = 0;
  uint256 remainder = 0;
  DivModImpl(*this, divisor, &quotient, &remainder);
  *this = quotient;
  return *this;
}
uint256& uint256::operator%=(const uint256& divisor) {
  uint256 quotient = 0;
  uint256 remainder = 0;
  DivModImpl(*this, divisor, &quotient, &remainder);
  *this = remainder;
  return *this;
}

std::ostream& operator<<(std::ostream& o, const uint256& b) {
  std::ios_base::fmtflags flags = o.flags();

  // Select a divisor which is the largest power of the base < 2^64.
  uint256 div;
  std::streamsize div_base_log;
  switch (flags & std::ios::basefield) {
    case std::ios::hex:
      div = static_cast<Uint64>(0x1000000000000000);  // 16^15
      div_base_log = 15;
      break;
    case std::ios::oct:
      div = static_cast<Uint64>(01000000000000000000000);  // 8^21
      div_base_log = 21;
      break;
    default:                                               // std::ios::dec
      div = static_cast<Uint64>(10000000000000000000ull);  // 10^19
      div_base_log = 19;
      break;
  }

  // Now piece together the uint256 representation from five chunks of
  // the original value, each less than "div" and therefore representable
  // as a uint64.
  std::ostringstream os;
  std::ios_base::fmtflags copy_mask =
      std::ios::basefield | std::ios::showbase | std::ios::uppercase;
  os.setf(flags & copy_mask, copy_mask);
  uint256 high = b;
  uint256 low;
  uint256::DivModImpl(high, div, &high, &low);
  uint256 mid_low;
  uint256::DivModImpl(high, div, &high, &mid_low);
  uint256 mid_mid;
  uint256::DivModImpl(high, div, &high, &mid_mid);
  uint256 mid_high;
  uint256::DivModImpl(high, div, &high, &mid_high);
  bool print = high.lo_ != 0;
  if (print) {
    os << high.lo_;
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
  }
  print |= mid_high.lo_ != 0;
  if (print) {
    os << mid_high.lo_;
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
  }
  print |= mid_mid.lo_ != 0;
  if (print) {
    os << mid_mid.lo_;
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
  }
  print |= mid_low.lo_ != 0;
  if (print) {
    os << mid_low.lo_;
    os << std::noshowbase << std::setfill('0') << std::setw(div_base_log);
  }
  os << low.lo_;
  std::string rep = os.str();

  // Add the requisite padding.
  std::streamsize width = o.width(0);
  if (width > rep.size()) {
    if ((flags & std::ios::adjustfield) == std::ios::left) {
      rep.append(width - rep.size(), o.fill());
    } else {
      rep.insert(0, width - rep.size(), o.fill());
    }
  }

  // Stream the final representation in a single "<<" call.
  return o << rep;
}

}  // namespace rlwe
