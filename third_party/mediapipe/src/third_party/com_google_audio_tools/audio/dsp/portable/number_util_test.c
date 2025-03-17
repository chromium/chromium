#include "audio/dsp/portable/number_util.h"

#include <limits.h>

#include "audio/dsp/portable/logging.h"
#include "audio/dsp/portable/math_constants.h"

#define kGoldenRatio 1.618033988749894903  /* = (1 + sqrt(5)) / 2. */

/* Check RoundUpToMultiple() function. */
void TestRoundUpToMultiple() {
  puts("TestRoundUpToMultiple");
  ABSL_CHECK(RoundUpToMultiple(33, 5) == 35);
  ABSL_CHECK(RoundUpToMultiple(-33, 5) == -30);

  static const int kFactors[] = {3, 4, 7, 10};
  int i;
  for (i = 0; i < sizeof(kFactors) / sizeof(*kFactors); ++i) {
    const int factor = kFactors[i];
    int multiple;
    for (multiple = -2; multiple <= 2; ++multiple) {
      int offset;
      for (offset = 0; offset < factor; ++offset) {
        const int value = factor * multiple - offset;
        ABSL_CHECK(RoundUpToMultiple(value, factor) == factor * multiple);
      }
    }
  }
}

/* Check GreatestCommonDivisor() function. */
void TestGreatestCommonDivisor() {
  puts("TestGreatestCommonDivisor");
  ABSL_CHECK(25 == GreatestCommonDivisor(200, 75));
  ABSL_CHECK(25 == GreatestCommonDivisor(75, 200));
  ABSL_CHECK(7 == GreatestCommonDivisor(7, 0));
  ABSL_CHECK(7 == GreatestCommonDivisor(0, 7));
  ABSL_CHECK(1 == GreatestCommonDivisor(64, 27));
  ABSL_CHECK(87 == GreatestCommonDivisor(13 * 87, 16 * 87));
  ABSL_CHECK(7 == GreatestCommonDivisor(7, 7));
  ABSL_CHECK(0 == GreatestCommonDivisor(0, 0));
}

/* RationalApproximation() represents ratios of small integers exactly. */
void TestRationalApproximationOfSimpleRatios() {
  puts("TestRationalApproximationOfSimpleRatios");
  int a;
  for (a = 1; a <= 12; ++a) {
    int b;
    for (b = 1; b <= 12; ++b) {
      int numerator;
      int denominator;
      RationalApproximation(((double) a) / b, 100, NULL,
                            &numerator, &denominator);

      const int gcd = GreatestCommonDivisor(a, b);
      ABSL_CHECK(numerator == a / gcd);
      ABSL_CHECK(denominator == b / gcd);
    }
  }

  /* Negative rationals should work too. */
  int numerator;
  int denominator;
  RationalApproximation(-11.0 / 7, 100, NULL, &numerator, &denominator);
  ABSL_CHECK(numerator == -11);
  ABSL_CHECK(denominator == 7);
}

/* Similar to the previous test, but with perturbation in the ratios. */
void TestRationalApproximationOfPerturbedRatios() {
  puts("TestRationalApproximationOfPerturbedRatios");

  static const int kTestRatios[][2] = {{1, 1}, {1, 3}, {1, 12}, {2, 3}, {7, 5},
                                       {11, 12}, {12, 1}, {12, 11}};
  int i;
  for (i = 0; i < sizeof(kTestRatios) / sizeof(*kTestRatios); ++i) {
    const int a = kTestRatios[i][0];
    const int b = kTestRatios[i][1];
    const double ratio = ((double) a) / b;

    static const double kPerturbations[] = {-1.1e-9, 1.1e-9, -1e-4, 1e-4};
    int j;
    for (j = 0; j < sizeof(kPerturbations) / sizeof(*kPerturbations); ++j) {
      int numerator;
      int denominator;
      /* RationalApproximation() recovers a/b even when ratio is perturbed. */
      RationalApproximation(ratio + kPerturbations[j], 100, NULL,
                            &numerator, &denominator);

      ABSL_CHECK(numerator == a);
      ABSL_CHECK(denominator == b);
    }
  }
}

/* Rational approximation of sqrt(2) is interesting because all but the first
 * term of its continued fraction representation are even,
 *   sqrt(2) = [1; 2, 2, 2, ...],
 * giving many chances to check the special-case behavior with
 * n = continued_fraction_term / 2 semiconvergents.
 */
