#include "audio/dsp/portable/iir_design.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio/dsp/portable/elliptic_fun.h"
#include "audio/dsp/portable/math_constants.h"

/* Evaluate the bilinear transformation, z = (K + s) / (K - s), discretizing a
 * Laplace domain point s to the Z domain by trapezoidal numerical integration.
 * The constant K should be K = 2 * sample_rate_hz.
 *
 * Reference:
 *   https://en.wikipedia.org/wiki/Bilinear_transform
 */
static ComplexDouble BilinearTransform(ComplexDouble s, double K) {
  const double denom = (K - s.real) * (K - s.real) + s.imag * s.imag;
  return ComplexDoubleMake((K * K - s.real * s.real - s.imag * s.imag) / denom,
                           2.0 * K * s.imag / denom);
}

/* Prewarping for the bilinear transform
 * [https://en.wikipedia.org/wiki/Bilinear_transform#Frequency_warping].
 * Returns the warped frequency in units of rad/s.
 */
static double BilinearPrewarp(double frequency_hz, double sample_rate_hz) {
  return 2.0 * sample_rate_hz * tan(M_PI * frequency_hz / sample_rate_hz);
}

void ZpkInit(Zpk* zpk) {
  zpk->num_zeros = 0;
  zpk->num_poles = 0;
  zpk->gain = 1.0;
}

ComplexDouble ZpkEval(const Zpk* zpk, ComplexDouble complex_freq) {
  ComplexDouble numerator = ComplexDoubleMake(zpk->gain, 0.0);
  int i;
  for (i = 0; i < zpk->num_zeros; ++i) {
    /* numerator *= (complex_freq - zpk->zeros[i]). */
    numerator = ComplexDoubleMul(numerator,
        ComplexDoubleSub(complex_freq, zpk->zeros[i]));
  }
  ComplexDouble denom = ComplexDoubleMake(1.0, 0.0);
  for (i = 0; i < zpk->num_poles; ++i) {
    /* denom *= (complex_freq - zpk->poles[i]). */
    denom = ComplexDoubleMul(denom,
        ComplexDoubleSub(complex_freq, zpk->poles[i]));
  }

  return ComplexDoubleDiv(numerator, denom);
}

int /*bool*/ ZpkBilinearTransform(Zpk* zpk, double sample_rate_hz) {
  if (!(sample_rate_hz > 0.0)) {
    fprintf(stderr, "Error: Invalid sample_rate_hz: %g\n", sample_rate_hz);
    return 0;
  }

  /* Substituting s = K (z - 1) / (z + 1) into the analog transfer function
   *
   *                          (s - zeros[i])
   *   H_a(s) = gain * prod_i --------------
   *                          (s - poles[i])
   *
   * obtains the discretized transfer function
   *
   *   H_d(z) = H_a(K(z-1)/(z+1))
   *
   *                          z - BilinearTransform(zeros[i], K)
   *          = H_a(K) prod_i ----------------------------------.
   *                          z - BilinearTransform(poles[i], K)
   *
   * So the discretized poles and zeros are found by BilinearTransform(), and
   * the discretized filter's gain is H_a(K).
   *
   * Reference:
   * https://en.wikipedia.org/wiki/Bilinear_transform#General_transformation_of_a_continuous-time_IIR_filter
   */
  const double K = 2 * sample_rate_hz;
  zpk->gain = ZpkEval(zpk, ComplexDoubleMake(K, 0.0)).real;

  int i;
  for (i = 0; i < zpk->num_zeros; ++i) {
    zpk->zeros[i] = BilinearTransform(zpk->zeros[i], K);
  }
  for (i = 0; i < zpk->num_poles; ++i) {
    zpk->poles[i] = BilinearTransform(zpk->poles[i], K);
  }

  /* Zeros at s = infinity map to z = -1. */
  for (i = zpk->num_zeros; i < zpk->num_poles; ++i) {
    zpk->zeros[i] = ComplexDoubleMake(-1.0, 0.0);
  }
  /* Poles at s = infinity map to z = -1. */
  for (i = zpk->num_poles; i < zpk->num_zeros; ++i) {
    zpk->poles[i] = ComplexDoubleMake(-1.0, 0.0);
  }
  const int max_num = (zpk->num_poles > zpk->num_zeros)
      ? zpk->num_poles : zpk->num_zeros;
  zpk->num_zeros = max_num;
  zpk->num_poles = max_num;
  return 1;
}

/* Discretize Zpk and convert to biquads. */
static int /*bool*/ DiscretizeToBiquads(Zpk* zpk,
                                        double sample_rate_hz,
                                        BiquadFilterCoeffs* coeffs,
                                        int max_biquads) {
  if (!ZpkBilinearTransform(zpk, sample_rate_hz)) { return 0; }
  return ZpkToBiquads(zpk, coeffs, max_biquads);
}

/* Shifts DC to center_rad_s by the mapping s -> (s^2 + ceter_rad_s^2) / (2s).
 * Returns 1 on success, 0 on failure.
 *
 * NOTE: This operation doubles the order of the filter. Each root maps to a
 * pair of roots, r +/- sqrt(r^2 - center_rad_s^2).
 */
