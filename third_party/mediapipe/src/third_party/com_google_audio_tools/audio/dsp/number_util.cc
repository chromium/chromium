/*
 * Copyright 2020 Google LLC
 *
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

#include "audio/dsp/number_util.h"

#include <cmath>
#include <iomanip>
#include <limits>

#include "glog/logging.h"
#include "audio/dsp/portable/number_util.h"

namespace audio_dsp {

using ::std::isfinite;

int Modulo(int value, int modulus) {
  ABSL_DCHECK_GT(modulus, 0);
  if (value < 0) {
    return modulus - 1 - ((-value - 1) % modulus);
  } else {
    return value % modulus;
  }
}

int RoundDownToMultiple(int value, int factor) {
  ABSL_DCHECK_GT(factor, 0);
  if (value < 0) {
    return value - factor + 1 + ((-value - 1) % factor);
  } else {
    return value - (value % factor);
  }
}

int RoundUpToMultiple(int value, int factor) {
  ABSL_DCHECK_GT(factor, 0);
  if (value < 1) {
    return value + ((-value) % factor);
  } else {
    return value + factor - 1 - ((value - 1) % factor);
  }
}

int GreatestCommonDivisor(int a, int b) {
  ABSL_DCHECK_GE(a, 0);
  ABSL_DCHECK_GE(b, 0);
  while (b != 0) {
    int remainder = a % b;
    a = b;
    b = remainder;
  }
  return a;
}

int GreatestCommonDivisor(const std::vector<int>& a) {
  ABSL_DCHECK_GE(a.size(), 1);
  int gcd = a[0];
  for (int i = 1; i < a.size(); ++i) {
    gcd = GreatestCommonDivisor(gcd, a[i]);
  }
  return gcd;
}

int Log2Floor(unsigned value) {
  if (value == 0) {
    return -1;
  }
  int result = 0;
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}

int Log2Ceiling(unsigned value) {
  int result = Log2Floor(value);
  if (!IsPowerOfTwoOrZero(value)) {
    ++result;
  }
  return result;
}

unsigned NextPowerOfTwo(unsigned value) {
  int exponent = Log2Ceiling(value);
  ABSL_DCHECK_LT(exponent, std::numeric_limits<unsigned>::digits);
  return 1 << exponent;
}

ArithmeticSequence::ArithmeticSequence(double base, double step, double limit)
    : base_(base), step_(step), limit_(limit) {
  // Some initial checks for obvious problems. Further checks are made below.
  // Inifinity and NaN arguments are not allowed, and step must be nonzero.
  ABSL_CHECK(std::isfinite(base_) && std::isfinite(limit_) &&
        std::isfinite(step_) && step_ != 0.0)
      << "Arguments: (" << base << ", " << step << ", " << limit << ")";
  // The following computes the size of the sequence.
  // Notations:
  //   B = base,
  //   S = step,
  //   L = limit,
  //   N = size.
  //
  // We want to compute the size N of the sequence from B, S, and L:
  //   N = 1 + floor((L - B) / S).
  // Frequently, the caller's intention is that (L - B) is a whole number of
  // steps S. We calculate N in an error-tolerant way so that the result is
  // correct for such sequences.
  //
  // [Example where tolerance matters: for ArithmeticSequence(3.1, 0.1, 3.3),
  //   (L - B) / S = 1.9999999999999973,
  // so a naive computation produces an incorrect size-2 result {3.1, 3.2}. To
  // correct for this, we adjust L by a small value,
  //   (L + sign(S) * tol - B) / S = 2.0000000000000284,
  // obtaining the correct size-3 result {3.1, 3.2, 3.3}.]
  //
  // We consider that the given B, S, L might not represent their exact intended
  // values and are perturbed with relative error of up to 4 epsilon,
  //   |B - B'| <= 4 epsilon |B'|,
  //   |S - S'| <= 4 epsilon |S'|,
  //   |L - L'| <= 4 epsilon |L'|,
  // where primed ' quantities are the exact intended values and epsilon is
  // machine double epsilon (about 2.22e-16). [Why 4 epsilon?---this loose
  // tolerance is in case an argument is the result of an arithmetic operation,
  // say L = M_PI / 3. For multiplication, division, or addition of the same
  // sign, 4 epsilon is a conservative bound on the result's relative error.]
  //
  // Our design goal is: if B, S, L are within 4 epsilon relative difference of
  // some B', S', L' for which (L' - B') / S' is integer, we want to compute the
  // sequence size N to be that integer.
  //
  // To that end, suppose B' + (N - 1) S' = L' for some integer N, then for some
  // quantites x, y, z with magnitude of no more than 4 epsilon,
  //   |B + (N - 1) S - L| =  |B' (1 + x) + (N - 1) S' (1 + y) - L' (1 + z)|
  //                       =  |B' (1 + x) + (L' - B') (1 + y) - L' (1 + z)|
  //                       <= |B'| |x| + |L' - B'| |y| + |L'| |z|
  //                       <= 4 epsilon (|B'| + |L' - B'| + |L'|)
  //                       <= 4 epsilon (1 + 2 epsilon) (|B| + |L - B| + |L|)
  //                       <= 5 epsilon (|B| + |L - B| + |L|).
  // Suppose we set tol >= 5 epsilon (|B| + |L - B| + |L|) and require that the
  // step S satisfies |S| >= 3 tol. We assume positive S below; analysis is
  // similar for negative S. Then,
  //   L - tol <= B + (N - 1) S <= L + tol,
  // and
  //   L + tol < L - tol + S <= B + N S,
  // so that L + tol is bounded from below and above as
  //   B + (N - 1) S <= L + tol < B + N S.
  // Similarly for negative S,
  //   B + (N - 1) S >= L - tol > B + N S.
  // Therefore, the desired size N can be determined from the perturbed
  // arguments B, S, L, by finding the unique integer N that satisfies the this
  // condition.
  //
  // The above condition suggests calculating the size by
  //   N = 1 + floor((L - B + tol) / S).
  // However, round-off error in computing this in double arithmetic may yet
  // produce the wrong answer. Computation of the difference "L - B" has
  // round-off error that may round down by as much as epsilon |L - B|. Adding
  // tol in "(L - B) + tol" constributes approximately another epsilon |L - B|
  // of rounding error. To prevent this from changing the result of the floor,
  // we counteract this error in tol,
  //   tol := 5 epsilon (|B| + |L - B| + |L|) + 2 epsilon |L - B|.
  // [Alternatively, we could set fesetround(FE_UPWARD) before computing to
  // force rounding upward, but fesetround has poor portability.] We ignore
  // round-off error in division by S: presumably, the result (L - B + tol) / S
  // is close to an int32 integer, which in double is exactly representable, so
  // rounding down toward this integer does not change the result.
  const int sign_step = step_ > 0 ? 1 : -1;
  const double epsilon = std::numeric_limits<double>::epsilon();
  double tol = epsilon * (5 * (std::abs(base_) + std::abs(limit_)) +
                          7 * std::abs(limit_ - base_));
  if (std::abs(step_) < 3 * tol) {
    LOG_FIRST_N(WARNING, 20)
      << std::setprecision(std::numeric_limits<double>::digits10 + 2)
      << "step is small compared to machine precision in ArithmeticSequence("
      << base_ << ", " << step_ << ", " << limit_ << "), result is unreliable.";
    tol = std::abs(step_) / 3;
  }

  double sized = 1 + std::floor(((limit_ - base_) + sign_step * tol) / step_);

  if (sized <= 0.0) {
    size_ = 0;  // Empty range.
    limit_ = base_;
    return;
  }

  ABSL_CHECK_LE(sized, std::numeric_limits<int>::max());  // Prevent overflow.
  size_ = static_cast<int>(sized);

  // In tests, the above computation always computes the size correctly. But as
  // a precaution, we check whether size is off.
  ABSL_CHECK_LE(sign_step * (base_ + step_ * (size_ - 1)),
           sign_step * limit_ + 2 * tol)
      << std::setprecision(std::numeric_limits<double>::digits10 + 2)
      << "size = " << size_ << " would overshoot limit in ArithmeticSequence("
      << base_ << ", " << step_ << ", " << limit_ << ").";
  ABSL_CHECK_LT(sign_step * limit_, sign_step * (base_ + step_ * size_))
      << std::setprecision(std::numeric_limits<double>::digits10 + 2)
      << "size = " << size_ << " would undershoot limit in ArithmeticSequence("
      << base_ << ", " << step_ << ", " << limit_ << ").";

  if (size_ == 1) {  // Singleton range, the only value is base.
    step_ = 1.0;
    limit_ = base_;
    return;
  }
  // Compute the last element in the range.
  double last = base_ + step_ * (size_ - 1);
  // If limit is more than a few epsilon beyond last, set limit to last.
  if (((step > 0.0 && limit_ > last) || (step < 0.0 && limit_ < last)) &&
      4 * epsilon * step_ * std::max(std::abs(limit_), std::abs(last))
      <= std::abs(limit_ - last)) {
    limit_ = last;
  }
}

ArithmeticSequence::Iterator ArithmeticSequence::Iterator::operator++(
    int /* postincrement */) {
  Iterator old(*this);
  ++(*this);
  return old;
}

