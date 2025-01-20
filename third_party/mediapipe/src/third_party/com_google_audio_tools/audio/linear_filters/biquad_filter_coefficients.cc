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

#include "audio/linear_filters/biquad_filter_coefficients.h"

#include <limits>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace linear_filters {

using ::std::complex;
using ::std::vector;

namespace {

bool IsFirstOrder(const vector<double>& poly) {
  return std::abs(poly[2]) <= 1e-10 && std::abs(poly[1]) > 1e-10;
}

bool IsZeroOrder(const vector<double>& poly) {
  return std::abs(poly[1]) <= 1e-10 && std::abs(poly[2]) <= 1e-10;
}

// Find a zero order set of poles and a zero order set of zeros to combine.
bool FindZeroOrderPoly(const vector<BiquadFilterCoefficients>& coeffs,
                       int* a_index, int* b_index, double* gain) {
  int n_stages = coeffs.size();
  *a_index = 0;
  *b_index = 0;
  while (!IsZeroOrder(coeffs[*a_index].a)) {
    *a_index += 1;
    if (*a_index >= n_stages) {
      return false;
    }
  }
  while (!IsZeroOrder(coeffs[*b_index].b)) {
    *b_index += 1;
    if (*b_index >= n_stages) {
      return false;
    }
  }
  *gain = coeffs[*b_index].b[0] / coeffs[*a_index].a[0];
  return true;
}

// Find pair of biquads that are both first order.
bool FindPairOfFirstOrderBiquads(const vector<BiquadFilterCoefficients>& coeffs,
                                 int* index, int* index2, bool use_A) {
  auto pred = [&](const BiquadFilterCoefficients& biquad) {
    if (use_A) {
      return IsFirstOrder(biquad.a);
    } else {
      return IsFirstOrder(biquad.b);
    }
  };

  *index = 0;
  auto it = std::find_if(coeffs.begin(), coeffs.end(), pred);
  if (it == coeffs.end()) { return false; }
  *index = it - coeffs.begin();  // Found a first-order polynomial.

  it = std::find_if(it + 1, coeffs.end(), pred);
  if (it == coeffs.end()) { return false; }
  *index2 = it - coeffs.begin();  // Found another one.

  return true;
}


std::vector<double> Convolve(const vector<double>& x,
                             const vector<double>& y) {
  std::vector<double> result(x.size() + y.size() - 1);
  for (int i = 0; i < x.size(); ++i) {
    for (int j = 0; j < y.size(); ++j) {
      result[i + j] += x[i] * y[j];
    }
  }
  // Remove highest order elements with a coefficient of exactly zero. This
  // will only happen if the highest order coefficients of x or y are zero.
  int order = result.size();
  while (std::abs(result[order - 1]) == 0) {
    --order;
  }
  result.resize(order);
  return result;
}

}  // namespace

// Create k and v coefficients for a ladder filter.