static int /*bool*/ ShiftFromBaseband(Zpk* zpk, double center_rad_s) {
  if (2 * zpk->num_zeros > kZpkMaxDegree ||
      2 * zpk->num_poles > kZpkMaxDegree) {
    fprintf(stderr, "Error: Filter degree is too high.\n");
    return 0;
  }

  const int relative_degree = zpk->num_poles - zpk->num_zeros;
  const double center_sq = center_rad_s * center_rad_s;
  int i;
  for (i = zpk->num_zeros - 1; i >= 0; --i) {
    /* Compute zeros[i] -/+ sqrt(zeros[i]^2 - center_rad_s^2). */
    const ComplexDouble root = zpk->zeros[i];
    const ComplexDouble temp = ComplexDoubleSqrt(ComplexDoubleMake(
        root.real * root.real - root.imag * root.imag - center_sq,
        2 * root.real * root.imag));
    zpk->zeros[2 * i] = ComplexDoubleSub(root, temp);
    zpk->zeros[2 * i + 1] = ComplexDoubleAdd(root, temp);
  }
  zpk->num_zeros *= 2;

  for (i = zpk->num_poles - 1; i >= 0; --i) {
    /* Compute poles[i] -/+ sqrt(poles[i]^2 - center_rad_s^2). */
    const ComplexDouble root = zpk->poles[i];
    const ComplexDouble temp = ComplexDoubleSqrt(ComplexDoubleMake(
        root.real * root.real - root.imag * root.imag - center_sq,
        2 * root.real * root.imag));
    zpk->poles[2 * i] = ComplexDoubleSub(root, temp);
    zpk->poles[2 * i + 1] = ComplexDoubleAdd(root, temp);
  }
  zpk->num_poles *= 2;

  /* Bring half of the zeros at infinity to zero. */
  if (relative_degree > 0) {
    for (i = 0; i < relative_degree; ++i) {
      zpk->zeros[zpk->num_zeros + i] = ComplexDoubleMake(0.0, 0.0);
    }
    zpk->num_zeros += relative_degree;
  }
  return 1;
}

void ZpkAnalogPrototypeToLowpass(Zpk* zpk, double cutoff_rad_s) {
  /* Change the filter's cutoff frequency from 1 rad/s to `cutoff_rad_s` by
   * substituting s / cutoff_rad_s -> s:
   *
   *                     prod_i (s / c - zeros[i])
   *   H(s / c) = gain * -------------------------
   *                     prod_i (s / c - poles[i])
   *
   *                     c^num_poles   prod_i (s - zeros[i] * c)
   *            = gain * ----------- * -------------------------.
   *                     c^num_zeros   prod_i (s - poles[i] * c)
   */
  zpk->gain *= pow(cutoff_rad_s, zpk->num_poles - zpk->num_zeros);

  int i;
  for (i = 0; i < zpk->num_zeros; ++i) { /* Compute zeros[i] * cutoff_rad_s. */
    zpk->zeros[i] = ComplexDoubleMulReal(zpk->zeros[i], cutoff_rad_s);
  }
  for (i = 0; i < zpk->num_poles; ++i) { /* Compute zeros[i] * cutoff_rad_s. */
    zpk->poles[i] = ComplexDoubleMulReal(zpk->poles[i], cutoff_rad_s);
  }
}

/* Converts analog prototype to lowpass filter and discretizes to biquads. */
static int /*bool*/ ZpkDesignLowpass(Zpk* zpk,
                                     double cutoff_hz,
                                     double sample_rate_hz,
                                     BiquadFilterCoeffs* coeffs,
                                     int max_biquads) {
  if (!(0 <= cutoff_hz && cutoff_hz < sample_rate_hz / 2)) {
    fprintf(stderr, "Error: Must have 0 <= cutoff_hz < sample_rate_hz / 2.");
    return 0;
  }

  const double warped_rad_s = BilinearPrewarp(cutoff_hz, sample_rate_hz);
  ZpkAnalogPrototypeToLowpass(zpk, warped_rad_s);
  return DiscretizeToBiquads(zpk, sample_rate_hz, coeffs, max_biquads);
}

void ZpkAnalogPrototypeToHighpass(Zpk* zpk, double cutoff_rad_s) {
  /* Convert lowpass analog prototype with 1 rad/s cutoff to a highpass filter
   * with cutoff `cutoff_rad_s` by mapping cutoff_rad_s / s -> s:
   *
   *                            c / s - zeros[i]
   *   H(c / s) = gain * prod_i ----------------
   *                            c / s - poles[i]
   *
   *                            zeros[i]   s - c / zeros[i]
   *            = gain * prod_i -------- * ----------------
   *                            poles[i]   s - c / poles[i]
   *
   *                            s - c / zeros[i]
   *            = H(0) * prod_i ----------------.
   *                            s - c / poles[i]
   */
  zpk->gain = ZpkEval(zpk, ComplexDoubleMake(0.0, 0.0)).real;

  int i;
  for (i = 0; i < zpk->num_zeros; ++i) { /* Compute cutoff_rad_s / zeros[i]. */
    zpk->zeros[i] = ComplexDoubleDiv(
        ComplexDoubleMake(cutoff_rad_s, 0.0), zpk->zeros[i]);
  }
  for (i = 0; i < zpk->num_poles; ++i) { /* Compute cutoff_rad_s / poles[i]. */
    zpk->poles[i] = ComplexDoubleDiv(
        ComplexDoubleMake(cutoff_rad_s, 0.0), zpk->poles[i]);
  }

  /* Zeros at s=infinity in the analog prototype map to zeros at s=0. */
  if (zpk->num_zeros < zpk->num_poles) {
    for (i = zpk->num_zeros; i < zpk->num_poles; ++i) {
      zpk->zeros[i] = ComplexDoubleMake(0.0, 0.0);
    }
    zpk->num_zeros = zpk->num_poles;

  }
}