ArithmeticSequence::Iterator ArithmeticSequence::Iterator::operator--(
    int /* postdecrement */) {
  Iterator old(*this);
  --(*this);
  return old;
}

ArithmeticSequence::Iterator ArithmeticSequence::Iterator::operator+(
    int n) const {
  Iterator it(*this);
  it += n;
  return it;
}

ArithmeticSequence::Iterator ArithmeticSequence::Iterator::operator-(
    int n) const {
  Iterator it(*this);
  it -= n;
  return it;
}

CombinationsIterator::CombinationsIterator(int n, int k)
    : n_(n), k_(k), indices_(k_), is_done_(k_ > n_) {
  for (int i = 0; i < k_; ++i) {
    indices_[i] = i;
  }
}

void CombinationsIterator::Next() {
  if (is_done_) {
    LOG(ERROR) << "Next() called when already done.";
    return;
  }
  int i;
  for (i = k_ - 1; i >= 0; --i) {
    ++indices_[i];
    // Stop on the rightmost index that doesn't need to roll over.
    if (indices_[i] <= n_ - k_ + i) {
      break;
    }
  }
  if (i < 0) {
    is_done_ = true;
    return;
  }
  // Roll over all indices to the right of i.
  for (++i; i < k_; ++i) {
    indices_[i] = indices_[i - 1] + 1;
  }
}

