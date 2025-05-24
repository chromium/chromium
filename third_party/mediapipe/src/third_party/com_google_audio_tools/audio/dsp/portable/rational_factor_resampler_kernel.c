/*
 * Copyright 2020-2021 Google LLC
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

#include "audio/dsp/portable/rational_factor_resampler_kernel.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "audio/dsp/portable/math_constants.h"

/* Evaluate I0(x), the zeroth-order modified Bessel function of the first kind,
 * which has expansion
 *   1 + (x^2/4) / (1!)^2 + (x^2/4)^2 / (2!)^2 + (x^2/4)^3 / (3!)^2 + ...
 * [Abramowitz, Stegun, "Handbook of Mathematical Functions," Chapter 9, p. 375]
 *
 * NOTE: This is reliable only for small to moderately large |x|, |x| <= 20.
 */
static double BesselI0(double x) {
  const double x_squared = x * x;
  double term = 1.0;
  double sum = 1.0;
  int k;
  /* The nth term of the expansion is (x^2/4)^n / (n!)^2, and
   *
   *    nth term     (x^2/4)^n  ((n-1)!)^2     x^2/4     x^2
   *  ------------ = --------- ------------- = ----- = -------.
   *  (n-1)th term    (n!)^2   (x^2/4)^(n-1)    n^2    (2n)^2
   *
   * So we count up by twos, k = 2n where n = 1, 2, ..., and compute successive
   * terms as term *= x^2 / k^2.
   */
  for (k = 2; k < 80; k += 2) {
    term *= x_squared / (k * k);
    if (term < DBL_EPSILON * sum) { break; }
    sum += term;
  }
  return sum;
}

/* Evaluate Kaiser window having nonzero support over -1 <= x <= 1.
 * NOTE: x must be nonnegative. [This allows us to eliminate a fabs() call.]
 * NOTE: This function does not include the 1/I0(beta) normalization factor.
 */
static double KaiserWindow(double x, double kaiser_beta) {
  /* The window is nonzero at the endpoints |x| = 1. To sample there reliably,
   * we adjust the threshold up by 100 ULPs.
   */
  if (x > 1.0 + 100 * DBL_EPSILON) {
    return 0.0;
  }

  double y = 1.0 - x * x;
  if (y < 0.0) { y = 0.0; }
  return BesselI0(kaiser_beta * sqrt(y));
}

int RationalFactorResamplerKernelInit(RationalFactorResamplerKernel* kernel,
                                      float input_sample_rate_hz,
                                      float output_sample_rate_hz,
                                      float filter_radius_factor,
                                      float cutoff_proportion,
                                      float kaiser_beta) {
  if (kernel == NULL ||
      !(input_sample_rate_hz > 0.0f) ||
      !(output_sample_rate_hz > 0.0f) ||
      !(filter_radius_factor > 0.0f) ||
      !(0.0f < cutoff_proportion && cutoff_proportion <= 1.0f) ||
      !(kaiser_beta > 0.0f)) {
    return 0;
  }

  /* Compute resampling factor, > 1 if downsampling. */
  kernel->factor = ((double) input_sample_rate_hz) / output_sample_rate_hz;

  const double factor_max_1 = (kernel->factor > 1.0) ? kernel->factor : 1.0;
  /* Determine the kernel radius in units of input samples. If upsampling
   * (factor < 1), the radius is `filter_radius_factor` input samples. If
   * downsampling (factor > 1), the radius is `filter_radius_factor` *output*
   * samples, which is `filter_radius_factor * factor` input samples.
   */
  kernel->radius = filter_radius_factor * factor_max_1;
  /* Compute the cutoff as a proportion of the input Nyquist frequency
   * [described in the .h file as theta = B / (Fs_in / 2)]. The cutoff frequency
   * is `cutoff_proportion` of the input or output Nyquist frequency, whichever
   * is smaller. For instance cutoff_proportion = 0.9 means the cutoff is 90% of
   * the smaller Nyquist frequency. In units of Hz, the cutoff frequency is
   *
   *   B = cutoff_proportion * min(Fs_in / 2, Fs_out / 2).
   *
   * It is represented by theta as a proportion of input Nyquist:
   *
   *   theta = B / (Fs_in / 2)
   *
   *           cutoff_proportion * min(Fs_in / 2, Fs_out / 2)
   *         = ----------------------------------------------
   *                             Fs_in / 2
   *
   *         = cutoff_proportion * min(1, 1 / factor)
   *         = cutoff_proportion / max(1, factor).
   */
  const double theta = cutoff_proportion / factor_max_1;

  kernel->radians_per_sample = M_PI * theta;
  kernel->normalization = theta / BesselI0(kaiser_beta);
  kernel->kaiser_beta = kaiser_beta;
  return 1;
}

double RationalFactorResamplerKernelEval(
    const RationalFactorResamplerKernel* kernel, double x) {
  x = fabs(x);
  const double omega = kernel->radians_per_sample * x;
  const double sinc = (omega < 1e-8) ? 1.0 : sin(omega) / omega;
  return kernel->normalization * sinc *
      KaiserWindow(x / kernel->radius, kernel->kaiser_beta);
}