void TestRationalApproximationOfSqrt2() {
  puts("TestRationalApproximationOfSqrt2");
  static const int kBestRationalApproximations[][2] = {
        {1, 1},   /* Convergent [1;].                      */
        {3, 2},   /* Convergent [1; 2].                    */
        {4, 3},   /* Semiconvergent between 1/1 and 3/2.   */
        {7, 5},   /* Convergent [1; 2, 2].                 */
        /* Skip the apparent semiconvergent 10/7 between 3/2 and 7/5, which is
         * not a best rational approximation since 7/5 is closer to sqrt(2).
         */
        {17, 12}, /* Convergent [1; 2, 2, 2].              */
        {24, 17}, /* Semiconvergent between 7/5 and 17/12. */
        {41, 29}, /* Convergent [1; 2, 2, 2, 2].           */
        /* Also skip the semiconvergent 65/46 between 24/17 and 41/29. */
        {99, 70}, /* Convergent [1; 2, 2, 2, 2, 2].        */
  };

  int i = 0;
  int numerator;
  int denominator;
  int max_denominator;

  for (max_denominator = 1; max_denominator <= 50; ++max_denominator) {
    while (kBestRationalApproximations[i + 1][1] <= max_denominator) {
      ++i;
    }
    RationalApproximation(M_SQRT2, max_denominator, NULL,
                          &numerator, &denominator);

    ABSL_CHECK(numerator == kBestRationalApproximations[i][0]);
    ABSL_CHECK(denominator == kBestRationalApproximations[i][1]);
  }
}

void TestRationalApproximationOfPi() {
  puts("TestRationalApproximationOfPi");
  static const int kBestRationalApproximations[][2] = {
        {3, 1},     /* Convergent [3;].           */
        /* Semiconvergents between "1/0" and 3/1. */
        {13, 4}, {16, 5}, {19, 6},
        {22, 7},    /* Convergent [3; 7].         */
        /* Semiconvergents between 19/6 and 22/7. */
        {179, 57}, {201, 64}, {223, 71}, {245, 78},
        {267, 85}, {289, 92}, {311, 99},
        {333, 106}  /* Convergent [3; 7, 15].     */
  };

  int i = 0;
  int numerator;
  int denominator;
  int max_denominator;

  for (max_denominator = 1; max_denominator <= 100; ++max_denominator) {
    while (kBestRationalApproximations[i + 1][1] <= max_denominator) {
      ++i;
    }
    RationalApproximation(M_PI, max_denominator, NULL,
                          &numerator, &denominator);

    ABSL_CHECK(numerator == kBestRationalApproximations[i][0]);
    ABSL_CHECK(denominator == kBestRationalApproximations[i][1]);
  }
}

/* The golden ratio (1 + sqrt(5))/2 = [1; 1, 1, 1, ...] is a particularly
 * difficult value to approximate by rationals [e.g. in the sense of Hurwitz's
 * theorem, https://en.wikipedia.org/wiki/Hurwitz%27s_theorem_(number_theory)].
 * Furthermore, it has no semiconvergents since all of its continued fraction
 * terms are one. The convergents are ratios of consecutive Fibonacci numbers.
 */
void TestRationalApproximationOfGoldenRatio() {
  puts("TestRationalApproximationOfGoldenRatio");
  int fibonacci_prev = 1;
  int fibonacci = 2;
  int numerator;
  int denominator;
  int max_denominator;

  for (max_denominator = 1; max_denominator <= 100; ++max_denominator) {
    while (fibonacci <= max_denominator) {
      const int fibonacci_next = fibonacci + fibonacci_prev;
      fibonacci_prev = fibonacci;
      fibonacci = fibonacci_next;
    }
    RationalApproximation(kGoldenRatio, max_denominator, NULL,
                          &numerator, &denominator);
    ABSL_CHECK(numerator == fibonacci);
    ABSL_CHECK(denominator == fibonacci_prev);
  }
}

/* Find the best rational approximation a/b of `value` by exhaustively testing
 * each possible denominator 0 < b <= max_denominator.
 */
void ExhaustiveFindBestRationalApproximation(
    double value, int max_denominator,
    int* out_numerator, int* out_denominator) {
  int best_numerator = (int) floor(value + 0.5);
  int best_denominator = 1;
  double best_error = fabs(value - best_numerator);
  int denominator;
  for (denominator = 2; denominator <= max_denominator; ++denominator) {
    const int numerator = floor(value * denominator + 0.5);
    const double error = fabs(value - ((double) numerator) / denominator);
    if (error < best_error) {
      best_numerator = numerator;
      best_denominator = denominator;
      best_error = error;
    }
  }
  const int gcd = GreatestCommonDivisor(abs(best_numerator), best_denominator);
  *out_numerator = best_numerator / gcd;
  *out_denominator = best_denominator / gcd;
}

/* Check that RationalApproximation() produces best rational approximations. */
void TestRationalApproximationIsOptimal(int max_denominator) {
  printf("TestRationalApproximationIsOptimal(%d)\n", max_denominator);
  const int kNumTrials = 20;
  int trial;
  for (trial = 0; trial < kNumTrials; ++trial) {
    const double value = -1.0 + (2.0 * rand()) / RAND_MAX;

    int numerator;
    int denominator;
    RationalApproximation(value, max_denominator, NULL,
                          &numerator, &denominator);

    ABSL_CHECK(denominator <= max_denominator);
    ABSL_CHECK(GreatestCommonDivisor(abs(numerator), denominator) == 1);

    int expected_numerator;
    int expected_denominator;
    ExhaustiveFindBestRationalApproximation(value, max_denominator,
        &expected_numerator, &expected_denominator);

    ABSL_CHECK(numerator == expected_numerator);
    ABSL_CHECK(denominator == expected_denominator);
  }
}