/* Converts analog prototype to highpass filter and discretizes to biquads. */
static int /*bool*/ ZpkDesignHighpass(Zpk* zpk,
                                      double cutoff_hz,
                                      double sample_rate_hz,
                                      BiquadFilterCoeffs* coeffs,
                                      int max_biquads) {
  if (!(0 <= cutoff_hz && cutoff_hz < sample_rate_hz / 2)) {
    fprintf(stderr, "Error: Must have 0 <= cutoff_hz < sample_rate_hz / 2.");
    return 0;
  }

  const double warped_rad_s = BilinearPrewarp(cutoff_hz, sample_rate_hz);
  ZpkAnalogPrototypeToHighpass(zpk, warped_rad_s);
  return DiscretizeToBiquads(zpk, sample_rate_hz, coeffs, max_biquads);
}

int ZpkAnalogPrototypeToBandpass(Zpk* zpk,
                                 double low_edge_rad_s,
                                 double high_edge_rad_s) {
  /* Change the cutoff frequency from 1 rad/s to bandwidth_rad_s / 2 by mapping
   * s -> (bandwidth_rad_s / 2) * s.
   */
  const double bandwidth_rad_s = high_edge_rad_s - low_edge_rad_s;
  ZpkAnalogPrototypeToLowpass(zpk, bandwidth_rad_s / 2.0);
  zpk->gain *= pow(2.0, zpk->num_poles - zpk->num_zeros);

  /* Shift DC to center_rad_s. This step doubles the order of the filter. */
  const double center_rad_s = sqrt(low_edge_rad_s * high_edge_rad_s);
  return ShiftFromBaseband(zpk, center_rad_s);
}

/* Converts analog prototype to bandpass filter and discretizes to biquads. */
static int /*bool*/ ZpkDesignBandpass(Zpk* zpk,
                                      double low_edge_hz,
                                      double high_edge_hz,
                                      double sample_rate_hz,
                                      BiquadFilterCoeffs* coeffs,
                                      int max_biquads) {
  if (!(0 <= low_edge_hz && low_edge_hz <= high_edge_hz &&
        high_edge_hz < sample_rate_hz / 2)) {
    fprintf(stderr, "Error: Must have "
            "0 <= low_edge_hz <= high_edge_hz < sample_rate_hz / 2.");
    return 0;
  }

  double warped_low_rad_s = BilinearPrewarp(low_edge_hz, sample_rate_hz);
  double warped_high_rad_s = BilinearPrewarp(high_edge_hz, sample_rate_hz);
  ZpkAnalogPrototypeToBandpass(zpk, warped_low_rad_s, warped_high_rad_s);
  return DiscretizeToBiquads(zpk, sample_rate_hz, coeffs, max_biquads);
}

int ZpkAnalogPrototypeToBandstop(Zpk* zpk,
                                 double low_edge_rad_s,
                                 double high_edge_rad_s) {
  /* Change the cutoff frequency from 1 rad/s to bandwidth_rad_s / 2 by mapping
   * s -> (bandwidth_rad_s / 2) / s.
   */
  const double bandwidth_rad_s = high_edge_rad_s - low_edge_rad_s;
  ZpkAnalogPrototypeToHighpass(zpk, bandwidth_rad_s / 2.0);

  /* Shift DC to center_rad_s. This step doubles the order of the filter. */
  const double center_rad_s = sqrt(low_edge_rad_s * high_edge_rad_s);
  return ShiftFromBaseband(zpk, center_rad_s);
}

/* Converts analog prototype to bandstop filter and discretizes to biquads. */
static int ZpkDesignBandstop(Zpk* zpk,
                             double low_edge_hz,
                             double high_edge_hz,
                             double sample_rate_hz,
                             BiquadFilterCoeffs* coeffs,
                             int max_biquads) {
  if (!(0 <= low_edge_hz && low_edge_hz <= high_edge_hz &&
        high_edge_hz < sample_rate_hz / 2)) {
    fprintf(stderr, "Error: Must have "
            "0 <= low_edge_hz <= high_edge_hz < sample_rate_hz / 2.");
    return 0;
  }

  double warped_low_rad_s = BilinearPrewarp(low_edge_hz, sample_rate_hz);
  double warped_high_rad_s = BilinearPrewarp(high_edge_hz, sample_rate_hz);
  ZpkAnalogPrototypeToBandstop(zpk, warped_low_rad_s, warped_high_rad_s);
  return DiscretizeToBiquads(zpk, sample_rate_hz, coeffs, max_biquads);
}

/* qsort comparator for ComplexDoubles. It orders first by real part and second
 * by absolute value of imaginary part such that sorting groups the complex
 * conjugate pairs together.
 */
