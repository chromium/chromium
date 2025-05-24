#include "audio/dsp/portable/iir_design.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio/dsp/portable/logging.h"
#include "audio/dsp/portable/math_constants.h"

static double RandUnif() {
  return (double) rand() / RAND_MAX;
}

/* Shuffle an array of complex values using the Fisher-Yates algorithm. */
static void ShuffleComplexArray(ComplexDouble* array, int size) {
  int i;
  for (i = 0; i < size - 1; ++i) {
    /* Generate random j, i <= j < size. */
    const int j = i + rand() / (RAND_MAX / (size - i) + 1);
    ComplexDouble temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }
}

/* Tests whether two ComplexDoubles are within `tol` of each other. */
static int /*bool*/ ComplexNear(ComplexDouble actual, ComplexDouble expected,
                                double tol) {
  return ComplexDoubleAbs(ComplexDoubleSub(actual, expected)) <= tol;
}

/* Tests whether two ComplexDoubles are within ~10 ULPs of each other. */
static int /*bool*/ ComplexEqual(ComplexDouble actual, ComplexDouble expected) {
  const double tol = 10 * DBL_EPSILON * ComplexDoubleAbs(expected);
  return ComplexNear(actual, expected, tol);
}

/* Makes a random Zpk transfer function whose poles and zeros can be paired. */
static void MakeRandomZpk(Zpk* zpk) {
  int num_real_zeros;
  int num_real_poles;
  do { /* Loop until num_zeros <= num_poles. */
    num_real_zeros = 1 + rand() / (RAND_MAX / 4 + 1);
    const int num_paired_zeros = rand() / (RAND_MAX / 5 + 1);
    zpk->num_zeros = num_real_zeros + 2 * num_paired_zeros;
    num_real_poles = 1 + rand() / (RAND_MAX / 4 + 1);
    const int num_paired_poles = rand() / (RAND_MAX / 5 + 1);
    zpk->num_poles = num_real_poles + 2 * num_paired_poles;
  } while (zpk->num_zeros > zpk->num_poles);

  int n;
  for (n = 0; n < num_real_zeros; ++n) { /* Generate real zeros. */
    zpk->zeros[n] = ComplexDoubleMake(2 * RandUnif() - 1, 0);
  }
  for (; n < zpk->num_zeros; n += 2) { /* Generate complex pair zeros. */
    zpk->zeros[n] = ComplexDoubleMake(2 * RandUnif() - 1, 2 * RandUnif() - 1);
    zpk->zeros[n + 1] = ComplexDoubleConj(zpk->zeros[n]);
  }

  for (n = 0; n < num_real_poles; ++n) { /* Generate real poles. */
    zpk->poles[n] = ComplexDoubleMake(-1e-3 - RandUnif(), 0);
  }
  for (; n < zpk->num_poles; n += 2) { /* Generate complex pair poles. */
    zpk->poles[n] = ComplexDoubleMake(-1e-3 - RandUnif(), 2 * RandUnif() - 1);
    zpk->poles[n + 1] = ComplexDoubleConj(zpk->poles[n]);
  }

  zpk->gain = 2 * RandUnif() - 1;
}

/* Evaluates the quadratic ((c0 * z) + c1) * z + c2. */
static ComplexDouble QuadraticEval(double c0, double c1, double c2,
                                   ComplexDouble z) {
  ComplexDouble accum = ComplexDoubleMulReal(z, c0);
  accum.real += c1;
  accum = ComplexDoubleMul(accum, z);
  accum.real += c2;
  return accum;
}

/* Evaluates a transfer function expressed with biquads at complex `z`. */
static ComplexDouble BiquadsEval(const BiquadFilterCoeffs* coeffs,
                                 int num_biquads, ComplexDouble z) {
  ComplexDouble numerator = ComplexDoubleMake(1.0, 0.0);
  ComplexDouble denominator = ComplexDoubleMake(1.0, 0.0);
  int n;
  for (n = 0; n < num_biquads; ++n) {
    numerator = ComplexDoubleMul(
        numerator, QuadraticEval(coeffs[n].b0, coeffs[n].b1, coeffs[n].b2, z));
    denominator = ComplexDoubleMul(
        denominator, QuadraticEval(1.0, coeffs[n].a1, coeffs[n].a2, z));
  }
  return ComplexDoubleDiv(numerator, denominator);
}

static void PrintZpk(const char* name, const Zpk* zpk) {
  fprintf(stderr, "%s.zeros\n", name);
  int i;
  for (i = 0; i < zpk->num_zeros; ++i) {
    fprintf(stderr, "  %.15g + j%.15g\n",
        zpk->zeros[i].real, zpk->zeros[i].imag);
  }
  fprintf(stderr, "%s.poles\n", name);
  for (i = 0; i < zpk->num_poles; ++i) {
    fprintf(stderr, "  %.15g + j%.15g\n",
        zpk->poles[i].real, zpk->poles[i].imag);
  }
  fprintf(stderr, "%s.gain = %.15g\n", name, zpk->gain);
}