void MakeLadderCoefficientsFromTransferFunction(
    const vector<double>& coeffs_b, const vector<double>& coeffs_a,
    vector<double>* coeffs_k_ptr, vector<double>* coeffs_v_ptr) {
  vector<double>& coeffs_k = *coeffs_k_ptr;
  vector<double>& coeffs_v = *coeffs_v_ptr;
  ABSL_CHECK(!coeffs_b.empty());
  ABSL_CHECK(!coeffs_a.empty());
  ABSL_CHECK_NE(coeffs_a[0], 0);
  // Normalize coefficients. The numerator coefficients, P, represents the
  // zeros of the original transfer function. The ladder filter output is a
  // linear combination of each lattice stage (shown in Gray & Markel as a
  // stage with some coupled multiply/add operations) whose weights,
  // coeffs_v, are related to both P and A. A, the denominator coefficients
  // are entirely responsible for the coeffs_k coefficients inside of the
  // lattice.
  double normalize = coeffs_a[0];
  vector<double> P(coeffs_b.size());
  for (int i = 0; i < coeffs_b.size(); ++i) {
    P[i] = coeffs_b[i] / normalize;
  }
  vector<double> A(coeffs_a.size());
  for (int i = 0; i < coeffs_a.size(); ++i) {
    A[i] = coeffs_a[i] / normalize;
  }
  int filter_order = std::max(P.size(), A.size()) - 1;

  // Resize all vectors.
  P.resize(filter_order + 1);  // One of these two resizes is a no-op.
  A.resize(filter_order + 1);
  coeffs_k.resize(filter_order);
  coeffs_v.resize(filter_order + 1);
  // Convert input coeffs_b and coeffs_a to 2-multiply lattice filter
  // coefficients k and v.
  // See equations 4-8 from Gray & Markel.
  for (int m = filter_order; m > 0; --m) {
    // A temporary vector that holds some of the A coefficients before
    // they get modified by the recursion below.
    vector<double> zB(filter_order + 1);
    // Equation (4) from Gray & Markel, z B_m(z) = A_m(1/z) z^-m.
    for (int j = 0; j < m; ++j) {
      zB[j] = A[m - j];
    }

    coeffs_k[m - 1] = A[m];

    const double one_minus_ksq_inv =
        1.0 / (1 - coeffs_k[m - 1] * coeffs_k[m - 1]);
    // Update feedback polynomial A according to equation (6),
    // A_{m-1}(z) = [A_m(z) - k{m-1}z B_m(z)] / (1 - k{m-1}^2).
    coeffs_v[m] = P[m];
    for (int j = 0; j < m; ++j) {
      A[j] = (A[j] - coeffs_k[m - 1] * zB[j]) * one_minus_ksq_inv;
      // Updated P according to equation (8),
      // P_{m-1}(z) = P_m(z) - z B_m(z) v{m}.
      P[j] = P[j] - zB[j] * coeffs_v[m];
    }
  }
  coeffs_v[0] = P[0];
}

bool BiquadFilterCoefficients::IsStable() const {
  vector<double> k;
  vector<double> v;
  MakeLadderCoefficientsFromTransferFunction(b, a, &k, &v);
  for (double k_i : k) {
    if (std::abs(k_i) >= 1) {
      return false;
    }
  }
  return true;
}

