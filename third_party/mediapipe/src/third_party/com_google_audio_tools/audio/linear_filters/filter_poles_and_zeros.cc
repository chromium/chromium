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

#include "audio/linear_filters/filter_poles_and_zeros.h"

using std::complex;
using std::conj;

namespace linear_filters {
namespace {

constexpr double kConjPairAbsTolerance = 1e-12;
constexpr double kConjPairRelTolerance = 1e-6;

bool AlmostEqual(complex<double> root1, complex<double> root2) {
  double diff = std::abs(root1 - root2);
  const double rel_tolerance =
      std::max(std::abs(root1), std::abs(root2)) * kConjPairRelTolerance;
  return diff <= std::max(kConjPairAbsTolerance, rel_tolerance);
}
// Convert two roots (i.e., poles or zeros) to the coefficients of a quadratic
// polynomial in z^-1.
std::vector<double> CoefficientsFromRootPair(double root1, double root2) {
  return {1.0, -root1 - root2, root1 * root2};
}

std::vector<double> CoefficientsFromRootPair(complex<double> root1,
                                        complex<double> root2) {
  ABSL_DCHECK((std::abs(root1.imag()) <= kConjPairAbsTolerance &&
          std::abs(root2.imag()) <= kConjPairAbsTolerance) ||
         AlmostEqual(root1, conj(root2)))
      << "Roots are not real or a complex conjugate pair.";
  return {1.0, -(root1 + root2).real(), (root1 * root2).real()};
}

// Choose pairs of roots from the lists lone_roots and paired_roots and convert
// them to coeficients for a second order polynomial in z. If a only a single
// root is available, that will be used instead. If no roots are available,
// the trivial polynomial, 1, will be returned.
std::vector<double> GetNextBiquadCoefficients(
    const std::vector<complex<double>>& paired_roots,
    const std::vector<double>& lone_roots,
    int* paired_roots_count,
    int* lone_roots_count) {
  std::vector<double> coeffs;
  // Get two poles if possible, otherwise get one. Remove them from the vector
  // they came from. Try to use paired roots first, then take the lone roots.
  if (*paired_roots_count < paired_roots.size()) {
    coeffs = CoefficientsFromRootPair(paired_roots[*paired_roots_count],
                                      conj(paired_roots[*paired_roots_count]));
    ++(*paired_roots_count);
  } else if (*lone_roots_count + 1 < lone_roots.size()) {
    coeffs =  CoefficientsFromRootPair(lone_roots[*lone_roots_count],
                                       lone_roots[*lone_roots_count + 1]);
    (*lone_roots_count) += 2;
  } else if (*lone_roots_count < lone_roots.size()) {
    coeffs =  CoefficientsFromRootPair(lone_roots[*lone_roots_count], 0);
    ++(*lone_roots_count);
  } else {
    coeffs = {1.0, 0.0, 0.0};
  }
  return coeffs;
}
}  // namespace

void FilterPolesAndZeros::AddPole(double pole_position) {
  lone_poles_.push_back(pole_position);
}

void FilterPolesAndZeros::AddConjugatePolePair(complex<double> pole_position) {
  paired_poles_.push_back(pole_position);
}

void FilterPolesAndZeros::AddZero(double zero_position) {
  lone_zeros_.push_back(zero_position);
}

void FilterPolesAndZeros::AddConjugateZeroPair(complex<double> zero_position) {
  paired_zeros_.push_back(zero_position);
}

std::complex<double> FilterPolesAndZeros::Eval(
    const std::complex<double>& complex_freq) const {
  std::complex<double> response = gain_;
  for (double pole : lone_poles_) {
    response /= pole - complex_freq;
  }
  for (std::complex<double> pole : paired_poles_) {
    response /= (pole - complex_freq) * (std::conj(pole) - complex_freq);
  }
  for (double zero : lone_zeros_) {
    response *= zero - complex_freq;
  }
  for (std::complex<double> zero : paired_zeros_) {
    response *= (zero - complex_freq) * (std::conj(zero) - complex_freq);
  }
  return response;
}

int FilterPolesAndZeros::GetPolesDegree() const {
  return lone_poles_.size() + 2 * paired_poles_.size();
}

int FilterPolesAndZeros::GetZerosDegree() const {
  return lone_zeros_.size() + 2 * paired_zeros_.size();
}

int FilterPolesAndZeros::RelativeDegree() const {
  return GetPolesDegree() - GetZerosDegree();
}

BiquadFilterCascadeCoefficients FilterPolesAndZeros::GetCoefficients() const {
  if (lone_zeros_.empty() && paired_zeros_.empty() &&
      paired_poles_.empty() && lone_poles_.empty()) {
    return BiquadFilterCascadeCoefficients();
  }

  int poles_count = 0;
  int paired_poles_count = 0;
  int zeros_count = 0;
  int paired_zeros_count = 0;

  const int total_roots = lone_zeros_.size() + paired_zeros_.size() +
      lone_poles_.size() + paired_poles_.size();
  BiquadFilterCascadeCoefficients coeffs;
  // Add sets of coefficients while there are still poles and zeros that
  // are unaccounted for.
  while (zeros_count + paired_zeros_count +
         poles_count + paired_poles_count < total_roots) {
    coeffs.AppendBiquad({
        GetNextBiquadCoefficients(paired_zeros_, lone_zeros_,
                                  &paired_zeros_count, &zeros_count),
        GetNextBiquadCoefficients(paired_poles_, lone_poles_,
                                  &paired_poles_count, &poles_count)});
  }
  coeffs.AdjustGain(gain_);
  return coeffs;
}
}  // namespace linear_filters