static void PrintBiquads(const char* name,
                         const BiquadFilterCoeffs* coeffs,
                         int num_biquads) {
  int i;
  for (i = 0; i < num_biquads; ++i) {
    fprintf(stderr, " %s[%d]: "
        "b0=%.7f b1=%.7f b2=%.7f a1=%.7f a2=%.7f\n",
        name, i, coeffs[i].b0, coeffs[i].b1, coeffs[i].b2,
        coeffs[i].a1, coeffs[i].a2);
  }
}

void QuadraticRoots(double c0, double c1, double c2,
    ComplexDouble* root1, ComplexDouble* root2) {
  c1 /= (2 * c0);
  c2 /= c0;
  const double d = c1 * c1 - c2;
  const ComplexDouble sqrt_d = ComplexDoubleSqrt(ComplexDoubleMake(d, 0));
  *root1 = ComplexDoubleSub(ComplexDoubleMake(-c1, 0.0), sqrt_d);
  *root2 = ComplexDoubleAdd(ComplexDoubleMake(-c1, 0.0), sqrt_d);
}

void BiquadsToZpk(const BiquadFilterCoeffs* coeffs,
                  int num_biquads,
                  Zpk* zpk) {
  const int degree = 2 * num_biquads;
  zpk->num_zeros = degree;
  zpk->num_poles = degree;
  zpk->gain = 1.0;

  int i;
  for (i = 0; i < num_biquads; ++i) {
    QuadraticRoots(coeffs[i].b0, coeffs[i].b1, coeffs[i].b2,
        &zpk->zeros[2 * i], &zpk->zeros[2 * i + 1]);
    QuadraticRoots(1.0, coeffs[i].a1, coeffs[i].a2,
        &zpk->poles[2 * i], &zpk->poles[2 * i + 1]);
    zpk->gain *= coeffs[i].b0;
  }
}

static int BiquadsAreApproximatelyEqual(
    const BiquadFilterCoeffs* actual,
    const BiquadFilterCoeffs* expected,
    int num_biquads) {
  Zpk actual_zpk;
  BiquadsToZpk(actual, num_biquads, &actual_zpk);
  Zpk expected_zpk;
  BiquadsToZpk(expected, num_biquads, &expected_zpk);

  if (!ZpkSort(&actual_zpk) || !ZpkSort(&expected_zpk)) {
    fprintf(stderr, "ZpkSort failed:\n");
    PrintZpk("actual", &actual_zpk);
    PrintBiquads("actual", actual, num_biquads);
    PrintZpk("expected", &expected_zpk);
    PrintBiquads("expected", expected, num_biquads);
    return 0;
  }

  const double tol = 5e-7f;
  const int degree = 2 * num_biquads;

  int i;
  for (i = 0; i < degree; ++i) {
    if (!(ComplexDoubleAbs(ComplexDoubleSub(
              actual_zpk.zeros[i], expected_zpk.zeros[i])) <= tol &&
          ComplexDoubleAbs(ComplexDoubleSub(
              actual_zpk.poles[i], expected_zpk.poles[i])) <= tol)) {
      goto fail;
    }
  }

  if (!(fabs(actual_zpk.gain - expected_zpk.gain) <= tol)) { goto fail; }

  return 1;

fail:
  fprintf(stderr, "Error: Biquads differ.\n");
  PrintZpk("actual", &actual_zpk);
  PrintBiquads("actual", actual, num_biquads);
  PrintZpk("expected", &expected_zpk);
  PrintBiquads("expected", expected, num_biquads);
  return 0;
}

void TestZpkEval() {
  puts("TestZpkEval");
  Zpk zpk;

  /*               s
   * H(s) = 1.2 -------.
   *            s + 3.0
   */
  zpk.num_zeros = 1;
  zpk.zeros[0] = ComplexDoubleMake(0.0, 0.0);
  zpk.num_poles = 1;
  zpk.poles[0] = ComplexDoubleMake(-3.0, 0.0);
  zpk.gain = 1.2;

  int trial;
  for (trial = 0; trial < 5; ++trial) {
    ComplexDouble s = ComplexDoubleMake(RandUnif() - 0.5, RandUnif() - 0.5);
    ComplexDouble expected = ComplexDoubleMulReal(ComplexDoubleDiv(
          s, ComplexDoubleSub(s, zpk.poles[0])), zpk.gain);

    ComplexDouble result = ZpkEval(&zpk, s);

    ABSL_CHECK(ComplexEqual(result, expected));
  }
}