static int OrderComplex(const void* w, const void* z) {
  const double w_real = ((ComplexDouble*)w)->real;
  const double z_real = ((ComplexDouble*)z)->real;
  if (w_real < z_real) { return -1; }
  if (w_real > z_real) { return 1; }
  const double w_abs_imag = fabs(((ComplexDouble*)w)->imag);
  const double z_abs_imag = fabs(((ComplexDouble*)z)->imag);
  if (w_abs_imag < z_abs_imag) { return -1; }
  if (w_abs_imag > z_abs_imag) { return 1; }
  return 0; /* Values are equal. */
}

/* Sort an array of complex-valued roots and pair complex conjugate pairs.
 * Returns 1 on success, 0 on failure (if roots can't be paired).
 */
static int /*bool*/ RootsSort(ComplexDouble* roots, int num_roots) {
  if (num_roots == 0) { return 1; }
  /* Put real-valued roots before complex conjugate pairs.
   * NOTE: NextQuadraticFactor() iterates roots in reverse and relies on this
   * sort order to handle complex roots in conjugate pairs.
   */
  const double kTol = 100 * DBL_EPSILON;
  int num_real = 0;
  int i;
  for (i = 0; i < num_roots; ++i) {
    ComplexDouble root = roots[i];
    if (fabs(root.imag) <= kTol * ComplexDoubleAbs(root)) {
      if (i != num_real) {
        roots[i] = roots[num_real];
        root.imag = 0.0;
        roots[num_real] = root;
      }
      ++num_real;
    }
  }

  /* Sort real roots in ascending order. */
  qsort(roots, num_real, sizeof(ComplexDouble), OrderComplex);

  roots += num_real;  /* Real roots are done; now we work on remaining roots. */
  num_roots -= num_real;
  if (num_roots == 0) { return 1; }  /* All roots are real. */

  /* Sort complex roots by real part then absolute value of imaginary part. */
  qsort(roots, num_roots, sizeof(ComplexDouble), OrderComplex);

  for (i = 0; i < num_roots;) {
    ComplexDouble root = roots[i];
    root.imag = fabs(root.imag);

    /* Loop over a run of roots where real part and abs imag part are equal. */
    const int start = i;
    int num_neg = 0;
    for (; i < num_roots && !OrderComplex(&root, &roots[i]); ++i) {
      if (roots[i].imag < 0.0) {
        ++num_neg;  /* Count roots where imag part is negative vs. positive. */
      }
    }
    const int end = i;

    /* Check that the run [start, end) is nonempty (no NaNs) and has matching
     * number of roots where imag part is negative vs. positive.
     */
    const int num_pos = (end - start) - num_neg;
    if (start == end || num_pos != num_neg) {
      fprintf(stderr, "Error: Complex roots can't be paired.\n");
      return 0;
    }

    /* Replace run with [root*, root, root*, root, ...]. */
    for (i = start; i < end; i += 2) {
      roots[i] = ComplexDoubleConj(root);
      roots[i + 1] = root;
    }
  }
  return 1;
}

int /*bool*/ ZpkSort(Zpk* zpk) {
  return RootsSort(zpk->zeros, zpk->num_zeros) &&
      RootsSort(zpk->poles, zpk->num_poles);
}

/* Helper for ZpkToBiquads to iterate an array of roots. `*index` iterates the
 * array in reverse from the end of the array to the beginning, consuming two
 * roots at a time. It is assumed that the  roots have been sorted by RootsSort.
 *
 * If two or more roots r1, r2 remain, they are expanded to a quadratic factor
 *
 *   (z - r1)(z - r2) = z^2 - (r1 + r2) z + r1 * r2
 *                    = z^2 + c1 z + c2.
 *
 * Assuming r1 and r2 are a conjugate pair or that r1 and r2 are both real,
 * the quadratic factor coefficients c1, c2 are real.
 */
static void NextQuadraticFactor(const ComplexDouble* roots,
    int* index, double* c1, double* c2) {
  if (*index < 0) { /* Don't have any more roots. */
    *c1 = 0.0;
    *c2 = 0.0;
  } else if (*index == 0) { /* One (necessarily real) root remaining. */
    *c1 = -roots[*index].real;
    *c2 = 0.0;
    --*index;
  } else { /* Two or more roots remaining. */
    /* Expand (z - r1)(z - r2) = z^2 + c1 z + c2. */
    const ComplexDouble r1 = roots[*index];
    const ComplexDouble r2 = roots[*index - 1];
    *c1 = -r1.real - r2.real;
    *c2 = r1.real * r2.real - r1.imag * r2.imag;
    *index -= 2;
  }
}

