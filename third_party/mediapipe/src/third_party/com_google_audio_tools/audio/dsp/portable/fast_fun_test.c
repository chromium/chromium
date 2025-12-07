#include "audio/dsp/portable/fast_fun.h"

#include <math.h>
#include <string.h>

#include "audio/dsp/portable/fast_fun_compute_tables.h"
#include "audio/dsp/portable/logging.h"

#ifndef M_LN2
#define M_LN2 0.693147180559945309417 /* log(2) */
#endif                                /* M_LN2 */

static double RandUniform() { return (double)rand() / RAND_MAX; }

/* Compare FastLog2 with math.h. */
void TestFastLog2Accuracy() {
  /* An arbitrary spot check at x=4.2. */
  ABSL_CHECK(fabs(FastLog2(4.2) - log(4.2) / M_LN2) <= 0.002);

  const double kThreshold = 0.003;
  double max_abs_error = 0.0;
  int i;

  /* Check thoroughly at random positions. To test a wide range, we first make x
   * in [-125.5, 125.5], then make y = exp2(x), so that y is distributed over
   * most of the finite float range, but excluding denormals.
   */
  for (i = 0; i < 10000; ++i) {
    const double x = 125.5 * (2 * RandUniform() - 1);
    const double y = exp(M_LN2 * x);

    const double abs_error = fabs(FastLog2(y) - x);
    if (abs_error > max_abs_error) {
      max_abs_error = abs_error;
      if (abs_error >= kThreshold) {
        printf("y=%g: FastLog2(y)=%g log2(y)=%g\n", y, FastLog2(y), x);
      }
    }
  }

  ABSL_CHECK(max_abs_error < kThreshold);
}

/* Compare FastExp2 with math.h. */
void TestFastExp2Accuracy() {
  /* An arbitrary spot check at x=4.2. */
  ABSL_CHECK(fabs(FastExp2(4.2) / exp(4.2 * M_LN2) - 1) <= 6e-4);

  const double kThreshold = 0.003;
  double max_rel_error = 0.0;
  int i;

  /* Check thoroughly at random positions over |x| <= 125.5, just within the |x| <=
   * 126 range that FastExp2 is supposed to work over.
   */
  for (i = 0; i < 10000; ++i) {
    const double x = 125.5 * (2 * RandUniform() - 1);
    const double y = exp(M_LN2 * x);

    const double rel_error = fabs((FastExp2(x) / y) - 1);
    if (rel_error > max_rel_error) {
      max_rel_error = rel_error;
      if (rel_error >= kThreshold) {
        printf("x=%g: FastExp2(x)=%g exp2(x)=%g\n", x, FastExp2(x), y);
      }
    }
  }

  ABSL_CHECK(max_rel_error < kThreshold);
}

/* Compare FastPow with math.h. */
void TestFastPowAccuracy() {
  double max_rel_error = 0.0;

  /* Check x^y over a 2-D grid of points 0.1 <= x <= 50, -2 <= y <= 2. */
  int i;
  for (i = 1; i <= 500; ++i) {
    const double x = 0.1 * i;
    int j;
    for (j = -20; j <= 20; ++j) {
      const double y = 0.1 * j;

      const double rel_error = fabs(FastPow(x, y) / pow(x, y) - 1.0);
      if (rel_error > max_rel_error) {
        max_rel_error = rel_error;
      }
    }
  }

  ABSL_CHECK(max_rel_error < 0.005);
}

/* Compare FastTanh with math.h. */
void TestFastTanhAccuracy() {
  /* Determined by testing 1 million random points uniformly in [-12, 12]. */
  const double kThreshold = 0.0008;
  double max_abs_error = 0.0;
  int i;

  for (i = 0; i < 10000; ++i) {
    const double x = 12.0 * (2 * RandUniform() - 1);
    const double y = tanh(x);

    const double abs_error = fabs(FastTanh(x) - y);
    if (abs_error > max_abs_error) {
      max_abs_error = abs_error;
      if (abs_error >= kThreshold) {
        printf("x=%g: FastTanh(x)=%g tanh(x)=%g\n", x, FastTanh(x), y);
      }
    }
  }

  ABSL_CHECK(max_abs_error < kThreshold);

  ABSL_CHECK(FastTanh(0.0f) == 0.0f);
  ABSL_CHECK(FastTanh(1000.0f) == 1.0f); /* Check large arguments. */
  ABSL_CHECK(FastTanh(-1000.0f) == -1.0f);
}

/* Finite differences of FastLog2 are close to exact derivative. */
void TestFastLog2DerivativeAccuracy() {
  double dlog2_max_rel_error = 0.0;
  int i;

  for (i = 0; i < 10000; ++i) {
    const double x = 5 * (2 * RandUniform() - 1);
    const double y = exp(M_LN2 * x);
    const double dy = 0.05 * y;

    /* Compute centered difference of FastLog2(y) around y. */
    const double dlog2_numerical =
        (FastLog2(y + dy) - FastLog2(y - dy)) / (2 * dy);
    /* Exact derivative of log2(y) is 1 / (y * log(2)). */
    const double dlog2_exact = 1.0 / (y * M_LN2);
    const double dlog2_rel_error = fabs(dlog2_numerical / dlog2_exact - 1.0);
    if (dlog2_rel_error > dlog2_max_rel_error) {
      dlog2_max_rel_error = dlog2_rel_error;
    }
  }

  ABSL_CHECK(dlog2_max_rel_error < 0.04);
}