/* Test ZpkSort(), utility that sorts and pairs complex roots. */
void TestZpkSort() {
  puts("TestZpkSort");
  Zpk zpk;
  zpk.num_zeros = 8;
  static const ComplexDouble zeros[] = {
    {-1.0, -0.3}, {-1.0, 0.2}, {2.0, 0.0}, {-1.0, -0.2},
    {-1.0, 0.3}, {-1.0, 0.0}, {-1.0, 0.2}, {-1.0, -0.2}};
  memcpy(zpk.zeros, zeros, sizeof(zeros));
  zpk.num_poles = 6;
  static const ComplexDouble poles[] = {
    {-1.0, 1.0}, {-3.0, 1.0}, {-3.0, -1.0},
    {-2.0, 1.0}, {-1.0, -1.0}, {-2.0, -1.0}};
  memcpy(zpk.poles, poles, sizeof(poles));

  ABSL_CHECK(ZpkSort(&zpk));
  ABSL_CHECK(zpk.num_zeros == 8);
  ABSL_CHECK(zpk.num_poles == 6);

  /* Zeros: s = -1, 2, -1 +/- 0.2i (double), -1 +/- 0.3i. */
  static const ComplexDouble expected_zeros[] = {
    {-1.0, 0.0}, {2.0, 0.0}, {-1.0, -0.2}, {-1.0, 0.2},
    {-1.0, -0.2}, {-1.0, 0.2}, {-1.0, -0.3}, {-1.0, 0.3}};
  int n;
  for (n = 0; n < 8; ++n) {
    ABSL_CHECK(ComplexEqual(zpk.zeros[n], expected_zeros[n]));
  }
  /* Poles: s = -3 +/- i, -2 +/- i, -1 +/- i. */
  static const ComplexDouble expected_poles[] = {
    {-3.0, -1.0}, {-3.0, 1.0}, {-2.0, -1.0},
    {-2.0, 1.0}, {-1.0, -1.0}, {-1.0, 1.0}};
  for (n = 0; n < 6; ++n) {
    ABSL_CHECK(ComplexEqual(zpk.poles[n], expected_poles[n]));
  }

  int trial;
  for (trial = 0; trial < 20; ++trial) {
    const int num_real = (int)(6 * RandUnif());
    const int num_pairs = (int)(11 * RandUnif());
    const int num_total = num_real + 2 * num_pairs;
    ComplexDouble* expected = (ComplexDouble*)ABSL_CHECK_NOTNULL(
        malloc(num_total * sizeof(ComplexDouble)));

    /* Generate expected zeros in sorted order, with the kth real root at
     *   s = k,
     * and the kth complex-conjugate pair at
     *   s = (k/3) +/- (1 + k%3)i.
     * Each root has random multiplicity of 1, 2, or 3.
     */
    int k = 0;
    for (n = 0; n < num_real;) {
      const ComplexDouble root = ComplexDoubleMake(k, 0.0);
      int multiplicity = (int)(1.5 + 2 * RandUnif());  /* = 1, 2, or 3. */
      for (; n < num_real && multiplicity > 0; ++n, multiplicity--) {
        expected[n] = root;
      }
      ++k;
    }

    k = 0;
    while (n < num_total) {
      const ComplexDouble root = ComplexDoubleMake(k / 3, 1 + k % 3);
      int multiplicity = (int)(1.5 + 2 * RandUnif());  /* = 1, 2, or 3. */
      for (; n < num_total && multiplicity > 0; n += 2, multiplicity--) {
        expected[n] = ComplexDoubleConj(root);
        expected[n + 1] = root;
      }
      ++k;
    }

    /* Make `zpk.zeros` a shuffled version of `expected`. */
    zpk.num_zeros = num_total;
    memcpy(zpk.zeros, expected, num_total * sizeof(ComplexDouble));
    ShuffleComplexArray(zpk.zeros, num_total);

    ABSL_CHECK(ZpkSort(&zpk));  /* Put zpk.zeros back in sorted order. */

    for (n = 0; n < num_total; ++n) {
      ABSL_CHECK(ComplexEqual(zpk.zeros[n], expected[n]));
    }

    free(expected);
  }
}

void TestZpkBilinearTransform() {
  puts("TestZpkBilinearTransform");
  int trial;
  for (trial = 0; trial < 10; ++trial) {
    Zpk analog;
    MakeRandomZpk(&analog); /* Make a random analog transfer function. */

    /* Discretize it with the bilinear transform. */
    const double sample_rate_hz = 0.4;
    Zpk discretized;
    memcpy(&discretized, &analog, sizeof(Zpk));
    ABSL_CHECK(ZpkBilinearTransform(&discretized, sample_rate_hz));

    /* Check that transfer functions agree at a few points. */
    const double K = 2.0 * sample_rate_hz;
    int n;
    for (n = 0; n < 10; ++n) {
      const ComplexDouble z = ComplexDoubleMake(2.0 * RandUnif() - 1.0,
                                                2.0 * RandUnif() - 1.0);
      /* Compute s = K (z - 1) / (z + 1). */
      const ComplexDouble s = ComplexDoubleMulReal(ComplexDoubleDiv(
          ComplexDoubleMake(z.real - 1.0, z.imag),
          ComplexDoubleMake(z.real + 1.0, z.imag)), K);
      ABSL_CHECK(ComplexNear(ZpkEval(&discretized, z), ZpkEval(&analog, s), 1e-9));
    }
  }
}