int /*bool*/ ZpkToBiquads(Zpk* zpk, BiquadFilterCoeffs* coeffs,
                          int max_biquads) {
  if (coeffs == NULL || max_biquads <= 0) { return 0; }
  if (zpk->num_zeros == 0 && zpk->num_poles == 0) {
    coeffs[0].b0 = (float)zpk->gain;
    coeffs[0].b1 = 0.0f;
    coeffs[0].b2 = 0.0f;
    coeffs[0].a1 = 0.0f;
    coeffs[0].a2 = 0.0f;
    return 1;
  }

  const int degree =
      (zpk->num_zeros > zpk->num_poles) ? zpk->num_zeros : zpk->num_poles;
  const int num_biquads = (degree + 1) / 2;
  if (num_biquads > max_biquads) {
    fprintf(stderr, "Error: Output of %d biquads exceeds max_biquads %d.\n",
        num_biquads, max_biquads);
    return 0;
  }

  if (!ZpkSort(zpk)) { return 0; }

  int pole_index = degree - 1;
  int zero_index = degree - 1;
  int i;
  for (i = 0; i < num_biquads; ++i) {
    double b0 = 1.0, b1, b2, a1, a2;
    NextQuadraticFactor(zpk->zeros, &zero_index, &b1, &b2);
    NextQuadraticFactor(zpk->poles, &pole_index, &a1, &a2);

    if (i == num_biquads - 1) {  /* Absorb gain into the last stage. */
      b0 *= zpk->gain;
      b1 *= zpk->gain;
      b2 *= zpk->gain;
    }

    coeffs[i].b0 = (float)b0;
    coeffs[i].b1 = (float)b1;
    coeffs[i].b2 = (float)b2;
    coeffs[i].a1 = (float)a1;
    coeffs[i].a2 = (float)a2;
  }

  return num_biquads;
}

/* Make the Butterworth analog prototype (lowpass filter with 1 rad/s cutoff).
 * Returns 1 on success, 0 on failure.
 */
static int /*bool*/ ButterworthAnalogPrototype(int order, Zpk* zpk) {
  if (order < 1) {
    fprintf(stderr, "Error: Invalid filter order %d\n", order);
    return 0;
  }

  zpk->num_zeros = 0;
  zpk->num_poles = order;
  zpk->gain = 1.0;

  ComplexDouble* poles = zpk->poles;
  const int even_order = (order % 2 == 0);
  /* The N-th order Butterworth analog prototype has poles equally spaced on the
   * unit circle in the left half plane,
   *
   *   s = exp(i pi (2 n + N - 1) / (2 N)) for n = 1, ..., N.
   *
   * https://en.wikipedia.org/wiki/Butterworth_filter
   */
  if (!even_order) {
    *poles++ = ComplexDoubleMake(-1.0, 0.0);
  }

  int n;
  for (n = 1; n <= order / 2 /* integer division */; ++n) {
    const double theta = (M_PI / order) * (n - even_order * 0.5);
    /* Compute complex exp(-i theta). */
    const ComplexDouble pole = ComplexDoubleMake(-cos(theta), sin(theta));
    *poles++ = ComplexDoubleConj(pole); /* Make complex conjugate pair. */
    *poles++ = pole;
  }

  /* Set unit DC gain. */
  zpk->gain = 1.0 / ComplexDoubleAbs(ZpkEval(zpk, ComplexDoubleMake(0.0, 0.0)));
  return 1;
}

int DesignButterworthLowpass(int order,
                             double cutoff_hz,
                             double sample_rate_hz,
                             BiquadFilterCoeffs* coeffs,
                             int max_biquads) {
  Zpk zpk;
  if (!ButterworthAnalogPrototype(order, &zpk)) { return 0; }
  return ZpkDesignLowpass(&zpk, cutoff_hz, sample_rate_hz, coeffs, max_biquads);
}

int DesignButterworthHighpass(int order,
                              double cutoff_hz,
                              double sample_rate_hz,
                              BiquadFilterCoeffs* coeffs,
                              int max_biquads) {
  Zpk zpk;
  if (!ButterworthAnalogPrototype(order, &zpk)) { return 0; }
  return ZpkDesignHighpass(&zpk, cutoff_hz, sample_rate_hz,
                           coeffs, max_biquads);
}

int DesignButterworthBandpass(int order,
                              double low_edge_hz,
                              double high_edge_hz,
                              double sample_rate_hz,
                              BiquadFilterCoeffs* coeffs,
                              int max_biquads) {
  Zpk zpk;
  if (!ButterworthAnalogPrototype(order, &zpk)) { return 0; }
  return ZpkDesignBandpass(&zpk, low_edge_hz, high_edge_hz,
      sample_rate_hz, coeffs, max_biquads);
}

int DesignButterworthBandstop(int order,
                              double low_edge_hz,
                              double high_edge_hz,
                              double sample_rate_hz,
                              BiquadFilterCoeffs* coeffs,
                              int max_biquads) {
  Zpk zpk;
  if (!ButterworthAnalogPrototype(order, &zpk)) { return 0; }
  return ZpkDesignBandstop(&zpk, low_edge_hz, high_edge_hz,
      sample_rate_hz, coeffs, max_biquads);
}

