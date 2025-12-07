#include "audio/dsp/portable/butterworth.h"

#include <math.h>

#include "audio/dsp/portable/logging.h"
#include "audio/dsp/portable/math_constants.h"

static double RandUniform() { return (double) rand() / RAND_MAX; }

/* Computes frequency magnitude response. */
static double FrequencyResponse(
    const BiquadFilterCoeffs* coeffs, int num_stages,
    double frequency_hz, double sample_rate_hz) {
  const double theta = 2 * M_PI * frequency_hz / sample_rate_hz;
  const double cos_theta = cos(theta);
  const double cos_2theta = cos(2 * theta);
  double gain = 1.0;

  int i;
  for (i = 0; i < num_stages; ++i) {
    const double b0 = coeffs[i].b0;
    const double b1 = coeffs[i].b1;
    const double b2 = coeffs[i].b2;
    const double a1 = coeffs[i].a1;
    const double a2 = coeffs[i].a2;

    /* Compute gain *= | b0 + b1 exp(i theta) + b2 exp(i 2 theta) |^2. */
    gain *= b0 * b0 + b1 * b1 + b2 * b2
        + 2 * (b0 * b1 + b1 * b2) * cos_theta
        + 2 * b0 * b2 * cos_2theta;
    /* Compute gain /= | 1 + a1 exp(i theta) + a2 exp(i 2 theta) |^2. */
    gain /= 1.0 + a1 * a1 + a2 * a2
        + 2 * (a1 + a1 * a2) * cos_theta
        + 2 * a2 * cos_2theta;
  }
  return sqrt(gain);
}

static float BCoeffByIndex(const BiquadFilterCoeffs* coeffs, int i) {
  switch (i) {
    case 0: return coeffs->b0;
    case 1: return coeffs->b1;
    case 2: return coeffs->b2;
    default: return 0.0f;
  }
}

static float ACoeffByIndex(const BiquadFilterCoeffs* coeffs, int i) {
  switch (i) {
    case 0: return 1.0f;
    case 1: return coeffs->a1;
    case 2: return coeffs->a2;
    default: return 0.0f;
  }
}

/* Expands second-order sections to "ba" coefficient form. */
static void ExpandPolynomialForm(const BiquadFilterCoeffs* coeffs,
                                 float* b, float* a) {
  const int kNumBiquads = 2;
  const int kNumCoeffs = 1 + 2 * kNumBiquads;
  int n;
  for (n = 0; n < kNumCoeffs; ++n) {
    int m;
    b[n] = 0.0f;
    for (m = 0; m < 3; ++m) {  /* Convolve coeffs[0].b and coeffs[1].b. */
      b[n] += BCoeffByIndex(&coeffs[0], n - m) * BCoeffByIndex(&coeffs[1], m);
    }

    a[n] = 0.0f;
    for (m = 0; m < 3; ++m) {  /* Convolve coeffs[0].a and coeffs[1].a. */
      a[n] += ACoeffByIndex(&coeffs[0], n - m) * ACoeffByIndex(&coeffs[1], m);
    }
  }
}

/* Design lowpass filters with randomly chosen parameters and check the
 * frequency response.
 */
