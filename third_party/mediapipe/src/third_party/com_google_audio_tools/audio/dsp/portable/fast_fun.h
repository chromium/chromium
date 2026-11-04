/*
 * Copyright 2019-2020, 2022-2023 Google LLC
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

/* Fast log2, exp2, pow, and tanh functions in C.
 *
 * This library implements fast log2 (log base 2), exp2 (2^x), power x^y, tanh,
 * and sigmoid functions for 32-bit float arguments through float bit
 * manipulations and small lookup tables.
 *
 * NOTE: This library assumes `float` uses 32-bit IEEE 754 floating point
 * representation. Most current machines do, but not all.
 *
 * You can use rules of logs and exponents to compute related functions:
 *  - Natural log: log(x) = FastLog2(x) * M_LN2.
 *  - Log base 10: log10(x) = FastLog2(x) / log2(10).
 *  - Log base b: log_b(x) = FastLog2(x) / log2(b).
 *  - Exponential: e^x = FastExp2(x / M_LN2).
 *  - Power of 10: 10^x = FastExp2(log2(10) * x).
 *  - Power of constant b: b^x = FastExp2(log2(b) * x).
 *  - Square root: sqrt(x) = FastPow(x, 0.5f)
 *    [Note: on modern x86 processors, `sqrtf()` from math.h is comparably fast,
 *    but more accurate, compiling to a `sqrtss` SSE instruction.]
 *
 * Accuracy:
 *  - FastLog2(x) has max absolute error of about 0.003.
 *  - FastExp2(x) has max relative error of about 0.3%.
 *  - FastPow(x) has max relative error of about 0.5%.
 *  - FastTanh(x) has max absolute error of about 0.0008.
 *  - FastSigmoid(x) has max absolute error of about 0.0004.
 *
 * Benchmarks (measured by extras/benchmark/fast_fun_benchmark.cpp):
 * These functions take an order of magnitude less time than those in math.h.
 * Benchmark timings of calling each function 1000 times on an Intel Skylake
 * Xeon (2000 MHz CPUs). To get the time for an individual call, divide times in
 * the table by 1000, e.g. one FastLog2 call costs 1 ns.
 *
 * Results on SkyLake, 2023-02-20:
 * ----------------------------------------------------------
 * Benchmark                Time             CPU   Iterations
 * ----------------------------------------------------------
 * BM_FastLog2           1025 ns         1022 ns       587651
 * BM_math_log2f         4745 ns         4737 ns       145170
 * BM_FastExp2            761 ns          760 ns       899909
 * BM_math_exp2f         3619 ns         3614 ns       193111
 * BM_FastPow            2170 ns         2166 ns       323478
 * BM_math_powf         15778 ns        15754 ns        42702
 * BM_FastTanh           1673 ns         1671 ns       414148
 * BM_math_tanhf        22670 ns        22632 ns        30033
 * BM_FastSigmoid        1492 ns         1489 ns       465835
 * BM_math_sigmoid       4907 ns         4897 ns       143939
 */

#ifndef AUDIO_DSP_PORTABLE_FAST_FUN_H_
#define AUDIO_DSP_PORTABLE_FAST_FUN_H_

#include <stdint.h>
#include <string.h>

#include "audio/dsp/portable/math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Check statically that sizeof(float) == sizeof(int32_t). If compiling fails
 * here, the target system has unusual floats and this library won't work.
 */
typedef char kFastFunStaticAssert_FLOAT_AND_INT32_T_MUST_HAVE_SAME_SIZE[
    (sizeof(float) == sizeof(int32_t)) ? 1:-1];

extern const float kFastFunLog2Table[256];
extern const int32_t kFastFunExp2Table[256];

/* Fast log base 2, accurate to about 0.003 max abs error, ~1 ns on Skylake.
 *
 * Limitations:
 *  - x is assumed to be positive and finite.
 *  - If x is a denormal, i.e. a small value less than about 1e-38, the result
 *    is less accurate.
 *
 * Algorithm:
 * Decomposing x as x = 2^exponent * (1 + mantissa), where mantissa is in the
 * half-open interval [0, 1), we have
 *
 *   log2(x) = log2(2^exponent * (1 + mantissa))
 *           = exponent + log2(1 + mantissa).
 *
 * We compute as in the last line, using a lookup table of log2 over [1, 2) to
 * approximate the second term.
 *
 * This simple algorithm has been reinvented and published several times. A few
 * are linked here:
 * https://dspguru.com/dsp/tricks/quick-and-dirty-logarithms/
 * http://www.icsi.berkeley.edu/~fractor/papers/friedland_84.pdf
 * http://www.hxa.name/articles/content/fast-pow-adjustable_hxa7241_2007.html
 */
