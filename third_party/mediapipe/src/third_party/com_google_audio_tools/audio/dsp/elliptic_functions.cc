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

#include "audio/dsp/elliptic_functions.h"

#include <cmath>
#include <limits>
#include <vector>

#include "audio/dsp/signal_vector_util.h"
#include "glog/logging.h"

using std::complex;

namespace audio_dsp {

namespace {

// Generate a sequence of moduli by repeated Landen transformations for
// parameter value m. This sequence is useful for evaluating K(m) and other
// elliptic functions.
std::vector<double> GenerateLandenSequence(double m) {
  constexpr int kMaxLength = 10;
  constexpr double kTol = std::numeric_limits<double>::epsilon();
  double elliptic_modulus = std::sqrt(m);
  std::vector<double> result({elliptic_modulus});
  result.reserve(kMaxLength);
  // Each iteration makes elliptic_modulus smaller. The iteration converges
  // rapidly, needing five or fewer iterations for m <= 0.99.
  while (result.size() < kMaxLength && elliptic_modulus > kTol) {
    // This recurrence is equation (50) in
    // http://www.ece.rutgers.edu/~orfanidi/ece521/notes.pdf
    elliptic_modulus = Square(elliptic_modulus /
        (1.0 + std::sqrt(1.0 - Square(elliptic_modulus))));
    result.push_back(elliptic_modulus);
  }
  LOG_IF(WARNING, elliptic_modulus > kTol)
      << "Landen sequence did not converge, results may be inaccurate.";
  return result;
}

double EllipticKFromLanden(const std::vector<double>& landen_sequence) {
  // Evaluate K(m) by equation (53) in
  // http://www.ece.rutgers.edu/~orfanidi/ece521/notes.pdf
  // NOTE: An alternative to compute K(m) is the arithmetic-geometric mean (AGM)
  // process [see 17.6 in http://people.math.sfu.ca/~cbm/aands/page_598.htm].
  double result = M_PI / 2;
  for (int i = 1; i < landen_sequence.size(); ++i) {
    result *= 1.0 + landen_sequence[i];
  }
  return result;
}

}  // namespace

double EllipticK(double m) {
  ABSL_DCHECK_GE(m, 0.0);
  ABSL_DCHECK_LE(m, 1.0);

  if (m == 1.0) {
    return std::numeric_limits<double>::infinity();
  } else if (m > 1.0 - 1e-12) {
    // Use asymptotic approximation near m = 1, where the function has a
    // singularity. This formula is between equations (53) and (54) in
    // http://www.ece.rutgers.edu/~orfanidi/ece521/notes.pdf
    const double m1 = 1.0 - m;
    const double temp = -0.5 * std::log(m1 / 16.0);
    return temp + (temp - 1.0) * m1 / 4;
  } else {
    return EllipticKFromLanden(GenerateLandenSequence(m));
  }
}

// u = sn^-1(sin(phi)|m) = F(phi|m)
complex<double> EllipticF(const complex<double>& phi, double m) {
  ABSL_DCHECK_LE(std::abs(std::real(phi)), M_PI / 2);
  ABSL_DCHECK_GE(m, 0.0);
  ABSL_DCHECK_LT(m, 1.0);

  if (std::abs(phi) < 1e-3) {
    // For small phi, a simple Taylor approximation is more accurate than the
    // inverse cd method used below. Use the first two series terms from
    // http://reference.wolfram.com/language/ref/EllipticF.html
    return phi * (1.0 + Square(phi) * (m / 6.0));
  } else {
    // To evaluate u = F(phi|m), compute
    //   w0 = sin(phi)
    // so that w0 = sn(u|m), then apply equation (56) for inverting sn(u|m)
    // described in http://www.ece.rutgers.edu/~orfanidi/ece521/notes.pdf
    std::vector<double> landen_sequence = GenerateLandenSequence(m);
    complex<double> w = std::sin(phi);
    for (int i = 1; i < landen_sequence.size(); ++i) {
      w *= 2.0 / ((1.0 + landen_sequence[i]) *
                  (1.0 + std::sqrt(1.0 - Square(landen_sequence[i - 1] * w))));
    }
    return (2 / M_PI) * std::asin(w) * EllipticKFromLanden(landen_sequence);
  }
}

complex<double> JacobiAmplitude(const complex<double>& u, double m) {
  ABSL_DCHECK_GE(m, 0.0);
  ABSL_DCHECK_LT(m, 1.0);
  // To evaluate phi = am(u|m), use descending Landen transformation (Gauss'
  // transformation) to compute sn(u|m), then get phi = asin(sn(u|m)).
  //
  // The recurrence to compute sn(u|m) is
  //   w_{n-1} = w_n (1 + k_n) / (1 + k_n w_n^2),
  // where k_n is the nth Landen sequence modulus for parameter m. The
  // recurrence is initialized with w_N = sin((pi/2) u / K(m)) and ends with
  // w_0 = sn(u|m). This recurrence is equation (55) in
  // http://www.ece.rutgers.edu/~orfanidi/ece521/notes.pdf or equation (16.12.2)
  // in http://people.math.sfu.ca/~cbm/aands/page_573.htm.
  // NOTE: An alternative is the AGM-based recurrence in equations (16.4.2-3)
  // from http://people.math.sfu.ca/~cbm/aands/page_571.htm.
  std::vector<double> landen_sequence = GenerateLandenSequence(m);
  complex<double> w =
      std::sin((M_PI / 2) * u / EllipticKFromLanden(landen_sequence));
  for (int i = landen_sequence.size() - 1; i > 0; --i) {
    const double k = landen_sequence[i];
    w *= (1.0 + k) / (1.0 + k * Square(w));
  }
  return std::asin(w);
}

}  // namespace audio_dsp