std::pair<double, double> FindPeakByBisection(
    const std::function<double(double)>& fun, double minimum, double maximum) {
  ABSL_CHECK_GT(maximum, minimum);
  // Rather than solve explicitly like we do above, we use a numerical approach
  // noting that for high order filters, the numerical root finding
  // becomes numerically unstable.
  double peak_frequency = 0.0;  // For identity filter, this will be the answer.
  double peak_gain = 0.0;
  constexpr int kNumFrequencySteps = 99;
  double frequency_step = (maximum - minimum) / kNumFrequencySteps;
  for (int k = 1; k < kNumFrequencySteps; ++k) {
    double frequency = minimum + k * frequency_step;
    double gain = fun(frequency);
    if (gain > peak_gain) {
      peak_frequency = frequency;
      peak_gain = gain;
    }
  }
  // Refine the peak by a lot of bisection steps.
  constexpr int kNumBisectionSteps = 20;
  for (int j = 0; j < kNumBisectionSteps; ++j) {
    frequency_step /= 2.0;
    double trial_gain = fun(peak_frequency + frequency_step);
    if (trial_gain > peak_gain) {
      peak_frequency += frequency_step;
      peak_gain = trial_gain;
    } else {
      trial_gain = fun(peak_frequency - frequency_step);
      if (trial_gain > peak_gain) {
        peak_frequency -= frequency_step;
        peak_gain = trial_gain;
      }
    }
    if (frequency_step < 1e-9 + 5e-6 * peak_frequency) break;  // Close enough.
  }
  return {peak_frequency, peak_gain};
}
std::pair<double, double>
    BiquadFilterCoefficients::FindPeakFrequencyRadiansPerSample() const {
  // We want to maximize the function
  //            | b0 + b1 z^-1 + b2 z^-2 |
  //   |H(z)| = | ---------------------- |
  //            | a0 + a1 z^-1 + a2 z^-2 |
  //
  // which is equivalent to maximizing H(z)H*(z), where * denotes conjugation.
  // This gives us,
  //   b2 * b0 * (z^-2 + z^2) + b1 * (b2 + b0)(z + z^-1) + (b2^2 + b1^2 + b0^2)
  //   ------------------------------------------------------------------------
  //   a2 * a0 * (z^-2 + z^2) + a1 * (a2 + a0)(z + z^-1) + (a2^2 + a1^2 + a0^2)
  //
  // We simplify the notation to
  //   B2 * (z^-2 + z^2) + B1 * (z^-1 + z) + B0
  //   ----------------------------------------,
  //   A2 * (z^-2 + z^2) + A1 * (z^-1 + z) + A0
  const double B2 = b[2] * b[0];
  const double B1 = b[1] * (b[2] + b[0]);
  const double B0 = b[2] * b[2] + b[1] * b[1] + b[0] * b[0];
  const double A2 = a[2] * a[0];
  const double A1 = a[1] * (a[2] + a[0]);
  const double A0 = a[2] * a[2] + a[1] * a[1] + a[0] * a[0];

  // Using the relationships z = exp(i*x) and exp(i*x) + exp(-i*x) = 2cos(x),
  // we can simplify, making it clear that the function is purely real:
  //             2 * B2 * (2 * cos^2(x) - 1) + 2 * B1 * cos(x) + B0
  //   |H(z)| = ------------------------------------------------
  //             2 * A2 * (2 * cos^2(x) - 1) + 2 * A1 * cos(x) + A0
  //
  //             4 * B2 * cos^2(x) + 2 * B1 * cos(x) + B0 - 2 * B2
  //          = -------------------------------------------------------
  //             4 * A2 * cos^2(x) + 2 * A1 * cos(x) + A0 - 2 * A2
  //            N(cos(x))
  //          = ---------
  //            D(cos(x))
  //
  // Note that we obtained the cos^2(x) term using Euler's formulas and the
  // double angle identity:
  //   z^-2 + z^2 = 2 * cos(2x) = 2 * (2 * cos^2(x) - 1)
  //
  // After making the convenient substitution y = cos(x), we can rewrite
  // N(cos(x)) as N(y):
  //   N(y) = 4 * B2 * y^2 + 2 * B1 * y + B0 - 2 * B2
  //
  // To find maxima, we differentiate |H(z)| using the quotient rule and set
  // to zero.
  //   N'(y)D(y) - N(y)D(y)  dy
  //   -------------------- ---- := 0,
  //           D(y)^2        dx
  //
  // where
  //   N'(y) = 8 * B2 * y + 2 * B1 and D'(y) = 8 * A2 * y + 2 * A1.
  // This function has roots when N'(y)D(y) - N(y)D(y) = 0 and when dy/dx = 0.
  // Since dy/dx is -sin(x), we have a root at y = 0 and y = pi. This is
  // accounted for below.

  // Compute N'(y)D(y) - N(y)D'(y), of the form (ax^2 + bx + c) and find roots.
  // N and D are quadratic. N'(y)D(y) - N(y)D'(y) is also quadratic because
  // the cubic terms cancel.
  const long double deriv_a = 4 * ((2 * B2 * A1 + B1 * A2) -
                                   (2 * A2 * B1 + A1 * B2));
  const long double deriv_b = 2 * ((A1 * B1 + 2 * B2 * (A0 - 2 * A2)) -
                                   (B1 * A1 + 2 * A2 * (B0 - 2 * B2)));
  const long double deriv_c = B1 * (A0 - 2 * A2) - A1 * (B0 - 2 * B2);
  long double root1 = 0;
  long double root2 = 0;
  MathUtil::QuadraticRootType result = MathUtil::RealRootsForQuadratic(
          deriv_a, deriv_b, deriv_c, &root1, &root2);
  vector<double> extrema_rads_per_sample;
  // Account for the root at zero.
  extrema_rads_per_sample.push_back(0);
  if (result != MathUtil::kNoRealRoots) {
    // root2 < root1 according to docs for RealRootsForQuadratic;
    for (double root : {root2, root1}) {
      // Undo the variable substitution, x = acos(y).
      if (std::abs(root) < 1) {  // Valid domain of acos is -1 <= y <= 1.
        extrema_rads_per_sample.push_back(std::acos(static_cast<double>(root)));
      }
    }
  }
  // Add pi after the other roots so roots maintain sorted order (so that the
  // lowest frequency is returned in the event that equal gains are found in
  // the search below).
  extrema_rads_per_sample.push_back(M_PI);

  double frequency_with_max_gain = 0;
  double max_gain = 0;
  for (double extrema : extrema_rads_per_sample) {
    double gain = GainMagnitudeAtFrequency(extrema);
    if (max_gain < gain) {
      max_gain = gain;
      frequency_with_max_gain = extrema;
    }
  }
  return {frequency_with_max_gain, max_gain};
}