void TestZpkAnalogPrototypeToLowpass() {
  puts("TestZpkAnalogPrototypeToLowpass");
  int trial;
  for (trial = 0; trial < 10; ++trial) {
    Zpk zpk;
    MakeRandomZpk(&zpk);

    const double cutoff_rad_s = 0.5 + RandUnif();
    Zpk map_zpk;
    memcpy(&map_zpk, &zpk, sizeof(Zpk));
    ZpkAnalogPrototypeToLowpass(&map_zpk, cutoff_rad_s);

    /* Check that transfer functions agree at a few points with the mapping
     * s -> s / cutoff_rad_s.
     */
    int n;
    for (n = 0; n < 10; ++n) {
      const ComplexDouble map_s =
          ComplexDoubleMake(RandUnif(), 2 * RandUnif() - 1);
      const ComplexDouble s =ComplexDoubleMulReal(map_s, 1 / cutoff_rad_s);
      ABSL_CHECK(ComplexNear(ZpkEval(&zpk, s), ZpkEval(&map_zpk, map_s), 1e-10));
    }
  }
}

void TestZpkAnalogPrototypeToHighpass() {
  puts("TestZpkAnalogPrototypeToHighpass");
  int trial;
  for (trial = 0; trial < 10; ++trial) {
    Zpk zpk;
    MakeRandomZpk(&zpk);

    const double cutoff_rad_s = 0.5 + RandUnif();
    Zpk map_zpk;
    memcpy(&map_zpk, &zpk, sizeof(Zpk));
    ZpkAnalogPrototypeToHighpass(&map_zpk, cutoff_rad_s);

    /* Check that transfer functions agree at a few points with the mapping
     * s -> cutoff_rad_s / s.
     */
    int n;
    for (n = 0; n < 10; ++n) {
      const ComplexDouble map_s =
          ComplexDoubleMake(RandUnif(), 1e-3 + RandUnif());
      const ComplexDouble s =
          ComplexDoubleDiv(ComplexDoubleMake(cutoff_rad_s, 0), map_s);
      ABSL_CHECK(ComplexNear(ZpkEval(&zpk, s), ZpkEval(&map_zpk, map_s), 1e-10));
    }
  }
}

void TestZpkAnalogPrototypeToBandpass() {
  puts("TestZpkAnalogPrototypeToBandpass");
  int trial;
  for (trial = 0; trial < 10; ++trial) {
    Zpk zpk;
    MakeRandomZpk(&zpk);

    const double low_edge_rad_s = 0.5 + RandUnif();
    const double high_edge_rad_s = low_edge_rad_s + RandUnif();
    Zpk map_zpk;
    memcpy(&map_zpk, &zpk, sizeof(Zpk));
    ZpkAnalogPrototypeToBandpass(&map_zpk, low_edge_rad_s, high_edge_rad_s);

    /* Check that transfer functions agree at a few points with the mapping
     * (s^2 + center_rad_s^2) / (s * bandwidth_rad_s) -> s.
     */
    const double center_rad_s = sqrt(low_edge_rad_s * high_edge_rad_s);
    const double bandwidth_rad_s = high_edge_rad_s - low_edge_rad_s;
    int n;
    for (n = 0; n < 10; ++n) {
      const ComplexDouble map_s =
          ComplexDoubleMake(RandUnif(), 1e-3 + RandUnif());
      const ComplexDouble s = ComplexDoubleDiv(
          ComplexDoubleAdd(ComplexDoubleSquare(map_s),
                           ComplexDoubleMake(center_rad_s * center_rad_s, 0)),
          ComplexDoubleMulReal(map_s, bandwidth_rad_s));
      ABSL_CHECK(ComplexNear(ZpkEval(&zpk, s), ZpkEval(&map_zpk, map_s), 1e-10));
    }
  }
}