void TestLowpassResponse() {
  BiquadFilterCoeffs coeffs;
  int trial;

  for (trial = 0; trial < 20; ++trial) {
    double sample_rate_hz = 1000 + 49000 * RandUniform();
    double corner_frequency_hz = 0.05 + 0.4 * sample_rate_hz * RandUniform();

    ABSL_CHECK(DesignButterworthOrder2Lowpass(
      corner_frequency_hz, sample_rate_hz, &coeffs) == 1);

    /* Unit gain at DC. */
    double f = 0.0;
    double gain = FrequencyResponse(&coeffs, 1, f, sample_rate_hz);
    ABSL_CHECK(fabs(gain - 1.0) <= 1e-4);

    /* Gain is in [1/sqrt(2), 1] in the passband. */
    int j;
    for (j = 0; j < 5; ++j) {
      f = corner_frequency_hz * j / 4;
      gain = FrequencyResponse(&coeffs, 1, f, sample_rate_hz);
      ABSL_CHECK(1 / M_SQRT2 - 1e-4 <= gain && gain <= 1 + 1e-4);
    }

    /* Gain is below 1/sqrt(2) in the stopband. */
    for (j = 1; j < 4; ++j) {
      f = corner_frequency_hz +
          (sample_rate_hz / 2 - corner_frequency_hz) * j / 4;
      gain = FrequencyResponse(&coeffs, 1, f, sample_rate_hz);
      ABSL_CHECK(gain <= 1 / M_SQRT2 + 1e-4);
    }

    /* Zero gain at Nyquist. */
    f = sample_rate_hz / 2;
    gain = FrequencyResponse(&coeffs, 1, f, sample_rate_hz);
    ABSL_CHECK(fabs(gain) <= 1e-4);
  }
}

/* Compare with lowpass filters designed by scipy.signal.butter. */
void TestLowpassCompareWithScipy() {
  BiquadFilterCoeffs coeffs;
  ABSL_CHECK(DesignButterworthOrder2Lowpass(2800.0, 8000.0, &coeffs));

  /* Compare with scipy.signal.butter(2, 2800 / 4000.0). */
  ABSL_CHECK(fabs(coeffs.b0 - 0.50500103f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.b1 - 1.01000206f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.b2 - 0.50500103f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.a1 - 0.74778918f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.a2 - 0.27221494f) <= 5e-7f);

  ABSL_CHECK(DesignButterworthOrder2Lowpass(6700.0, 44100.0, &coeffs));

  /* Compare with scipy.signal.butter(2, 6700 / 22050.0). */
  ABSL_CHECK(fabs(coeffs.b0 - 0.13381137f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.b1 - 0.26762273f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.b2 - 0.13381137f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.a1 - -0.73294293f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.a2 - 0.26818839f) <= 5e-7f);
}

/* Check that invalid arguments are rejected. */
void TestLowpassInvalidArgs() {
  BiquadFilterCoeffs coeffs;
  ABSL_CHECK(!DesignButterworthOrder2Lowpass(0.0, 1000.0, &coeffs));
  ABSL_CHECK(!DesignButterworthOrder2Lowpass(900.0, 1000.0, &coeffs));
  ABSL_CHECK(!DesignButterworthOrder2Lowpass(100.0, 1000.0, NULL));
}

/* Design highpass filters with randomly choosen parameters and check the
 * frequency response.
 */
void TestHighpassResponse() {
  BiquadFilterCoeffs coeffs;
  int trial;

  for (trial = 0; trial < 20; ++trial) {
    double sample_rate_hz = 1000 + 49000 * RandUniform();
    double corner_frequency_hz = 0.05 + 0.4 * sample_rate_hz * RandUniform();

    ABSL_CHECK(DesignButterworthOrder2Highpass(
      corner_frequency_hz, sample_rate_hz, &coeffs) == 1);

    /* Zero gain at DC. */
    double f = 0.0;
    double gain = FrequencyResponse(&coeffs, 1, f, sample_rate_hz);
    ABSL_CHECK(fabs(gain) <= 1e-4);

    /* Gain is below 1/sqrt(2) in the stopband. */
    int j;
    for (j = 0; j < 4; ++j) {
      f = corner_frequency_hz * j / 4;
      gain = FrequencyResponse(&coeffs, 1, f, sample_rate_hz);
      ABSL_CHECK(gain <= 1 / M_SQRT2 + 1e-4);
    }

    /* Gain is in [1/sqrt(2), 1] in the passband. */
    for (j = 0; j < 4; ++j) {
      f = corner_frequency_hz +
          (sample_rate_hz / 2 - corner_frequency_hz) * j / 4;
      gain = FrequencyResponse(&coeffs, 1, f, sample_rate_hz);
      ABSL_CHECK(1 / M_SQRT2 - 1e-4 <= gain && gain <= 1 + 1e-4);
    }

    /* Unit gain at Nyquist. */
    f = sample_rate_hz / 2;
    gain = FrequencyResponse(&coeffs, 1, f, sample_rate_hz);
    ABSL_CHECK(fabs(gain - 1.0f) <= 1e-4);
  }
}