void TestRationalApproximationMaxTerms() {
  puts("TestRationalApproximationMaxTerms");
  int numerator;
  int denominator;

  RationalApproximation(M_SQRT2, 10000, NULL, &numerator, &denominator);
  ABSL_CHECK(numerator == 8119);
  ABSL_CHECK(denominator == 5741);
  RationalApproximation(M_PI, 10000, NULL, &numerator, &denominator);
  ABSL_CHECK(numerator == 355);
  ABSL_CHECK(denominator == 113);

  /* Expansion stops after options.max_terms continued fraction terms. */
  RationalApproximationOptions options =
      kRationalApproximationDefaultOptions;
  options.max_terms = 3;
  RationalApproximation(M_SQRT2, 10000, &options, &numerator, &denominator);
  ABSL_CHECK(numerator == 7); /* 3rd convergent, [1; 2, 2]. */
  ABSL_CHECK(denominator == 5);
  RationalApproximation(M_PI, 10000, &options, &numerator, &denominator);
  ABSL_CHECK(numerator == 333); /* 3rd convergent, [3; 7, 15]. */
  ABSL_CHECK(denominator == 106);
}

typedef struct {
  double value;
  int expected_numerator;
  int expected_denominator;
} TestCase;

/* Check a few calls with large arguments to stress test overflow checks.
 * Expected results were created with python, for instance
 *
 *   import math, fractions
 *   print(fractions.Fraction(math.pi).limit_denominator(1000000))
 *   # Prints: 3126535/995207
 */
void TestLargeArguments() {
  puts("TestLargeArguments");

  static const TestCase kTestCases[] = {
    /* value, expected_numerator, expected_denominator. */
    {M_PI,                  3126535, 995207},
    {M_SQRT2,               665857, 470832},
    {kGoldenRatio,          1346269, 832040},
    /* The following were real cases that would previously overflow. */
    {(0.989866 / 1.989866), 494933, 994933},
    {(1 / 1.989866),        500000, 994933},
    {(0.96242 / 1.96242),   48121, 98121},
    {(1 / 1.96242),         50000, 98121},
    /* Call with value in the vicinity of INT_MAX plus 2/3. The best rational
     * approximation has numerator (3 * kHugeInt + 2) and denominator 3, however,
     * that would overflow in the numerator. The best we can do is round to
     * numerator = (kHugeInt + 1), denominator = 1.
     */
    #define kHugeInt (INT_MAX - INT_MAX / 8)
    {(kHugeInt + 2.0 / 3),  (kHugeInt +  1), 1},
    /* Here, we call with value = kLargeInt - 0.25. The best rational is
     * numerator = (4 * kLargeInt - 1), denominator = 4, but that overflows. The
     * best we can do is numerator = (3 * kLargeInt - 1), denominator = 3, equal
     * to large_int - 1/3. This is more accurate than rounding to kLargeInt.
     */
    #define kLargeInt (INT_MAX / 3)
    {(kLargeInt - 0.25),    (3 * kLargeInt - 1), 3},
    {INT_MAX,               INT_MAX, 1},
    {INT_MIN,               INT_MIN, 1},
    /* Test some values outside of int range. */
    {1e30,                  INT_MAX, 1},
    {-1e30,                 INT_MIN, 1},
  };
  int i;
  for (i = 0; i < sizeof(kTestCases) / sizeof(*kTestCases); ++i) {
    const TestCase* test_case = &kTestCases[i];
    int numerator;
    int denominator;
    /* I demand the max_denominator... OF 1 MILLION. */
    RationalApproximation(test_case->value, 1000000, NULL,
                          &numerator, &denominator);

    ABSL_CHECK(numerator == test_case->expected_numerator);
    ABSL_CHECK(denominator == test_case->expected_denominator);
  }
}

int main(int argc, char** argv) {
  srand(0);
  TestRoundUpToMultiple();
  TestGreatestCommonDivisor();
  TestRationalApproximationOfSimpleRatios();
  TestRationalApproximationOfPerturbedRatios();
  TestRationalApproximationOfSqrt2();
  TestRationalApproximationOfPi();
  TestRationalApproximationOfGoldenRatio();
  TestRationalApproximationIsOptimal(5);
  TestRationalApproximationIsOptimal(10);
  TestRationalApproximationIsOptimal(20);
  TestRationalApproximationMaxTerms();
  TestLargeArguments();

  puts("PASS");
  return EXIT_SUCCESS;
}