void TestZpkAnalogPrototypeToBandstop() {
  puts("TestZpkAnalogPrototypeToBandstop");
  int trial;
  for (trial = 0; trial < 10; ++trial) {
    Zpk zpk;
    MakeRandomZpk(&zpk);

    const double low_edge_rad_s = 0.5 + RandUnif();
    const double high_edge_rad_s = low_edge_rad_s + RandUnif();
    Zpk map_zpk;
    memcpy(&map_zpk, &zpk, sizeof(Zpk));
    ZpkAnalogPrototypeToBandstop(&map_zpk, low_edge_rad_s, high_edge_rad_s);

    /* Check that transfer functions agree at a few points with the mapping
     * (s * bandwidth_rad_s) / (s^2 + center_rad_s^2) -> s.
     */
    const double center_rad_s = sqrt(low_edge_rad_s * high_edge_rad_s);
    const double bandwidth_rad_s = high_edge_rad_s - low_edge_rad_s;
    int n;
    for (n = 0; n < 10; ++n) {
      const ComplexDouble map_s =
          ComplexDoubleMake(RandUnif(), 1e-3 + RandUnif());
      const ComplexDouble s = ComplexDoubleDiv(
          ComplexDoubleMulReal(map_s, bandwidth_rad_s),
          ComplexDoubleAdd(ComplexDoubleSquare(map_s),
                           ComplexDoubleMake(center_rad_s * center_rad_s, 0)));
      ABSL_CHECK(ComplexNear(ZpkEval(&zpk, s), ZpkEval(&map_zpk, map_s), 1e-10));
    }
  }
}

void TestZpkToBiquads() {
  puts("TestZpkToBiquads");
  int trial;
  for (trial = 0; trial < 10; ++trial) {
    Zpk zpk;
    MakeRandomZpk(&zpk);
    ABSL_CHECK(ZpkBilinearTransform(&zpk, 1.0));

    const int num_biquads = (zpk.num_poles + 1) / 2;
    BiquadFilterCoeffs* coeffs = (BiquadFilterCoeffs*)ABSL_CHECK_NOTNULL(
        malloc(num_biquads * sizeof(BiquadFilterCoeffs)));

    ABSL_CHECK(ZpkToBiquads(&zpk, coeffs, num_biquads) == num_biquads);

    int n;
    for (n = 0; n < 10; ++n) {
      ComplexDouble z;
      do {
        z = ComplexDoubleMake(4 * RandUnif() - 2, 4 * RandUnif() - 2);
      } while (ComplexDoubleAbs2(z) <= 1.0);

      ABSL_CHECK(ComplexNear(ZpkEval(&zpk, z),
                        BiquadsEval(coeffs, num_biquads, z), 2e-6));
    }

    free(coeffs);
  }
}

/* Compare with lowpass filters designed by scipy.signal.butter. */
void TestButterworthLowpassCompareWithScipy() {
  puts("TestButterworthLowpassCompareWithScipy");
  BiquadFilterCoeffs coeffs[4];

  ABSL_CHECK(DesignButterworthLowpass(2, 2800.0, 8000.0, coeffs, 1) == 1);
  /* Compare with scipy.signal.butter(2, 2800 / 4000.0). */
  static const BiquadFilterCoeffs kExpected1[] =  {
    {0.50500103f, 1.01000206f, 0.50500103f, 0.74778918f, 0.27221494f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected1, 1));

  ABSL_CHECK(DesignButterworthLowpass(2, 6700.0, 44100.0, coeffs, 1) == 1);
  /* Compare with scipy.signal.butter(2, 6700 / 22050.0). */
  static const BiquadFilterCoeffs kExpected2[] =  {
    {0.13381137f, 0.26762273f, 0.13381137f, -0.73294293f, 0.26818839f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected2, 1));

  ABSL_CHECK(DesignButterworthLowpass(3, 2800.0, 8000.0, coeffs, 2) == 2);
  /* Compare with scipy.signal.butter(3, 2800 / 4000.0, output='sos'). */
  static const BiquadFilterCoeffs kExpected3[] =  {
    {1.0f, 2.0f, 1.0f, 0.83699779f, 0.42398569f},
    {0.37445269f, 0.37445269f, 0.0f, 0.32491970f, 0.0f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected3, 2));

  ABSL_CHECK(DesignButterworthLowpass(3, 6700.0, 44100.0, coeffs, 2) == 2);
  /* Compare with scipy.signal.butter(3, 6700 / 22050.0, output='sos'). */
  static const BiquadFilterCoeffs kExpected4[] =  {
    {1.0f, 2.0f, 1.0f, -0.82092225f, 0.42041629f},
    {0.05108901f, 0.05108901f, 0.0f, -0.31823827f, 0.0f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected4, 2));

  ABSL_CHECK(DesignButterworthLowpass(8, 2800.0, 8000.0, coeffs, 4) == 4);
  /* Compare with scipy.signal.butter(8, 2800 / 4000.0, output='sos'). */
  static const BiquadFilterCoeffs kExpected5[] =  {
    {0.078902024f, 0.157804048f, 0.078902024f, 0.655471918f, 0.115155433f},
    {1.0f, 2.0f, 1.0f, 0.702809498f, 0.195690936f},
    {1.0f, 2.0f, 1.0f, 0.811037099f, 0.379818727f},
    {1.0f, 2.0f, 1.0f, 1.015320986f, 0.727367235f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected5, 4));

  ABSL_CHECK(DesignButterworthLowpass(8, 6700.0, 44100.0, coeffs, 4) == 4);
  /* Compare with scipy.signal.butter(8, 6700 / 22050.0, output='sos'). */
  static const BiquadFilterCoeffs kExpected6[] =  {
    {0.000389505713767f, 0.000779011427533f, 0.000389505713767f,
     -0.642019975362694f, 0.11086723319112f},
    {1.0f, 2.0f, 1.0f, -0.688627084050815f, 0.191510066533124f},
    {1.0f, 2.0f, 1.0f, -0.795307312923094f, 0.376095554884317f},
    {1.0f, 2.0f, 1.0f, -0.99713698106804f, 0.725315164291949f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected6, 4));
}

