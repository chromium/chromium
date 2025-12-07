#include "audio/dsp/portable/butterworth.h"

#include <math.h>
#include <stdlib.h>

#include "audio/dsp/portable/complex.h"
#include "audio/dsp/portable/math_constants.h"

/* Prewarping for the bilinear transform. */
static double BilinearPrewarp(double frequency_hz, double sample_rate_hz) {
  return 2.0 * sample_rate_hz * tan(M_PI * frequency_hz / sample_rate_hz);
}

/* Evaluate the bilinear transformation (K + s) / (K - s). */
static ComplexDouble BilinearTransform(double K, ComplexDouble s) {
  const double denom = (K - s.real) * (K - s.real) + s.imag * s.imag;
  return ComplexDoubleMake((K * K - s.real * s.real - s.imag * s.imag) / denom,
                           2.0 * K * s.imag / denom);
}

int DesignButterworthOrder2Lowpass(double corner_frequency_hz,
                                   double sample_rate_hz,
                                   BiquadFilterCoeffs* coeffs) {
  if (!(0 < corner_frequency_hz &&
        corner_frequency_hz < sample_rate_hz / 2) ||
      coeffs == NULL) {
    return 0;
  }

  const double prewarped_rad_s =
      BilinearPrewarp(corner_frequency_hz, sample_rate_hz);

  /* The analog prototype for a 2nd-order Butterworth filter is a pair of
   * complex-conjugate poles at s = -1/sqrt(2) +/- i 1/sqrt(2). We scale them to
   * change the cutoff frequency from 1 rad/s to prewarped_rad_s.
   */
  const ComplexDouble pole = ComplexDoubleMake(
      -(1 / M_SQRT2) * prewarped_rad_s,
      (1 / M_SQRT2) * prewarped_rad_s);

  /* Discretize by bilinear transform to the Z plane. */
  const double K = 2 * sample_rate_hz;
  ComplexDouble discretized_pole = BilinearTransform(K, pole);

  /* Calculate gain for unit response at DC. */
  const double gain = 0.25 * ComplexDoubleAbs2(ComplexDoubleMake(
      discretized_pole.real - 1.0, discretized_pole.imag));

  /* Convert from z-plane zero/pole/gain to second-order section. */
  coeffs->b0 = (float) gain;
  coeffs->b1 = (float) (2 * gain);
  coeffs->b2 = (float) gain;
  coeffs->a1 = (float) (-2 * discretized_pole.real);
  coeffs->a2 = (float) ComplexDoubleAbs2(discretized_pole);
  return 1;
}

int DesignButterworthOrder2Highpass(double corner_frequency_hz,
                                    double sample_rate_hz,
                                    BiquadFilterCoeffs* coeffs) {
  if (!(0 < corner_frequency_hz &&
        corner_frequency_hz < sample_rate_hz / 2) ||
      coeffs == NULL) {
    return 0;
  }

  const double prewarped_rad_s =
      BilinearPrewarp(corner_frequency_hz, sample_rate_hz);

  /* The analog prototype for a 2nd-order Butterworth filter is a pair of
   * complex-conjugate poles at s = -1/sqrt(2) +/- i 1/sqrt(2). We scale them to
   * change the cutoff frequency from 1 rad/s to prewarped_rad_s.
   */
  const ComplexDouble pole = ComplexDoubleMake(
      -(1 / M_SQRT2) * prewarped_rad_s,
      (1 / M_SQRT2) * prewarped_rad_s);

  /* Discretize by bilinear transform to the Z plane. */
  const double K = 2 * sample_rate_hz;
  ComplexDouble discretized_pole = BilinearTransform(K, pole);

  /* Calculate gain for unit response at Nyquist. */
  const double gain = -0.25 * (
      1 + 2 * discretized_pole.real + ComplexDoubleAbs2(discretized_pole));

  /* Convert from z-plane zero/pole/gain to second-order section. */
  coeffs->b0 = (float) -gain;
  coeffs->b1 = (float) (2 * gain);
  coeffs->b2 = (float) -gain;
  coeffs->a1 = (float) (-2 * discretized_pole.real);
  coeffs->a2 = (float) ComplexDoubleAbs2(discretized_pole);
  return 1;
}

int DesignButterworthOrder2Bandpass(double low_edge_hz,
                                    double high_edge_hz,
                                    double sample_rate_hz,
                                    BiquadFilterCoeffs* coeffs) {
  if (!(0 < low_edge_hz && low_edge_hz < high_edge_hz &&
        high_edge_hz < sample_rate_hz / 2) ||
      coeffs == NULL) {
    return 0;
  }

  const double prewarped_low_rad_s =
      BilinearPrewarp(low_edge_hz, sample_rate_hz);
  const double prewarped_high_rad_s =
      BilinearPrewarp(high_edge_hz, sample_rate_hz);
  const double bandwidth_rad_s = prewarped_high_rad_s - prewarped_low_rad_s;

  /* The analog prototype for a 2nd-order Butterworth filter is a pair of
   * complex-conjugate poles at s = -1/sqrt(2) +/- i 1/sqrt(2).
   */
  const ComplexDouble pole = ComplexDoubleMake(
      -(0.5 / M_SQRT2) * bandwidth_rad_s,
      (0.5 / M_SQRT2) * bandwidth_rad_s);

  /* Calculate d = sqrt(pole^2 - prewarped_high_rad_s * prewarped_low_rad_s). */
  const ComplexDouble d = ComplexDoubleSqrt(ComplexDoubleSub(
      ComplexDoubleSquare(pole),
      ComplexDoubleMake(prewarped_high_rad_s * prewarped_low_rad_s, 0.0)));

  /* Shift up from baseband to convert to a bandpass filter. There are now
   * two pairs of complex-conjugate poles.
   */
  ComplexDouble shifted_poles[2];
  shifted_poles[0] = ComplexDoubleAdd(pole, d);
  shifted_poles[1] = ComplexDoubleSub(pole, d);

  /* Discretization: poles are mapped by bilinear transform to the Z plane. */
  const double K = 2 * sample_rate_hz;
  ComplexDouble discretized_poles[2];
  int i;
  for (i = 0; i < 2; ++i) {
    discretized_poles[i] = BilinearTransform(K, shifted_poles[i]);
  }

  /* Calculate the gain for the discretized filter. */
  double gain = (bandwidth_rad_s * K) * (bandwidth_rad_s * K);
  for (i = 0; i < 2; ++i) {
    gain /= ComplexDoubleAbs2(
        ComplexDoubleMake(shifted_poles[i].real - K, shifted_poles[i].imag));
  }

  /* Convert from z-plane zero/pole/gain to two second-order sections. */
  coeffs[0].b0 = (float) gain;
  coeffs[0].b1 = (float) (-2 * gain);
  coeffs[0].b2 = (float) gain;
  coeffs[0].a1 = (float) (-2 * discretized_poles[0].real);
  coeffs[0].a2 = (float) ComplexDoubleAbs2(discretized_poles[0]);

  coeffs[1].b0 = 1.0f;
  coeffs[1].b1 = 2.0f;
  coeffs[1].b2 = 1.0f;
  coeffs[1].a1 = (float) (-2 * discretized_poles[1].real);
  coeffs[1].a2 = (float) ComplexDoubleAbs2(discretized_poles[1]);
  return 1;
}