static float FastLog2(float x) {
  int32_t x_bits;
  /* Convert to int32_t without changing underlying bits. Beware that type
   * punning `*((int*)&x)` is technically undefined behavior (UB). Using memcpy
   * avoids UB, and optimizes well on most compilers to one load and one store.
   */
  memcpy(&x_bits, &x, sizeof(float));
  /* Extract the top 8 bits of the mantissa. Magic constant 23 is the number of
   * mantissa bits in a 32-bit float.
   * NOTE: For speed, we don't do corner case logic to extract denormal values
   * properly. Results are then still reasonable but less accurate.
   */
  const int32_t significand = (x_bits >> (23 - 8)) & ((1 << 8) - 1);
  /* Compute log2(x) = exponent + log2(1 + mantissa). The -127 debiases the
   * exponent.
   */
  return (((x_bits >> 23) & 0xFF) - 127) + kFastFunLog2Table[significand];
}

/* Fast 2^x, accurate to about 0.3% relative error, ~0.8 ns on Skylake.
 *
 * Limitations:
 *  - Assumes |x| <= 126. (Otherwise, result may be nonsensical.)
 *
 * Algorithm:
 * We more or less do the log2 algorithm in reverse order. Decompose x into an
 * integer part and fractional part, x = x_int + x_frac, then 2^x is
 *
 *   2^x = 2^(x_int + x_frac) = 2^x_int * 2^x_frac.
 *
 * The last expression looks float representation if written as
 *
 *   2^x = 2^x_int * (1 + mantissa), where mantissa = 2^x_frac - 1.
 *
 * The approach is first convert x to a 8.8 fixed-point representation, then
 * extract `x_int` from the higher 8 bits and `x_frac` from the lower 8 bits. We
 * construct the result as a float whose exponent is `x_int` and using `x_frac`
 * as an index into a lookup table to determine the mantissa.
 *
 * This is a simplified (faster, less accurate) version of the approach in
 * http://soc.knu.ac.kr/video_lectures/15_4.pdf
 */
static float FastExp2(float x) {
  /* Convert to fixed-point representation in such a way that
   * y_bits = round(2^8 * (x + 127)).
   * We use `y_bits >> 8` below as the exponent of the result. The +127 offset
   * is so that this exponent is already in biased form. The 23 appears as the
   * number of mantissa bits in a 32-bit float.
   */
  const float y = x + (127 + (1 << (23 - 8)));
  int32_t y_bits;
  memcpy(&y_bits, &y, sizeof(float));
  /* Construct result as a float with exponent `y_bits >> 8` and mantissa
   * `kFastFunExp2Table[y_bits & 0xFF]`. This effectively computes
   * result = 2^x = 2^x_int * (1 + mantissa), where mantissa = 2^x_frac - 1.
   */
  const int32_t result_bits =
      ((y_bits & (0xFF << 8)) << (23 - 8)) | kFastFunExp2Table[y_bits & 0xFF];
  float result;
  memcpy(&result, &result_bits, sizeof(float));
  return result;
}

/* Fast power x^y for x > 0, with about 0.5% relative error, ~2 ns on Skylake.
 *
 * Limitations:
 *  - Assumes x is positive and finite.
 *  - Assumes 1e-37 <= |x^y| <= 1e37, i.e. that the exact result would be
 *    neither very large nor very close to zero.
 *
 * Otherwise, result may be nonsensical.
 *
 * The implementation comes from basic rules of logs and exponents,
 * x^y = 2^(log2(x^y)) = 2^(log2(x) * y) ~= FastExp2(FastLog2(x) * y).
 */
static float FastPow(float x, float y) { return FastExp2(FastLog2(x) * y); }

/* Fast tanh(x), accurate to about 0.0008 max abs error, ~1.7 ns on Skylake.
 *
 * The result is valid for non-NaN x (even for large x). The implementation
 * comes from the relationship
 * tanh(x) = (exp(x) - exp(-x)) / (exp(x) + exp(-x))
 *         = (exp(2x) - 1) / (exp(2x) + 1)
 *         ~= (y - 1) / (y + 1) with y = FastExp2((2 / M_LN2) * x).
 */
static float FastTanh(float x) {
  /* For |x| >= 9.011, tanh(x) is exactly 1 or -1 to float32 precision. */
  if (x >= 9.011f) { return 1.0f; }
  if (x <= -9.011f) { return -1.0f; }
  const float exp_2x = FastExp2((float)(2.0 / M_LN2) * x);
  return (exp_2x - 1.0f) / (exp_2x + 1.0f);
}

/* Fast sigmoid (logistic) function, accurate to about 0.0004 max abs error,
 * ~1.5 ns on Skylake.
 *
 * The result is valid for non-NaN x (even for large x). The implementation is
 *
 *   sigmoid(x) = 1 / (1 + exp(-x))
 *              ~= 1 / (1 + FastExp2((-1 / M_LN2) * x).
 */
static float FastSigmoid(float x) {
  /* For |x| >= 8.517, the exact value is within 0.0002 of saturating. */
  if (x >= 8.517f) { return 1.0f; }
  if (x <= -8.517f) { return 0.0f; }
  return 1.0f / (1.0f + FastExp2((float)(-1.0 / M_LN2) * x));
}

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* AUDIO_DSP_PORTABLE_FAST_FUN_H_ */