/* Compare with highpass filters designed by scipy.signal.butter. */
void TestButterworthHighpassCompareWithScipy() {
  puts("TestButterworthHighpassCompareWithScipy");
  BiquadFilterCoeffs coeffs[4];

  ABSL_CHECK(DesignButterworthHighpass(2, 2800.0, 8000.0, coeffs, 1) == 1);
  /* Compare with scipy.signal.butter(2, 2800 / 4000.0, btype='high'). */
  static const BiquadFilterCoeffs kExpected1[] =  {
    {0.13110644f, -0.26221288f, 0.13110644f, 0.74778918f, 0.27221494f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected1, 1));

  ABSL_CHECK(DesignButterworthHighpass(3, 2800.0, 8000.0, coeffs, 2) == 2);
  /* Compare with scipy.signal.butter. */
  static const BiquadFilterCoeffs kExpected2[] =  {
    {0.049532996357253f, -0.099065992714506f, 0.049532996357253f,
     0.324919696232906f, 0.0f},
    {1.0f, -1.0f, 0.0f, 0.836997787438826f, 0.423985688947412f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected2, 2));

  ABSL_CHECK(DesignButterworthHighpass(8, 2800.0, 8000.0, coeffs, 4) == 4);
  /* Compare with scipy.signal.butter. */
  static const BiquadFilterCoeffs kExpected3[] =  {
    {0.00035844f, -0.00071688f, 0.00035844f, 0.65547192f, 0.11515543f},
    {1.0f, -2.0f, 1.0f, 0.7028095f, 0.19569094f},
    {1.0f, -2.0f, 1.0f, 0.8110371f, 0.37981873f},
    {1.0f, -2.0f, 1.0f, 1.01532099f, 0.72736724f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected3, 4));
}

/* Compare with bandpass filters designed by scipy.signal.butter. */
void TestButterworthBandpassCompareWithScipy() {
  puts("TestButterworthBandpassCompareWithScipy");
  BiquadFilterCoeffs coeffs[8];

  ABSL_CHECK(DesignButterworthBandpass(1, 400.0, 2000.0, 8000.0, coeffs, 1) == 1);
  /* Compare with butter(1, [400./4000, 2000./4000], btype='band'). */
  static const BiquadFilterCoeffs kExpected1[] =  {
    {0.42080778f, 0.0f, -0.42080778f, -0.84161556f, 0.15838444f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected1, 1));

  ABSL_CHECK(DesignButterworthBandpass(2, 1000.0, 2800.0, 8000.0, coeffs, 2) == 2);
  /* Compare with butter(2, [1000./4000, 2800./4000], btype='band'). */
  static const BiquadFilterCoeffs kExpected2[] =  {
    {0.24834108f, -0.49668216f, 0.24834108f, -0.92397316f, 0.44643105},
    {1.0f, 2.0f, 1.0f, 0.6986048f, 0.39777278f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected2, 2));

  ABSL_CHECK(DesignButterworthBandpass(3, 1000.0, 2800.0, 8000.0, coeffs, 3) == 3);
  /* Compare with butter(3, [1000./4000, 2800./4000], btype='band'). */
  static const BiquadFilterCoeffs kExpected3[] =  {
    {0.13006301f, -0.26012602f, 0.13006301f, -1.08969889f, 0.60361443f},
    {1.0f, 2.0f, 1.0f, 0.86172781f, 0.5613314f},
    {1.0f, 0.0f, -1.0f, -0.11130102f, 0.07870171f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected3, 3));

  ABSL_CHECK(DesignButterworthBandpass(8, 1000.0, 2800.0, 8000.0, coeffs, 8) == 8);
  /* Compare with butter(8, [1000./4000, 2800./4000], btype='band'). */
  static const BiquadFilterCoeffs kExpected4[] =  {
    {0.00477865f, -0.0095573f, 0.00477865f, -0.41903671f, 0.14077695f},
    {1.0f, -2.0f, 1.0f, 0.1962781f, 0.11289888f},
    {1.0f, -2.0f, 1.0f, -0.78708697f, 0.33824169f},
    {1.0f, -2.0f, 1.0f, 0.56300055f, 0.29019847f},
    {1.0f, 2.0f, 1.0f, -1.04928756f, 0.56258171f},
    {1.0f, 2.0f, 1.0f, 0.82208233f, 0.5179345f},
    {1.0f, 2.0f, 1.0f, -1.28985277f, 0.83358982f},
    {1.0f, 2.0f, 1.0f, 1.05642513f, 0.81201072f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected4, 8));
}