/* Compare with highpass filters designed by scipy.signal.butter. */
void TestHighpassCompareWithScipy() {
  BiquadFilterCoeffs coeffs;
  ABSL_CHECK(DesignButterworthOrder2Highpass(2800.0, 8000.0, &coeffs));

  /* Compare with scipy.signal.butter(2, 2800 / 4000.0, btype='high'). */
  ABSL_CHECK(fabs(coeffs.b0 - 0.13110644f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.b1 - -0.26221288f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.b2 - 0.13110644f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.a1 - 0.74778918f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.a2 - 0.27221494f) <= 5e-7f);

  ABSL_CHECK(DesignButterworthOrder2Highpass(6700.0, 44100.0, &coeffs));

  /* Compare with scipy.signal.butter(2, 6700 / 22050.0, btype='high'). */
  ABSL_CHECK(fabs(coeffs.b0 - 0.50028283f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.b1 - -1.00056566f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.b2 - 0.50028283f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.a1 - -0.73294293f) <= 5e-7f);
  ABSL_CHECK(fabs(coeffs.a2 - 0.26818839f) <= 5e-7f);
}

/* Check that invalid arguments are rejected. */
void TestHighpassInvalidArgs() {
  BiquadFilterCoeffs coeffs;
  ABSL_CHECK(!DesignButterworthOrder2Highpass(0.0, 1000.0, &coeffs));
  ABSL_CHECK(!DesignButterworthOrder2Highpass(900.0, 1000.0, &coeffs));
  ABSL_CHECK(!DesignButterworthOrder2Highpass(100.0, 1000.0, NULL));
}

/* Design bandpass filters with randomly choosen parameters and check the
 * frequency response.
 */
void TestBandpassResponse() {
  BiquadFilterCoeffs coeffs[2];
  int trial;

  for (trial = 0; trial < 20; ++trial) {
    double sample_rate_hz = 1000 + 49000 * RandUniform();
    double low_edge_hz = 0.05 + 0.4 * sample_rate_hz * RandUniform();
    double high_edge_hz = 0.05 + 0.4 * sample_rate_hz * RandUniform();

    if (low_edge_hz > high_edge_hz) {
      double temp = low_edge_hz;
      low_edge_hz = high_edge_hz;
      high_edge_hz = temp;
    }

    ABSL_CHECK(DesignButterworthOrder2Bandpass(
      low_edge_hz, high_edge_hz, sample_rate_hz, coeffs) == 1);

    /* Gain is 1.0 at the center frequency. */
    double f = sqrt(low_edge_hz * high_edge_hz);
    double gain = FrequencyResponse(coeffs, 2, f, sample_rate_hz);
    ABSL_CHECK(fabs(gain - 1.0) <= 5e-3);

    /* Zero gain at DC. */
    f = 0.0;
    gain = FrequencyResponse(coeffs, 2, f, sample_rate_hz);
    ABSL_CHECK(fabs(gain) <= 1e-4);

    /* Gain is below 1/sqrt(2) for 0 < f < low_edge_hz. */
    int j;
    for (j = 1; j < 4; ++j) {
      f = low_edge_hz * j / 4;
      gain = FrequencyResponse(coeffs, 2, f, sample_rate_hz);
      ABSL_CHECK(gain <= 1 / M_SQRT2 + 1e-4);
    }

    /* Gain is in [1/sqrt(2), 1] in the passband. */
    for (j = 0; j < 5; ++j) {
      f = low_edge_hz + (high_edge_hz - low_edge_hz) * j / 4;
      gain = FrequencyResponse(coeffs, 2, f, sample_rate_hz);
      ABSL_CHECK(1 / M_SQRT2 - 1e-4 <= gain && gain <= 1 + 1e-4);
    }

    /* Gain is below 1/sqrt(2) for high_edge_hz < f < Nyquist. */
    for (j = 1; j < 4; ++j) {
      f = high_edge_hz + (sample_rate_hz / 2 - high_edge_hz) * j / 4;
      gain = FrequencyResponse(coeffs, 2, f, sample_rate_hz);
      ABSL_CHECK(gain <= 1 / M_SQRT2 + 1e-4);
    }

    /* Zero gain at Nyquist. */
    f = sample_rate_hz / 2;
    gain = FrequencyResponse(coeffs, 2, f, sample_rate_hz);
    ABSL_CHECK(fabs(gain) <= 1e-4);
  }
}