std::pair<complex<double>, complex<double>>
    BiquadFilterCoefficients::GetPoles() const {
  const double discriminant = a[1] * a[1] - 4 * a[0] * a[2];
  if (std::abs(discriminant) <= std::numeric_limits<double>::epsilon() *
      std::max(2 * a[1] * a[1], std::abs(4 * a[0] * a[2]))) {
    // Consider the discriminant to be zero, since it has magnitude on the order
    // of round-off error, meaning the filter has a double pole.
    double pole = -a[1] / (2 * a[0]);
    return {pole, pole};
  } else {
    auto sqrt_d = std::sqrt(static_cast<complex<double>>(discriminant));
    complex<double> q = -0.5 * (a[1] + (a[1] < 0.0 ? -sqrt_d : sqrt_d));
    return {q / a[0], a[2] / q};
  }
}

double BiquadFilterCoefficients::EstimateDecayTime(double decay_db) const {
  const auto poles = GetPoles();
  double max_pole_magnitude =
      std::max(std::abs(poles.first), std::abs(poles.second));
  if (max_pole_magnitude >= 1.0) {
    return std::numeric_limits<double>::infinity();  // Unstable filter.
  }
  // The rate of decay of the biquad filter's impulse response is determined
  // mostly by the larger pole magnitude.
  //
  // For a transfer function with two distinct poles p != q of the form
  //                      1
  //   H(z) = -------------------------,
  //          (1 - p z^-1) (1 - q z^-1)
  // H(z) is after partial fraction expansion
  //          p / (p - q)   q / (q - p)
  //   H(z) = ----------- + -----------
  //          1 - p z^-1    1 - q z^-1
  // and has impulse response
  //   h[n] = [p/(p - q)] p^n + [q/(q - p)] q^n  for n >= 0
  // [from the tables in https://en.wikipedia.org/wiki/Z-transform]. Supposing
  // |p| > |q|, the q^n term decays faster, so h[n] is asymptotically just the
  // first term, and h[n] reduces by -20 log10|p| dB per sample,
  //   20 log10|h[n]| ~ n 20 log10|p| + 20 log10|p/(p - q)|  as n -> infinity,
  // where the second term is constant with respect to n.
  //
  // If p and q have equal magnitude but differ in phase, then
  //                        1
  //   H(z) = ------------------------------,
  //          1 - 2 r cos(w) z^-1 + r^2 z^-2
  // where p = r e^iw, q = r e^-iw, and the impulse response is
  //   h[n] = csc(w) sin(w (n + 1)) r^n  for n >= 0.
  // Ignoring the oscillation, the decay rate is again -20 log10|p| dB/sample:
  //   20 log10|h[n]| ~ n 20 log10(r) + 20 log10|csc(w)|.
  //
  // Things are slightly different for a transfer function with a double pole,
  //                1
  //   H(z) = --------------.
  //          (1 - p z^-1)^2
  // The impulse response is
  //   h[n] = (n + 1) p^n  for n >= 0.
  // In dB scale, this adds a log(n) term,
  //   20 log10|h[n]| = n 20 log10|p| + 20 log10(n + 1).
  // For large enough n, the second term changes slowly, and the decay is
  // determined mostly by the first term. So with this detail aside, the decay
  // rate is once again -20 log10|p| dB/sample.
  double decay_db_per_sample = -20.0 * std::log10(
      std::max(std::abs(poles.first), std::abs(poles.second)));
  // Compute the number of samples needed to decay by decay_db dB. We add 2.0
  // samples to the result to account for the biquad numerator.
  return decay_db / decay_db_per_sample + 2.0;
}