/* Make the Chebyshev type 1 analog prototype. */
static int /*bool*/ Chebyshev1AnalogPrototype(
    int order, double passband_ripple_db, Zpk* zpk) {
  if (order < 1) {
    fprintf(stderr, "Error: Invalid filter order %d\n", order);
    return 0;
  }

  zpk->num_zeros = 0;
  zpk->num_poles = order;
  zpk->gain = 1.0;

  ComplexDouble* poles = zpk->poles;

  /* The Chebyshev type 1 analog prototype has response
   *
   *   |H(iw)| = 1 / sqrt[1 + (epsilon T_N(w))^2]
   *
   * where T_N is the Nth Chebyshev polynomial and epsilon is the "ripple
   * factor". The response is equiripple in the passband, with a max gain of 1
   * and min gain of 1 / sqrt(1 + epsilon^2) = 10^(-passband_ripple_db/20).
   *
   * Considered with complex frequency s, the poles of this response occur where
   * 1 + (epsilon T_N(-is))^2 = 0, which can be shown to be
   *
   *   s = -sinh(mu + i theta),
   *
   * for
   *
   *   mu = arcsinh(1 / epsilon) / N,
   *   theta = (pi / 2) ((2 n - 1) / N - 1) for n = 1, ..., N.
   *
   * Reference:
   * https://en.wikipedia.org/wiki/Chebyshev_filter#Type_I_Chebyshev_filters_(Chebyshev_filters)
   */
  const double epsilon = sqrt(exp((M_LN10 / 10) * passband_ripple_db) - 1.0);
  /* Compute arcsinh by the equality arcsinh(x) = log(x + sqrt(x^2 + 1)). */
  const double mu = log((1 + sqrt(1 + epsilon * epsilon)) / epsilon) / order;
  const int even_order = (order % 2 == 0);
  if (!even_order) { /* For odd order, there is one pole on the real axis. */
    *poles++ = ComplexDoubleMake(-sinh(mu), 0.0);
  }

  /* Generate all other poles in complex conjugate pairs. */
  int n;
  for (n = 1; n <= order / 2 /* integer division */; ++n) {
    const double theta = (M_PI / order) * (n - even_order * 0.5);
    /* Compute pole locations -sinh(mu + i theta). */
    const ComplexDouble pole =
        ComplexDoubleNeg(ComplexDoubleSinh(ComplexDoubleMake(mu, theta)));
    *poles++ = ComplexDoubleConj(pole);
    *poles++ = pole;
  }

  /* Set unit DC gain. */
  zpk->gain = 1.0 / ComplexDoubleAbs(ZpkEval(zpk, ComplexDoubleMake(0.0, 0.0)));
  if (even_order) { /* Small adjustment for passband ripple if order is even. */
    zpk->gain /= sqrt(1.0 + epsilon * epsilon);
  }
  return 1;
}

