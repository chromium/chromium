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

#include "audio/dsp/bessel_functions.h"

#include <cfloat>
#include <cmath>

#include "audio/dsp/signal_vector_util.h"
#include "glog/logging.h"

namespace audio_dsp {

namespace {

// Evaluate I_n(x) the nth-order modified Bessel function of the first kind,
// where n = 0 or 1, using a power series accurate for small |x|, e.g. for I0(x)
//   1 + (x^2/4) / (1!)^2 + (x^2/4)^2 / (2!)^2 + (x^2/4)^3 / (3!)^2 + ...
// [Abramowitz, Stegun, "Handbook of Mathematical Functions," Chapter 9, p. 375]
// This function is reliable for small |x|, |x| <= 21.
double BesselISeries(int n, double x) {
  ABSL_DCHECK_GE(n, 0);
  ABSL_DCHECK_LE(n, 1);
  constexpr int kMaxTerms = 40;
  const double x_squared_over_4 = x * x / 4.0;
  // NOTE: This function could support any (even non-integer) order n by setting
  // here sum = (x / 2)^n / Gamma(n + 1).
  double sum = (n == 0) ? 1.0 : x / 2;
  double term = sum;
  for (int k = 1; k < kMaxTerms; ++k) {
    term *= x_squared_over_4 / (k * (k + n));
    // Typically fewer than 20 terms are needed to reach machine precision. More
    // terms are needed for larger |x| with up to 34 terms as |x| approaches 21.
    if (term < DBL_EPSILON * sum) {
      break;
    }
    sum += term;
  }
  return sum;
}

// Evaluate I_n(x) the nth-order modified Bessel function of the first kind
// using an asymptotic expansion,
//   (e^x / sqrt(2 pi x)) sum_k=0^infinity (-1)^k a_k(n) / x^k,
// where
//   a_k(n) = [ (4n^2 - 1^1) (4n^2 - 3^2) ... (4n^2 - (2k - 1)^2) ] / (k! 8^k)
// [Abramowitz, Stegun, "Handbook of Mathematical Functions," Chapter 9, p. 377]
// This function is reliable for large |x|, |x| >= 21.
double BesselIAsymptotic(int n, double x) {
  constexpr int kMaxTerms = 40;
  const double n_squared_times_4 = 4.0 * (n * n);
  double sum = std::exp(x) / std::sqrt(2.0 * M_PI * x);
  double term = sum;
  for (int k = 1; k < kMaxTerms; ++k) {
    term *= (Square(2 * k - 1) - n_squared_times_4) / (8 * k * x);
    // Being an asymptotic approximation, more terms are needed for smaller |x|,
    // with up to 20 terms when |x| = 21.
    if (std::abs(term) < DBL_EPSILON * sum) {
      break;
    }
    sum += term;
  }
  return sum;
}

}  // namespace

double BesselI0(double x) {
  x = std::abs(x);
  // TODO: Consider using Miller's algorithm for 2 <= |x| < 21, like
  // Amos does http://prod.sandia.gov/techlib/access-control.cgi/1985/851018.pdf
  if (x < 21) {
    return BesselISeries(0, x);
  } else {
    return BesselIAsymptotic(0, x);
  }
}

double BesselI1(double x) {
  const int sign = (x < 0) ? -1 : 1;
  x = std::abs(x);
  if (x < 21) {
    return sign * BesselISeries(1, x);
  } else {
    return sign * BesselIAsymptotic(1, x);
  }
}

}  // namespace audio_dsp