std::string BiquadFilterCoefficients::ToString() const {
  return absl::StrCat("{{", absl::StrJoin(b, ", "),
                      "}, {",
                      absl::StrJoin(a, ", "), "}}");
}

bool BiquadFilterCascadeCoefficients::IsStable() const {
  vector<double> k;
  vector<double> v;
  vector<double> b;
  vector<double> a;

  AsPolynomialRatio(&b, &a);
  MakeLadderCoefficientsFromTransferFunction(b, a, &k, &v);
  for (double k_i : k) {
    // Written with negative logic to catch NaNs.
    if (!(std::abs(k_i) < 1)) {
      return false;
    }
  }
  return true;
}

// Squeeze out trivial stages; merge pole-only and zero-only stages together.
void BiquadFilterCascadeCoefficients::Simplify() {
  // Combine first order polynomials, leaving behind a second and zero order
  // polynomial.
  int index, index2;
  while (FindPairOfFirstOrderBiquads(coeffs, &index, &index2, false)) {
    ABSL_CHECK_NE(index, index2);
    coeffs[index].b = Convolve(coeffs[index].b, coeffs[index2].b);
    coeffs[index].b.resize(3);  // It could end up being size 1 or 2.
    coeffs[index2].b = {1, 0, 0};
  }
  while (FindPairOfFirstOrderBiquads(coeffs, &index, &index2, true)) {
    ABSL_CHECK_NE(index, index2);
    coeffs[index].a = Convolve(coeffs[index].a, coeffs[index2].a);
    coeffs[index].a.resize(3);  // It could end up being size 1 or 2.
    coeffs[index2].a = {1, 0, 0};
  }
  // Combine zero order polynomials.
  int a_index, b_index;
  double gain;
  while (size() > 1 && FindZeroOrderPoly(coeffs, &a_index, &b_index, &gain)) {
    if (a_index != b_index) {
      coeffs[b_index].b = coeffs[a_index].b;  // Merge nontrivial b to b_index.
    }
    coeffs.erase(coeffs.begin() + a_index);  // Remove the trivial stage.
    if (gain != 1.0) {
      // Apply the saved gain if it was not 1.0, which it usually is.
      AdjustGain(gain);
    }
  }
}

void BiquadFilterCascadeCoefficients::AsPolynomialRatio(
    vector<double>* b, vector<double>* a) const {
  b->assign(1, 1.0);
  for (int i = 0; i < coeffs.size(); ++i) {
    *b = Convolve(coeffs[i].b, *b);
  }
  a->assign(1, 1.0);
  for (int i = 0; i < coeffs.size(); ++i) {
    *a = Convolve(coeffs[i].a, *a);
  }
}

// Evaluate the transfer function at complex value z,
complex<double> BiquadFilterCascadeCoefficients::EvalTransferFunction(
    const complex<double>& z) const {
  complex<double> transfer_function(1.0, 0.0);
  for (const auto& stage_coeffs : coeffs) {
    transfer_function *= stage_coeffs.EvalTransferFunction(z);
  }
  return transfer_function;
}

std::pair<double, double>
    BiquadFilterCascadeCoefficients::FindPeakFrequencyRadiansPerSample() const {
  auto EvaluateMagnitudeAtFrequency = [this](double freq) {
    return GainMagnitudeAtFrequency(freq);
  };
  return FindPeakByBisection(EvaluateMagnitudeAtFrequency, 0, M_PI);
}

std::string BiquadFilterCascadeCoefficients::ToString() const {
  auto formatter = [](std::string* out, const BiquadFilterCoefficients& coeff) {
    absl::StrAppend(out, coeff.ToString());
  };
  return absl::StrCat("{", absl::StrJoin(coeffs, ", ", formatter), "}");
}

}  // namespace linear_filters