/* Compare with bandstop filters designed by scipy.signal.butter. */
void TestButterworthBandstopCompareWithScipy() {
  puts("TestButterworthBandstopCompareWithScipy");
  BiquadFilterCoeffs coeffs[8];

  ABSL_CHECK(DesignButterworthBandstop(2, 1000.0, 2800.0, 8000.0, coeffs, 2) == 2);
  /* Compare with butter(2, [1000./4000, 2800./4000], btype='bandstop'). */
  static const BiquadFilterCoeffs kExpected1[] =  {
    {0.34044798f, -0.07025521f, 0.34044798f, -0.92397316f, 0.44643105f},
    {1.0f, -0.20636107f, 1.0f, 0.6986048f, 0.39777278f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected1, 2));

  ABSL_CHECK(DesignButterworthBandstop(3, 1000.0, 2800.0, 8000.0, coeffs, 3) == 3);
  /* Compare with butter(3, [1000./4000, 2800./4000], btype='bandstop'). */
  static const BiquadFilterCoeffs kExpected2[] =  {
    {0.20876472f, -0.04308091f, 0.20876472f, -1.08969889f, 0.60361443f},
    {1.0f, -0.20636107f, 1.0f, 0.86172781f, 0.5613314f},
    {1.0f, -0.20636107f, 1.0f, -0.11130102f, 0.07870171f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected2, 3));
}

/* Compare with lowpass filters designed by scipy.signal.cheby1. */
void TestChebyshev1LowpassCompareWithScipy() {
  puts("TestChebyshev1LowpassCompareWithScipy");
  BiquadFilterCoeffs coeffs[4];

  ABSL_CHECK(DesignChebyshev1Lowpass(2, 0.25, 2800.0, 8000.0, coeffs, 1) == 1);
  /* Compare with scipy.signal.cheby1(2, 0.25, 2800 / 4000.0). */
  static const BiquadFilterCoeffs kExpected1[] =  {
    {0.6245025f, 1.249005f, 0.6245025f, 1.12761236f, 0.44334085f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected1, 1));

  ABSL_CHECK(DesignChebyshev1Lowpass(3, 0.25, 2800.0, 8000.0, coeffs, 2) == 2);
  /* Compare with scipy.signal.cheby1(3, 0.25, 2800 / 4000.0). */
  static const BiquadFilterCoeffs kExpected2[] =  {
    {0.40439508f, 0.80879017f, 0.40439508f, 1.08489114f, 0.60695144f},
    {1.0f, 1.0f, 0.0f, 0.20183873f, 0.0f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected2, 2));

  ABSL_CHECK(DesignChebyshev1Lowpass(8, 0.25, 2800.0, 8000.0, coeffs, 4) == 4);
  /* Compare with scipy.signal.cheby1(8, 0.25, 2800 / 4000.0). */
  static const BiquadFilterCoeffs kExpected3[] =  {
    {0.036675023172098f, 0.073350046344197f, 0.036675023172098f,
     -0.469314768116427f, 0.159262574580242f},
    {1.0f, 2.0f, 1.0f, 0.278908537926303f, 0.476066730951042f},
    {1.0f, 2.0f, 1.0f, 0.857482902248379f, 0.741492106793509f},
    {1.0f, 2.0f, 1.0f, 1.149761850742742f, 0.92082241188338f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected3, 4));
}

/* Compare with lowpass filters designed by scipy.signal.cheby2. */
void TestChebyshev2LowpassCompareWithScipy() {
  puts("TestChebyshev2LowpassCompareWithScipy");
  BiquadFilterCoeffs coeffs[4];

  ABSL_CHECK(DesignChebyshev2Lowpass(2, 40.0, 2800.0, 8000.0, coeffs, 1) == 1);
  /* Compare with scipy.signal.cheby2(2, 40.0, 2800 / 4000.0). */
  static const BiquadFilterCoeffs kExpected1[] =  {
    {0.05930588f, 0.09135622f, 0.05930588f, -1.25779318f, 0.46776116f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected1, 1));

  ABSL_CHECK(DesignChebyshev2Lowpass(3, 40.0, 2800.0, 8000.0, coeffs, 2) == 2);
  /* Compare with scipy.signal.cheby2(3, 40.0, 2800 / 4000.0). */
  static const BiquadFilterCoeffs kExpected2[] =  {
    {0.10319149f, 0.13911109f, 0.10319149f, -0.54363979f, 0.38881671f},
    {1.0f, 1.0f, 0.0f, -0.18243373f, 0.0f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected2, 2));

  ABSL_CHECK(DesignChebyshev2Lowpass(8, 40.0, 2800.0, 8000.0, coeffs, 4) == 4);
  /* Compare with scipy.signal.cheby2(8, 40.0, 2800 / 4000.0). */
  static const BiquadFilterCoeffs kExpected3[] =  {
    {0.18321788f, 0.21998572f, 0.18321788f, 0.81264743f, 0.8141466f},
    {1.0f, 1.39131493f, 1.0f, 0.80396075f, 0.52978068f},
    {1.0f, 1.70324873f, 1.0f, 0.86881113f, 0.33534356f},
    {1.0f, 1.96086251f, 1.0f, 0.92680045f, 0.2320861f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected3, 4));
}