/* Compare with bandpass filters designed by scipy.signal.butter. */
void TestBandpassCompareWithScipy() {
  BiquadFilterCoeffs coeffs[2];
  float b[5];
  float a[5];

  ABSL_CHECK(DesignButterworthOrder2Bandpass(1500.0, 2800.0, 8000.0, coeffs));

  /* Compare with
   * scipy.signal.butter(2, np.array([1500, 2800]) / 4000.0, btype='band')
   */
  {
    ExpandPolynomialForm(coeffs, b, a);
    float scipy_b[5] = {0.14894852, 0.0, -0.29789704, 0.0, 0.14894852};
    float scipy_a[5] = {1.0, 0.35725315, 0.6864134, 0.15457128, 0.24773253};
    int i;
    for (i = 0; i < 5; ++i) {
      ABSL_CHECK(fabs(b[i] - scipy_b[i]) <= 5e-7f);
      ABSL_CHECK(fabs(a[i] - scipy_a[i]) <= 5e-7f);
    }
  }

  ABSL_CHECK(DesignButterworthOrder2Bandpass(80.0, 6700.0, 44100.0, coeffs));

  /* Compare with
   * scipy.signal.butter(2, np.array([80, 6700]) / 22050.0, btype='band')
   */
  {
    ExpandPolynomialForm(coeffs, b, a);
    float scipy_b[5] = {0.13126508, 0.0, -0.26253016, 0.0, 0.13126508};
    float scipy_a[5] = {1.0, -2.73077003, 2.7421433, -1.28327902, 0.27197549};
    int i;
    for (i = 0; i < 5; ++i) {
      ABSL_CHECK(fabs(b[i] - scipy_b[i]) <= 5e-7f);
      ABSL_CHECK(fabs(a[i] - scipy_a[i]) <= 5e-7f);
    }
  }
}

/* Check that invalid arguments are rejected. */
void TestBandpassInvalidArgs() {
  BiquadFilterCoeffs coeffs[2];
  ABSL_CHECK(!DesignButterworthOrder2Bandpass(-1.0, 100.0, 1000.0, coeffs));
  ABSL_CHECK(!DesignButterworthOrder2Bandpass(400.0, 100.0, 1000.0, coeffs));
  ABSL_CHECK(!DesignButterworthOrder2Bandpass(20.0, 900.0, 1000.0, coeffs));
  ABSL_CHECK(!DesignButterworthOrder2Bandpass(20.0, 100.0, 1000.0, NULL));
}

int main(int argc, char** argv) {
  srand(0);
  TestLowpassResponse();
  TestLowpassCompareWithScipy();
  TestLowpassInvalidArgs();

  TestHighpassResponse();
  TestHighpassCompareWithScipy();
  TestHighpassInvalidArgs();

  TestBandpassResponse();
  TestBandpassCompareWithScipy();
  TestBandpassInvalidArgs();

  puts("PASS");
  return EXIT_SUCCESS;
}