/* Finite differences of FastExp2 are close to exact derivative. */
void TestFastExp2DerivativeAccuracy() {
  double dexp2_max_rel_error = 0.0;
  int i;

  for (i = 0; i < 10000; ++i) {
    const double x = 5 * (2 * RandUniform() - 1);
    const double y = exp(M_LN2 * x);
    const double dx = 0.05;

    /* Compute centered difference of FastExp2(x) around x. */
    const double dexp2_numerical =
        (FastExp2(x + dx) - FastExp2(x - dx)) / (2 * dx);
    /* Exact derivative of 2^x is log(2) * 2^x. */
    const double dexp2_exact = M_LN2 * y;
    const double dexp2_rel_error = fabs(dexp2_numerical / dexp2_exact - 1.0);
    if (dexp2_rel_error > dexp2_max_rel_error) {
      dexp2_max_rel_error = dexp2_rel_error;
    }
  }

  ABSL_CHECK(dexp2_max_rel_error < 0.04);
}

/* Checks that FastLog2 is monotonically increasing. */
void TestFastLog2Monotonicity() {
  float x;
  float prev_value = FastLog2(1e-3f / 1.2f);
  for (x = 1e-3f; x <= 1e3f; x *= 1.2f) { /* About 80 steps. */
    float value = FastLog2(x);
    ABSL_CHECK(value > prev_value);
    prev_value = value;
  }
}

/* Checks that FastExp2 is monotonically increasing. */
void TestFastExp2Monotonicity() {
  float x;
  float prev_value = FastExp2(-8.2f);
  for (x = -8; x <= 8; x += 0.2f) { /* About 80 steps. */
    float value = FastExp2(x);
    ABSL_CHECK(value > prev_value);
    prev_value = value;
  }
}

/* Checks that FastTanh is monotonically increasing. */
void TestFastTanhMonotonicity() {
  float x;
  float prev_value = FastTanh(-8.2f);
  for (x = -8; x <= 8; x += 0.2f) { /* About 80 steps. */
    float value = FastTanh(x);
    ABSL_CHECK(value > prev_value);
    prev_value = value;
  }
}

/* Checks that FastTanh is close to having odd symmetry about x=0. */
void TestFastTanhOddSymmetry() {
  double max_error = 0.0;
  int i;

  for (i = 0; i < 10000; ++i) {
    const double x = 10.0 * (2 * RandUniform() - 1);
    const double y1 = FastTanh(x);
    const double y2 = -FastTanh(-x);
    const double error = fabs(y1 - y2);
    if (error > max_error) {
      max_error = error;
    }
  }

  ABSL_CHECK(max_error < 1e-6);
}

/* Tests that hardcoded lookup tables agree with table computation functions. */
void TestCheckTables() {
  int i;
  {
    float table[256];
    ComputeFastFunLog2Table(table);
    for (i = 0; i < 256; ++i) {
      ABSL_CHECK(fabs(table[i] - kFastFunLog2Table[i]) <= 1e-8);
    }
  }
  {
    int32_t table[256];
    ComputeFastFunExp2Table(table);
    for (i = 0; i < 256; ++i) {
      ABSL_CHECK(abs(table[i] - kFastFunExp2Table[i]) <= 1);
    }
  }
}

/* Prints lookup tables. Called if the program runs with --print_tables. */
void PrintTables() {
  int i;
  {
    float table[256];
    ComputeFastFunLog2Table(table);
    printf("const float kFastFunLog2Table[256] = {\n    ");
    for (i = 0; i < 256; ++i) {
      if (i > 0) {
        printf((i % 4) ? ", " : ",\n    ");
      }
      printf("%.9g", table[i]);
    }
    printf("\n};\n");
  }
  {
    int32_t table[256];
    ComputeFastFunExp2Table(table);
    printf("\nconst int32_t kFastFunExp2Table[256] = {\n    ");
    for (i = 0; i < 256; ++i) {
      if (i > 0) {
        printf((i % 4) ? ", " : ",\n    ");
      }
      printf("%ld", (long)table[i]);
    }
    printf("\n};\n");
  }
}

int main(int argc, char** argv) {
  if (argc == 2 && !strcmp(argv[1], "--print_tables")) {
    PrintTables();
    return EXIT_SUCCESS;
  }

  TestFastLog2Accuracy();
  TestFastExp2Accuracy();
  TestFastPowAccuracy();
  TestFastTanhAccuracy();
  TestFastLog2DerivativeAccuracy();
  TestFastExp2DerivativeAccuracy();
  TestFastLog2Monotonicity();
  TestFastExp2Monotonicity();
  TestFastTanhMonotonicity();
  TestFastTanhOddSymmetry();
  TestCheckTables();

  puts("PASS");
  return EXIT_SUCCESS;
}
