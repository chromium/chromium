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

#include "audio/linear_filters/biquad_filter_design.h"

#include <cmath>
#include <limits>

#include "audio/dsp/elliptic_functions.h"
#include "audio/dsp/signal_vector_util.h"
#include "audio/linear_filters/discretization.h"
#include "glog/logging.h"

using audio_dsp::EllipticF;
using audio_dsp::EllipticK;
using audio_dsp::JacobiAmplitude;
using audio_dsp::Square;
using std::complex;

namespace linear_filters {

namespace {

// Change the locations of the poles and zeros with C->C function frequency_map.
// The returned filter has the same scale_factor, but note that mapping poles
// and zeros of a filter changes the gain. The caller must correct the gain.
template <typename Function>
FilterPolesAndZeros MapFrequency(const FilterPolesAndZeros& filter,
                                 Function frequency_map) {
  FilterPolesAndZeros mapped_filter;
  for (double pole : filter.GetRealPoles()) {
    mapped_filter.AddPole(frequency_map(pole).real());
  }
  for (const std::complex<double>& pole : filter.GetConjugatedPoles()) {
    mapped_filter.AddConjugatePolePair(frequency_map(pole));
  }
  for (double zero : filter.GetRealZeros()) {
    mapped_filter.AddZero(frequency_map(zero).real());
  }
  for (const std::complex<double>& zero : filter.GetConjugatedZeros()) {
    mapped_filter.AddConjugateZeroPair(frequency_map(zero));
  }
  mapped_filter.SetGain(filter.GetGain());
  return mapped_filter;
}

// Move poles and zeros from baseband (centered around DC) to being centered
// around new_center_frequency. This operation doubles the order of the filter.
FilterPolesAndZeros ShiftRootsFromBaseband(double new_center_frequency,
                                           FilterPolesAndZeros* filter) {
  FilterPolesAndZeros mapped_filter;
  double center_sq = new_center_frequency * new_center_frequency;
  for (double pole : filter->GetRealPoles()) {
    std::complex<double> pole_sq_minus_f_sq =
        pole * pole - center_sq;
    mapped_filter.AddConjugatePolePair(pole + std::sqrt(pole_sq_minus_f_sq));
  }
  for (const std::complex<double>& pole : filter->GetConjugatedPoles()) {
    std::complex<double> pole_sq_minus_f_sq =
        pole * pole - center_sq;
    mapped_filter.AddConjugatePolePair(pole + std::sqrt(pole_sq_minus_f_sq));
    mapped_filter.AddConjugatePolePair(pole - std::sqrt(pole_sq_minus_f_sq));
  }
  for (double zero : filter->GetRealZeros()) {
    std::complex<double> zero_sq_minus_f_sq = zero * zero - center_sq;
    mapped_filter.AddConjugateZeroPair(zero + std::sqrt(zero_sq_minus_f_sq));
  }
  for (const std::complex<double>& zero : filter->GetConjugatedZeros()) {
    std::complex<double> zero_sq_minus_f_sq = zero * zero - center_sq;
    mapped_filter.AddConjugateZeroPair(zero + std::sqrt(zero_sq_minus_f_sq));
    mapped_filter.AddConjugateZeroPair(zero - std::sqrt(zero_sq_minus_f_sq));
  }
  mapped_filter.SetGain(filter->GetGain());
  return mapped_filter;
}

}  // namespace

BiquadFilterCascadeCoefficients PoleZeroFilterDesign::LowpassCoefficients(
    double sample_rate_hz, double corner_frequency_hz) const {
  ABSL_CHECK_GT(sample_rate_hz, 0.0);
  ABSL_CHECK_GE(corner_frequency_hz, 0.0);
  ABSL_CHECK_LT(corner_frequency_hz, sample_rate_hz / 2);

  // Prewarp the corner frequency and convert to units of rad/s.
  const double prewarped_rad_s =
      BilinearPrewarp(corner_frequency_hz, sample_rate_hz);
  // Change the cutoff frequency from 1 rad/s to prewarped_rad_s by mapping
  // s -> prewarped_rad_s * s.
  FilterPolesAndZeros analog_prototype = GetAnalogPrototype();
  FilterPolesAndZeros mapped_filter = MapFrequency(analog_prototype,
      [prewarped_rad_s](const complex<double>& s) {
        return prewarped_rad_s * s;
      });
  // Counteract the gain change due to the shifting of the poles and zeros.
  const int relative_degree = analog_prototype.RelativeDegree();
  mapped_filter.SetGain(mapped_filter.GetGain() *
                        pow(prewarped_rad_s, relative_degree));
  return BilinearTransform(mapped_filter, sample_rate_hz).GetCoefficients();
}

BiquadFilterCascadeCoefficients PoleZeroFilterDesign::HighpassCoefficients(
    double sample_rate_hz, double corner_frequency_hz) const {
  ABSL_CHECK_GT(sample_rate_hz, 0.0);
  ABSL_CHECK_GE(corner_frequency_hz, 0.0);
  ABSL_CHECK_LT(corner_frequency_hz, sample_rate_hz / 2);

  // Prewarp the corner frequency and convert to units of rad/s.
  const double prewarped_rad_s =
      BilinearPrewarp(corner_frequency_hz, sample_rate_hz);
  // Convert lowpass filter with 1 rad/s cutoff to a highpass filter with cutoff
  // prewarped_rad_s by mapping s -> prewarped_rad_s / s.
  FilterPolesAndZeros analog_prototype = GetAnalogPrototype();
  FilterPolesAndZeros mapped_filter = MapFrequency(analog_prototype,
      [prewarped_rad_s](const complex<double>& s) {
        return prewarped_rad_s / s;
      });
  // Zeros at s=infinity in analog_prototype map to zeros at s=0.
  while (mapped_filter.RelativeDegree() > 0) {
    mapped_filter.AddZero(0.0);
  }
  // Counteract the gain change due to the mapping.
  mapped_filter.SetGain(std::abs(analog_prototype.Eval(0.0)));
  return BilinearTransform(mapped_filter, sample_rate_hz).GetCoefficients();
}

BiquadFilterCascadeCoefficients PoleZeroFilterDesign::BandpassCoefficients(
    double sample_rate_hz, double low_frequency_hz, double high_frequency_hz)
    const {
  ABSL_CHECK_GT(sample_rate_hz, 0.0);
  ABSL_CHECK_GE(low_frequency_hz, 0.0);
  ABSL_CHECK_LT(low_frequency_hz, high_frequency_hz);
  ABSL_CHECK_LT(high_frequency_hz, sample_rate_hz / 2);

  // Prewarp the corner frequencies and convert to units of rad/s.
  const double prewarped_low_rad_s =
      BilinearPrewarp(low_frequency_hz, sample_rate_hz);
  const double prewarped_high_rad_s =
      BilinearPrewarp(high_frequency_hz, sample_rate_hz);
  const double bandwidth_rad_s = prewarped_high_rad_s - prewarped_low_rad_s;
  const double center_rad_s =
      std::sqrt(prewarped_high_rad_s * prewarped_low_rad_s);
  // Convert lowpass filter with 1 rad/s cutoff to a bandpass filter with center
  // center_rad_s by mapping s -> bandwidth_rad_s * s / 2.0.
  FilterPolesAndZeros analog_prototype = GetAnalogPrototype();
  FilterPolesAndZeros mapped_filter = MapFrequency(analog_prototype,
      [bandwidth_rad_s](const complex<double>& s) {
        return bandwidth_rad_s * s / 2.0;
      });

  int degree = mapped_filter.RelativeDegree();

  // Note that this actually doubles the order of the filter.
  mapped_filter = ShiftRootsFromBaseband(center_rad_s, &mapped_filter);
  // Bring half of the zeros at infinity to zero.
  for (int i = 0; i < degree; ++i) {
    mapped_filter.AddZero(0.0);
  }
  // Counteract the gain change due to the shifting of the poles and zeros.
  mapped_filter.SetGain(mapped_filter.GetGain() *
                        pow(bandwidth_rad_s, degree));
  return BilinearTransform(mapped_filter, sample_rate_hz).GetCoefficients();
}

BiquadFilterCascadeCoefficients PoleZeroFilterDesign::BandstopCoefficients(
    double sample_rate_hz, double low_frequency_hz, double high_frequency_hz)
    const {
  ABSL_CHECK_GT(sample_rate_hz, 0.0);
  ABSL_CHECK_GE(low_frequency_hz, 0.0);
  ABSL_CHECK_LT(low_frequency_hz, high_frequency_hz);
  ABSL_CHECK_LT(high_frequency_hz, sample_rate_hz / 2);

  // Prewarp the corner frequency and convert to units of rad/s.
  const double prewarped_low_rad_s =
      BilinearPrewarp(low_frequency_hz, sample_rate_hz);
  const double prewarped_high_rad_s =
      BilinearPrewarp(high_frequency_hz, sample_rate_hz);
  const double bandwidth_rad_s = prewarped_high_rad_s - prewarped_low_rad_s;
  const double center_rad_s =
      std::sqrt(prewarped_high_rad_s * prewarped_low_rad_s);
  // Convert lowpass filter with 1 rad/s cutoff to a bandpass filter with center
  // center_rad_s by mapping s -> bandwidth_rad_s * s / 2.0.
  FilterPolesAndZeros analog_prototype = GetAnalogPrototype();
  FilterPolesAndZeros mapped_filter = MapFrequency(analog_prototype,
      [bandwidth_rad_s](const complex<double>& s) {
        return bandwidth_rad_s / s / 2.0;
      });

  int degree = mapped_filter.RelativeDegree();

  // Note that this actually doubles the order of the filter.
  mapped_filter = ShiftRootsFromBaseband(center_rad_s, &mapped_filter);
  // Bring the zeros at infinity to the stopband.
  for (int i = 0; i < degree; ++i) {
    mapped_filter.AddConjugateZeroPair({0.0, center_rad_s});
  }
  // Counteract the gain change due to the mapping.
  mapped_filter.SetGain(std::abs(analog_prototype.Eval(0.0)));
  return BilinearTransform(mapped_filter, sample_rate_hz).GetCoefficients();
}


FilterPolesAndZeros ButterworthFilterDesign::GetAnalogPrototype() const {
  ABSL_CHECK_GE(order_, 1);
  // The poles of the butterworth filter are placed in a semi-circular pattern
  // around the origin as seen here:
  // http://www.analog.com/media/en/training-seminars/tutorials/MT-224.pdf
  FilterPolesAndZeros analog_prototype;

  const bool order_odd = order_ % 2;
  const double pole_spacing_rads = M_PI / order_;
  for (int i = 0; i < order_ / 2 /* integer division */; ++i) {
    const complex<double> pole =
        std::polar(1.0, M_PI - pole_spacing_rads * (i + 1 - !order_odd * 0.5));
    analog_prototype.AddConjugatePolePair(pole);
  }
  if (order_odd) {
    analog_prototype.AddPole(-1);
  }
  analog_prototype.SetGain(1.0 / std::abs(analog_prototype.Eval(0.0)));
  return analog_prototype;
}

FilterPolesAndZeros ChebyshevType1FilterDesign::GetAnalogPrototype() const {
  ABSL_CHECK_GE(order_, 1);
  ABSL_CHECK_GT(passband_ripple_db_, 0.0);
  FilterPolesAndZeros analog_prototype;
  // The poles of the nth order_ Chebyshev type 1 analog prototype filter are
  // the complex values of s so that
  //   1 + epsilon^2 T_n(-i s)^2 = 0,
  // where T_n(x) = cos(n arccos(x)) is the Chebyshev polynomial of order_ n and
  // epsilon is a quantity computed from passband_ripple_db_. Substituting
  // -i s = cos(theta) and solving for theta obtains
  //   1 + epsilon^2 cos(n theta)^2 = 0
  //   theta = (arccos(+/- i / epsilon) + m pi) / n
  // where m is any integer. The poles in terms of s are
  //   s = i cos((arccos(+/- i / epsilon) + m pi) / n).
  // Through trig identities [e.g., arccos(i z) = pi / 2 - i arcsinh(z)], this
  // formula for the poles can be expressed in several ways. We use
  //   s = -sinh(arcsinh(1 / epsilon) - i pi m / (2 n)),
  // where m is any odd integer for even n or any even integer for odd n.
  const double epsilon = sqrt(pow(10.0, passband_ripple_db_ / 10.0) - 1.0);
  const double mu = asinh(1.0 / epsilon) / order_;

  for (int i = order_ - 1; i >= 0; i -= 2) {
    const double theta = M_PI * i / (2 * order_);
    const complex<double> pole = -sinh(complex<double>(mu, -theta));
    if (i > 0) {
      analog_prototype.AddConjugatePolePair(pole);
    } else {
      analog_prototype.AddPole(pole.real());
    }
  }
  analog_prototype.SetGain(1.0 / std::abs(analog_prototype.Eval(0.0)));
  if (order_ % 2 == 0) {
    analog_prototype.SetGain(analog_prototype.GetGain() /
                             sqrt(1.0 + Square(epsilon)));
  }
  return analog_prototype;
}

FilterPolesAndZeros ChebyshevType2FilterDesign::GetAnalogPrototype() const {
  ABSL_CHECK_GE(order_, 1);
  ABSL_CHECK_GT(stopband_ripple_db_, 0.0);
  FilterPolesAndZeros analog_prototype;
  const double epsilon =
      1.0 / sqrt(pow(10.0, stopband_ripple_db_ / 10.0) - 1.0);
  const double mu = asinh(1.0 / epsilon) / order_;
  for (int i = order_ - 1; i >= 0; i -= 2) {
    const double theta = M_PI * i / (2 * order_);
    const complex<double> pole = -1.0 / sinh(complex<double>(mu, theta));
    if (i > 0) {
      const complex<double> zero(0.0, 1.0 / sin(theta));
      analog_prototype.AddConjugateZeroPair(zero);
      analog_prototype.AddConjugatePolePair(pole);
    } else {
      analog_prototype.AddPole(pole.real());
    }
  }
  analog_prototype.SetGain(1.0 / std::abs(analog_prototype.Eval(0.0)));
  return analog_prototype;
}

namespace {

complex<double> JacobiCD(const complex<double>& u, double m) {
  complex<double> phi = JacobiAmplitude(u, m);
  complex<double> w = cos(phi) / sqrt(1.0 - m * Square(sin(phi)));
  return w;
}

complex<double> JacobiSN(const complex<double>& u, double m) {
  complex<double> w = sin(JacobiAmplitude(u, m));
  return w;
}

complex<double> InverseJacobiSN(complex<double> w, double m) {
  complex<double> u = EllipticF(asin(w), m);
  return u;
}

}  // namespace

FilterPolesAndZeros EllipticFilterDesign::GetAnalogPrototype() const {
  ABSL_CHECK_GE(order_, 1);
  ABSL_CHECK_GT(passband_ripple_db_, 0.0);
  ABSL_CHECK_GT(stopband_ripple_db_, 0.0);

  const int num_conjugate_pairs = order_ / 2;  // Integer division.
  const double epsilon_p = sqrt(pow(10.0, passband_ripple_db_ / 10.0) - 1.0);
  const double epsilon_s = sqrt(pow(10.0, stopband_ripple_db_ / 10.0) - 1.0);
  const double k1 = epsilon_p / epsilon_s;
  const double elliptic_k1 = EllipticK(Square(k1));

  // Solve the degree equation for k.
  const double k1p = sqrt(1.0 - k1 * k1);
  const double elliptic_k1p = EllipticK(Square(k1p));
  double kp = 1.0;
  for (int i = 0; i < num_conjugate_pairs; ++i) {
    kp *= JacobiSN((2.0 * i + 1.0) * elliptic_k1p / order_, Square(k1p)).real();
  }
  kp = pow(k1p, order_) * pow(kp, 4);
  const double k = sqrt(1.0 - kp * kp);
  const double elliptic_k = EllipticK(Square(k));

  const double v0 = (InverseJacobiSN({0, 1.0 / epsilon_p}, Square(k1))).imag() /
      (order_ * elliptic_k1);
  FilterPolesAndZeros analog_prototype;
  for (int i = 0; i < num_conjugate_pairs; ++i) {
    const double u = (2.0 * i + 1.0) * elliptic_k / order_;
    double zeta = JacobiCD(u, Square(k)).real();
    const complex<double> zero(0.0, 1.0 / (k * zeta));
    analog_prototype.AddConjugateZeroPair(zero);

    const complex<double> pole = complex<double>(0, 1) *
        JacobiCD({u, -v0 * elliptic_k}, Square(k));
      analog_prototype.AddConjugatePolePair(pole);
  }

  if (order_ % 2 == 1) {
    const complex<double> pole = complex<double>(0, 1) *
        JacobiSN({0.0, v0 * elliptic_k}, Square(k));
    analog_prototype.AddPole(pole.real());
  }

  double gain = 1.0 / std::abs(analog_prototype.Eval(0.0));
  if (order_ % 2 == 0) {
    gain /= sqrt(1.0 + Square(epsilon_p));
  }
  analog_prototype.SetGain(gain);
  return analog_prototype;
}

}  // namespace linear_filters