/* Compare with lowpass filters designed by Mathematica's EllipticFilterModel.
 *
 * NOTE: We don't compare with scipy.signal.ellip. Its results are accurate to
 * only ~4 digits because it does some steps as numeric root finding with a
 * hard-coded stopping tolerance of 1e-4. Mathematica's implementation on the
 * other hand can execute with arbitrarily high precision.
 */
void TestEllipticLowpassCompareWithMathematica() {
  puts("TestEllipticLowpassCompareWithMathematica");
  BiquadFilterCoeffs coeffs[4];

  ABSL_CHECK(DesignEllipticLowpass(3, 0.5, 40.0, 2800.0, 8000.0, coeffs, 2) == 2);
  /* Compare with Mathematica:
   *   fc = 2*8000*Tan[Pi*2800/8000];
   *   tfa = N[EllipticFilterModel[{"Lowpass", {fc/8000, (280/100)*
   *           fc/8000}, {1/2, 40}}], 24];
   *   tf = ToDiscreteTimeModel[tfa, 1, z, Method -> {"BilinearTransform"}]
   */
  static const BiquadFilterCoeffs kExpected1[] = {
    {0.390135333f, 0.739301033f, 0.390135333f, 1.042023853f, 0.652283958f},
    {1.0f, 1.0f, 0.0f, 0.127986707f, 0.0f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected1, 2));

  ABSL_CHECK(DesignEllipticLowpass(4, 0.5, 40.0, 1200.0, 8000.0, coeffs, 2) == 2);
  /* Compare with Mathematica:
   *   fc = 2*8000*Tan[Pi*1200/8000];
   *   tfa = N[EllipticFilterModel[{"Lowpass", {fc/8000, (258/100)*
   *           fc/8000}, {1/2, 40}}], 24];
   *   tf = ToDiscreteTimeModel[tfa, 1, z, Method -> {"BilinearTransform"}]
   */
  static const BiquadFilterCoeffs kExpected2[] = {
    {0.038870928f, -0.009549698f, 0.038870928f, -1.024610377f, 0.804046421f},
    {1.0f, 1.178803959f, 1.0f, -1.119799060f, 0.414388849f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected2, 2));

  ABSL_CHECK(DesignEllipticLowpass(8, 0.5, 40.0, 2800.0, 8000.0, coeffs, 4) == 4);
  /* Compare with Mathematica:
   *   fc = 2*8000*Tan[Pi*2800/8000];
   *   tfa = N[EllipticFilterModel[{"Lowpass", {fc/8000, (103/100)*
   *           fc/8000}, {1/2, 40}}], 24];
   *   tf = ToDiscreteTimeModel[tfa, 1, z, Method -> {"BilinearTransform"}]
   */
  static const BiquadFilterCoeffs kExpected3[] = {
    {0.204958471f, 0.249675031f, 0.204958471f, 1.170311892f, 0.987234014f},
    {1.0f, 1.270427471f, 1.0f, 1.099868917f, 0.932952951f},
    {1.0f, 1.465723207f, 1.0f, 0.826009356f, 0.735026625f},
    {1.0f, 1.888901578f, 1.0f, 0.067003098f, 0.188704305f}};
  ABSL_CHECK(BiquadsAreApproximatelyEqual(coeffs, kExpected3, 4));
}

int main(int argc, char** argv) {
  srand(0);
  TestZpkEval();
  TestZpkSort();
  TestZpkBilinearTransform();
  TestZpkAnalogPrototypeToLowpass();
  TestZpkAnalogPrototypeToHighpass();
  TestZpkAnalogPrototypeToBandpass();
  TestZpkAnalogPrototypeToBandstop();
  TestZpkToBiquads();

  TestButterworthLowpassCompareWithScipy();
  TestButterworthHighpassCompareWithScipy();
  TestButterworthBandpassCompareWithScipy();
  TestButterworthBandstopCompareWithScipy();

  TestChebyshev1LowpassCompareWithScipy();
  TestChebyshev2LowpassCompareWithScipy();
  TestEllipticLowpassCompareWithMathematica();

  puts("PASS");
  return EXIT_SUCCESS;
}