CrossProductRange::CrossProductRange(const std::vector<int>& shape)
    : shape_(shape) {}

bool CrossProductRange::empty() const {
  for (int size : shape_) {
    if (size <= 0) {
      return true;
    }
  }
  return shape_.empty();
}

CrossProductRange::Iterator::Iterator(const CrossProductRange& range)
    : shape_(range.shape()),
      indices_(shape_.size(), 0),
      is_end_(range.empty()) {}

CrossProductRange::Iterator::Iterator(const CrossProductRange& range,
                                      int64 flat_index)
    : Iterator(range) {
  if (!is_end_) {
    for (int i = 0; i < shape_.size() - 1; ++i) {
      indices_[i] = flat_index % shape_[i];
      flat_index /= shape_[i];
    }
    indices_.back() = static_cast<int>(flat_index);
    is_end_ = indices_.back() >= shape_.back();
  }
}

CrossProductRange::Iterator& CrossProductRange::Iterator::operator++() {
  if (!is_end_) {
    for (int i = 0; i < indices_.size(); ++i) {
      ++indices_[i];
      if (indices_[i] >= shape_[i]) {
        indices_[i] = 0;
      } else {
        return *this;
      }
    }
    is_end_ = true;
  }
  return *this;
}

CrossProductRange::Iterator CrossProductRange::Iterator::operator++(
    int /* postincrement */) {
  CrossProductRange::Iterator old(*this);
  ++(*this);
  return old;
}


bool CrossProductRange::Iterator::operator==(
    const CrossProductRange::Iterator& other) const {
  // Two "end" iterators are always equal. If the two iterators being
  // compared aren't both end iterators, then we fall back to comparing their
  // fields.
  return (is_end_ && other.is_end_) ||
      (is_end_ == other.is_end_ &&
       shape_ == other.shape_ &&
       indices_ == other.indices_);
}

namespace {

typedef std::pair<int, int> Fraction;

inline double FractionToDouble(const Fraction& rational) {
  ABSL_DCHECK_GT(rational.second, 0);
  return static_cast<double>(rational.first) / rational.second;
}

// Suppose that convergent and prev_convergent are respectively the N- and
// (N - 1)-term convergents of a continued fraction representation. This
// function computes the fraction obtained by appending a term as the (N + 1)th
// continued fraction term. This update formula applies Theorem 1 from
// https://en.wikipedia.org/wiki/Continued_fraction#Some_useful_theorems
// and the produced fraction is in reduced form by Corollary 1.
inline Fraction AppendContinuedFractionTerm(
    const Fraction& convergent, const Fraction& prev_convergent, int term) {
  ABSL_DCHECK_GE(term, 1);
  return {term * convergent.first + prev_convergent.first,
          term * convergent.second + prev_convergent.second};
}

}  // namespace

std::pair<int, int> RationalApproximation(double value, int max_denominator) {
  std::pair<int, int> rational;
  ::RationalApproximation(value, max_denominator, nullptr,
                          &rational.first, &rational.second);
  return rational;
}

}  // namespace audio_dsp