int DesignChebyshev1Lowpass(int order,
                            double passband_ripple_db,
                            double cutoff_hz,
                            double sample_rate_hz,
                            BiquadFilterCoeffs* coeffs,
                            int max_biquads) {
  Zpk zpk;
  if (!Chebyshev1AnalogPrototype(order, passband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignLowpass(&zpk, cutoff_hz, sample_rate_hz, coeffs, max_biquads);
}

int DesignChebyshev1Highpass(int order,
                             double passband_ripple_db,
                             double cutoff_hz,
                             double sample_rate_hz,
                             BiquadFilterCoeffs* coeffs,
                             int max_biquads) {
  Zpk zpk;
  if (!Chebyshev1AnalogPrototype(order, passband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignHighpass(&zpk, cutoff_hz, sample_rate_hz,
                           coeffs, max_biquads);
}

int DesignChebyshev1Bandpass(int order,
                             double passband_ripple_db,
                             double low_edge_hz,
                             double high_edge_hz,
                             double sample_rate_hz,
                             BiquadFilterCoeffs* coeffs,
                             int max_biquads) {
  Zpk zpk;
  if (!Chebyshev1AnalogPrototype(order, passband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignBandpass(&zpk, low_edge_hz, high_edge_hz,
      sample_rate_hz, coeffs, max_biquads);
}

int DesignChebyshev1Bandstop(int order,
                             double passband_ripple_db,
                             double low_edge_hz,
                             double high_edge_hz,
                             double sample_rate_hz,
                             BiquadFilterCoeffs* coeffs,
                             int max_biquads) {
  Zpk zpk;
  if (!Chebyshev1AnalogPrototype(order, passband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignBandstop(&zpk, low_edge_hz, high_edge_hz,
      sample_rate_hz, coeffs, max_biquads);
}

/* Make the Chebyshev type 2 analog prototype. */
static int /*bool*/ Chebyshev2AnalogPrototype(
    int order, double stopband_ripple_db, Zpk* zpk) {
  if (order < 1) {
    fprintf(stderr, "Error: Invalid filter order %d\n", order);
    return 0;
  }

  zpk->num_zeros = order - order % 2;
  zpk->num_poles = order;
  zpk->gain = 1.0;

  ComplexDouble* zeros = zpk->zeros;
  ComplexDouble* poles = zpk->poles;

  /* The stopband has equiripple response with a maximum gain of
   * (1 + 1 / epsilon^2)^-1/2. The poles are at the reciprocal (1/s) location
   * of the poles of the Chebyshev type 1 analog prototype.
   *
   * The type 2 filter also has zeros, occuring at the zeros of T_N(-1/is):
   *
   *   s = i csc(theta),
   *   theta = (pi / 2) ((2 n - 1) / N - 1) for n = 1, ..., N.
   *
   * Reference:
   * https://en.wikipedia.org/wiki/Chebyshev_filter#Type_II_Chebyshev_filters_(inverse_Chebyshev_filters)
   */
  const double epsilon = sqrt(exp((M_LN10 / 10) * stopband_ripple_db) - 1.0);
  const double mu = log(epsilon + sqrt(1.0 + epsilon * epsilon)) / order;
  const int even_order = (order % 2 == 0);
  if (!even_order) {
    *poles++ = ComplexDoubleMake(-1.0 / sinh(mu), 0.0);
  }

  int n;
  for (n = 1; n <= order / 2 /* integer division */; ++n) {
    const double theta = (M_PI / order) * (n - even_order * 0.5);
    const ComplexDouble zero = ComplexDoubleMake(0.0, 1.0 / sin(theta));
    *zeros++ = ComplexDoubleConj(zero); /* Make complex conjugate pair. */
    *zeros++ = zero;
    /* Compute -1 / sinh(mu + i theta). */
    const ComplexDouble pole = ComplexDoubleDiv(
        ComplexDoubleMake(-1.0, 0.0),
        ComplexDoubleSinh(ComplexDoubleMake(mu, theta)));
    *poles++ = ComplexDoubleConj(pole); /* Make complex conjugate pair. */
    *poles++ = pole;
  }

  /* Set unit DC gain. */
  zpk->gain = 1.0 / ComplexDoubleAbs(ZpkEval(zpk, ComplexDoubleMake(0.0, 0.0)));
  return 1;
}

int DesignChebyshev2Lowpass(int order,
                            double stopband_ripple_db,
                            double cutoff_hz,
                            double sample_rate_hz,
                            BiquadFilterCoeffs* coeffs,
                            int max_biquads) {
  Zpk zpk;
  if (!Chebyshev2AnalogPrototype(order, stopband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignLowpass(&zpk, cutoff_hz, sample_rate_hz, coeffs, max_biquads);
}

int DesignChebyshev2Highpass(int order,
                             double stopband_ripple_db,
                             double cutoff_hz,
                             double sample_rate_hz,
                             BiquadFilterCoeffs* coeffs,
                             int max_biquads) {
  Zpk zpk;
  if (!Chebyshev2AnalogPrototype(order, stopband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignHighpass(&zpk, cutoff_hz, sample_rate_hz, coeffs, max_biquads);
}

int DesignChebyshev2Bandpass(int order,
                             double stopband_ripple_db,
                             double low_edge_hz,
                             double high_edge_hz,
                             double sample_rate_hz,
                             BiquadFilterCoeffs* coeffs,
                             int max_biquads) {
  Zpk zpk;
  if (!Chebyshev2AnalogPrototype(order, stopband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignBandpass(&zpk, low_edge_hz, high_edge_hz,
      sample_rate_hz, coeffs, max_biquads);
}

int DesignChebyshev2Bandstop(int order,
                             double stopband_ripple_db,
                             double low_edge_hz,
                             double high_edge_hz,
                             double sample_rate_hz,
                             BiquadFilterCoeffs* coeffs,
                             int max_biquads) {
  Zpk zpk;
  if (!Chebyshev2AnalogPrototype(order, stopband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignBandstop(&zpk, low_edge_hz, high_edge_hz,
      sample_rate_hz, coeffs, max_biquads);
}

/* Needed for the elliptic analog prototype, implement the "cd" and "sn" Jacobi
 * elliptic functions [https://en.wikipedia.org/wiki/Jacobi_elliptic_functions]:
 *
 *           cn(u)           cos(phi)
 *   cd(u) = ----- = ------------------------,
 *           dn(u)   sqrt(1 - sin(phi)^2 * m)
 *
 *   sn(u) = sin(phi),
 *
 * where phi is the the Jacobi amplitude of u.
 */

static ComplexDouble JacobiCD(ComplexDouble u, double m) {
  ComplexDouble phi = JacobiAmplitude(u, m);
  /* Compute w = cos(phi) / sqrt(1 - sin(phi)^2 * m). */
  ComplexDouble w = ComplexDoubleDiv(
      ComplexDoubleCos(phi),
      ComplexDoubleSqrt(ComplexDoubleSub(
          ComplexDoubleMake(1.0, 0.0),
          ComplexDoubleMulReal(ComplexDoubleSquare(ComplexDoubleSin(phi)),
                               m))));
  return w;
}

static ComplexDouble JacobiSN(ComplexDouble u, double m) {
  ComplexDouble w = ComplexDoubleSin(JacobiAmplitude(u, m));
  return w;
}

static ComplexDouble InverseJacobiSN(ComplexDouble w, double m) {
  ComplexDouble u = EllipticF(ComplexDoubleASin(w), m);
  return u;
}

/* Given modulus k1, solve the degree equation for k,
 *
 *   N EllipticK(k') / EllipticK(k) = EllipticK(k1') / EllipticK(k1),
 *
 * where prime ' denotes complementary modulus.
 */
static double SolveEllipticDegreeEquation(int order, double k1) {
  const double k1p = sqrt(1.0 - k1 * k1);
  const double elliptic_k1p = EllipticK(k1p * k1p);
  /* Compute k' = (k1')^N prod_i=1^N/2 (sn(u_i K1', k1'))^4. This is equation
   * (47) in Orfanidis' notes linked below.
   */
  double kp = 1.0;
  int n;
  for (n = 0; n < order / 2; ++n) {
    ComplexDouble u = ComplexDoubleMake(
        (2 * n + 1) * elliptic_k1p / order, 0.0);
    kp *= JacobiSN(u, k1p * k1p).real;
  }
  kp = pow(k1p, order) * pow(kp, 4);
  return sqrt(1.0 - kp * kp); /* Convert k' to complementary modulus k. */
}

/* Make the elliptic filter analog prototype, following
 *
 * Sophocles J. Orfanidis, "Lecture Notes on Elliptic Filter Design," 2006.
 * https://pdfs.semanticscholar.org/1131/3b5276e5deb428afb7dc208c9120460fbc4a.pdf
 */
static int EllipticAnalogPrototype(int order, double passband_ripple_db,
                                   double stopband_ripple_db, Zpk* zpk) {
  if (order < 1) {
    fprintf(stderr, "Error: Invalid filter order %d\n", order);
    return 0;
  }

  zpk->num_zeros = order - order % 2;
  zpk->num_poles = order;
  zpk->gain = 1.0;

  const double epsilon_p = sqrt(exp((M_LN10 / 10) * passband_ripple_db) - 1.0);
  const double epsilon_s = sqrt(exp((M_LN10 / 10) * stopband_ripple_db) - 1.0);
  /* k1 and k control the balance between passband vs. stopband ripple size. */
  const double k1 = epsilon_p / epsilon_s;
  const double k = SolveEllipticDegreeEquation(order, k1);
  const double elliptic_k = EllipticK(k * k);
  const double elliptic_k1 = EllipticK(k1 * k1);

  /* Solve `sn(i v0 order K1, k1^2) = i / epsilon_p` for v0,
   * v0 = -i asne(i/epsilon_p, k1) / (order K1), by equation (65).
   */
  const double v0 =
      InverseJacobiSN(ComplexDoubleMake(0.0, 1.0 / epsilon_p), k1 * k1).imag /
      (order * elliptic_k1);

  ComplexDouble* zeros = zpk->zeros;
  ComplexDouble* poles = zpk->poles;

  if (order % 2 == 1) {
    /* Compute i sn(i v0 elliptic_k, k^2). */
    const double pole =
        -(JacobiSN(ComplexDoubleMake(0.0, v0 * elliptic_k), k * k).imag);
    *poles++ = ComplexDoubleMake(pole, 0.0);
  }

  int n;
  for (n = 0; n < order / 2; ++n) {
    const double u = (2 * n + 1) * elliptic_k / order;

    /* Compute zeta = cd(u K, k) by equation (44). */
    const double zeta = JacobiCD(ComplexDoubleMake(u, 0.0), k * k).real;
    /* Compute zero +/- i / (k zeta) by equation (62). */
    const ComplexDouble zero = ComplexDoubleMake(0.0, 1.0 / (k * zeta));
    *zeros++ = ComplexDoubleConj(zero); /* Make complex conjugate pair. */
    *zeros++ = zero;

    /* Compute pole i cd(u - i v0 elliptic_k, k^2) by equation (64). */
    const ComplexDouble pole = ComplexDoubleMul(
        ComplexDoubleMake(0.0, 1.0),
        JacobiCD(ComplexDoubleMake(u, -v0 * elliptic_k), k * k));
    *poles++ = ComplexDoubleConj(pole); /* Make complex conjugate pair. */
    *poles++ = pole;
  }

  zpk->gain = 1.0 / ComplexDoubleAbs(ZpkEval(zpk, ComplexDoubleMake(0.0, 0.0)));
  if (order % 2 == 0) {
    zpk->gain /= sqrt(1.0 + epsilon_p * epsilon_p);
  }

  return 1;
}

int DesignEllipticLowpass(int order,
                          double passband_ripple_db,
                          double stopband_ripple_db,
                          double cutoff_hz,
                          double sample_rate_hz,
                          BiquadFilterCoeffs* coeffs,
                          int max_biquads) {
  Zpk zpk;
  if (!EllipticAnalogPrototype(order, passband_ripple_db,
        stopband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignLowpass(&zpk, cutoff_hz, sample_rate_hz, coeffs, max_biquads);
}

int DesignEllipticHighpass(int order,
                           double passband_ripple_db,
                           double stopband_ripple_db,
                           double cutoff_hz,
                           double sample_rate_hz,
                           BiquadFilterCoeffs* coeffs,
                           int max_biquads) {
  Zpk zpk;
  if (!EllipticAnalogPrototype(order, passband_ripple_db,
        stopband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignHighpass(&zpk, cutoff_hz, sample_rate_hz,
                           coeffs, max_biquads);
}

int DesignEllipticBandpass(int order,
                           double passband_ripple_db,
                           double stopband_ripple_db,
                           double low_edge_hz,
                           double high_edge_hz,
                           double sample_rate_hz,
                           BiquadFilterCoeffs* coeffs,
                           int max_biquads) {
  Zpk zpk;
  if (!EllipticAnalogPrototype(order, passband_ripple_db,
        stopband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignBandpass(&zpk, low_edge_hz, high_edge_hz,
                           sample_rate_hz, coeffs, max_biquads);
}

int DesignEllipticBandstop(int order,
                           double passband_ripple_db,
                           double stopband_ripple_db,
                           double low_edge_hz,
                           double high_edge_hz,
                           double sample_rate_hz,
                           BiquadFilterCoeffs* coeffs,
                           int max_biquads) {
  Zpk zpk;
  if (!EllipticAnalogPrototype(order, passband_ripple_db,
        stopband_ripple_db, &zpk)) { return 0; }
  return ZpkDesignBandstop(&zpk, low_edge_hz, high_edge_hz,
                           sample_rate_hz, coeffs, max_biquads);
}
