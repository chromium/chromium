#include "audio/dsp/portable/rational_factor_resampler.h"

#include <math.h>
#include <string.h>

#include "audio/dsp/portable/logging.h"
#include "audio/dsp/portable/math_constants.h"
#include "audio/dsp/portable/phasor_rotator.h"
#include "audio/dsp/portable/rational_factor_resampler_kernel.h"

/* Tested sample rates in Hz. */
static const float kRates[] = {
    12000.0f, 16000.0f, 32000.0f, 44100.0f, 48000.0f, (float) (16000 * M_SQRT2),
};
static const int kNumRates = sizeof(kRates) / sizeof(*kRates);

/* Implement resampling directly according to
 *   x'[m] = x(m/F') = sum_n x[n] h(m F/F' - n),
 * where h is the resampling kernel, F is the input sample rate, and F' is the
 * output sample rate.
 */
static float* ReferenceResampling(
    const RationalFactorResamplerKernel* kernel, double rational_factor,
    const float* input, int input_size, int* output_size) {
  float* output = NULL;
  int output_capacity = 0;

  int m;
  for (m = 0;; ++m) {
    const double n0 = m * rational_factor;
    /* Determine the range of n values for `sum_n x[n] h(m F/F' - n)`. */
    const int n_first = (int) floor(n0 - kernel->radius + 0.5);
    const int n_last = (int) floor(n0 + kernel->radius + 0.5);
    /* The kernel `h(m F/F' - n)` is zero outside of [n_first, n_last]. */
    ABSL_CHECK(RationalFactorResamplerKernelEval(kernel, n0 - (n_first - 1)) == 0.0);
    ABSL_CHECK(RationalFactorResamplerKernelEval(kernel, n0 - (n_last + 1)) == 0.0);

    if (n_last >= input_size) { break; }
    double sum = 0.0;
    int n;
    for (n = n_first; n <= n_last; ++n) {
      sum += (n < 0 ? 0.0 : input[n])  /* Compute `sum_n x[n] h(m F/F' - n)`. */
          * RationalFactorResamplerKernelEval(kernel, n0 - n);
    }

    if (m >= output_capacity) {
      output_capacity = (m == 0) ? 64 : 2 * (m + 1);
      output = (float*) ABSL_CHECK_NOTNULL(realloc(
            output, sizeof(float) * output_capacity));
    }
    output[m] = (float) sum;
  }

  *output_size = m;
  return output;
}

/* Compare with ReferenceResampling(). */
void TestCompareWithReferenceResampler(float filter_radius_factor) {
  printf("TestCompareWithReferenceResampler(%g)\n", filter_radius_factor);

  const int kInputSize = 50;
  float* input = ABSL_CHECK_NOTNULL(malloc(sizeof(float) * kInputSize));
  int n;
  for (n = 0; n < kInputSize; ++n) {
    input[n] = -0.5f + ((float) rand()) / RAND_MAX;
  }

  int i;
  for (i = 0; i < kNumRates; ++i) {
    int j;
    for (j = 0; j < kNumRates; ++j) {
      const float input_sample_rate_hz = kRates[i];
      const float output_sample_rate_hz = kRates[j];

      RationalFactorResamplerOptions options =
          kRationalFactorResamplerDefaultOptions;
      options.filter_radius_factor = filter_radius_factor;
      RationalFactorResampler* resampler = ABSL_CHECK_NOTNULL(
          RationalFactorResamplerMake(input_sample_rate_hz,
                                      output_sample_rate_hz,
                                      kInputSize,
                                      &options));

      int output_size = RationalFactorResamplerProcessSamples(
          resampler, input, kInputSize);

      RationalFactorResamplerKernel kernel;
      ABSL_CHECK(RationalFactorResamplerKernelInit(
          &kernel, input_sample_rate_hz, output_sample_rate_hz,
          options.filter_radius_factor, options.cutoff_proportion,
          options.kaiser_beta));
      int rational_factor_numerator;
      int rational_factor_denominator;
      RationalFactorResamplerGetRationalFactor(
          resampler, &rational_factor_numerator, &rational_factor_denominator);
      double rational_factor =
          ((double) rational_factor_numerator) / rational_factor_denominator;
      ABSL_CHECK(fabs(rational_factor - kernel.factor) <= 5e-4);

      int expected_size;
      float* expected = ReferenceResampling(
          &kernel, rational_factor, input, kInputSize, &expected_size);
      ABSL_CHECK(abs(output_size - expected_size) <= 2);

      const float* output = RationalFactorResamplerOutput(resampler);
      int n;
      for (n = 0; n < output_size && n < expected_size; ++n) {
        ABSL_CHECK(fabs(output[n] - expected[n]) <= 5e-7f);
      }

      free(expected);
      RationalFactorResamplerFree(resampler);
    }
  }

  free(input);
}

/* Test streaming with blocks of random sizes between 0 and kMaxBlockSize. */
void TestStreamingRandomBlockSizes() {
  puts("TestStreamingRandomBlockSizes");
  const int kTotalInputSize = 500;
  const int kMaxBlockSize = 20;

  /* Generate random input samples. */
  float* input = ABSL_CHECK_NOTNULL(malloc(sizeof(float) * kTotalInputSize));
  int n;
  for (n = 0; n < kTotalInputSize; ++n) {
    input[n] = -0.5f + ((float) rand()) / RAND_MAX;
  }

  int i;
  for (i = 0; i < kNumRates; ++i) {
    int j;
    for (j = 0; j < kNumRates; ++j) {
      const float input_sample_rate_hz = kRates[i];
      const float output_sample_rate_hz = kRates[j];

      /* Do "nonstreaming" resampling, processing the whole input at once. */
      RationalFactorResampler* resampler = ABSL_CHECK_NOTNULL(
          RationalFactorResamplerMake(input_sample_rate_hz,
                                      output_sample_rate_hz,
                                      kTotalInputSize,
                                      NULL));
      const int total_output_size = RationalFactorResamplerProcessSamples(
          resampler, input, kTotalInputSize);
      float* nonstreaming = ABSL_CHECK_NOTNULL(malloc( /* Save the output. */
            sizeof(float) * total_output_size));
      memcpy(nonstreaming, RationalFactorResamplerOutput(resampler),
             sizeof(float) * total_output_size);
      RationalFactorResamplerFree(resampler);

      /* Do "streaming" resampling, passing successive blocks of input. */
      float* streaming = ABSL_CHECK_NOTNULL(malloc(
            sizeof(float) * total_output_size));
      float* input_block = ABSL_CHECK_NOTNULL(malloc(
            sizeof(float) * kMaxBlockSize));
      resampler = ABSL_CHECK_NOTNULL(
          RationalFactorResamplerMake(input_sample_rate_hz,
                                      output_sample_rate_hz,
                                      kMaxBlockSize,
                                      NULL));
      ABSL_CHECK(total_output_size ==
            RationalFactorResamplerNextOutputSize(resampler, kTotalInputSize));
      int streaming_size = 0;

      for (n = 0; n < kTotalInputSize;) {
        /* Get the next block of input samples, having random size
         * 0 <= input_block_size <= kMaxBlockSize.
         */
        int input_block_size = (int) floor(
            (((float) rand()) / RAND_MAX) * kMaxBlockSize + 0.5);
        if (input_block_size > kTotalInputSize - n) {
          input_block_size = kTotalInputSize - n;
        }
        /* NOTE: We write the test this way for sake of demonstration and so
         * that potential buffer overruns are detectable with valgrind or asan.
         * More practically, we would read directly from input without copying.
         */
        memcpy(input_block, input + n, sizeof(float) * input_block_size);
        n += input_block_size;

        /* Resample the block. */
        const int expected_output_block_size =
            RationalFactorResamplerNextOutputSize(resampler, input_block_size);
        const int output_block_size = RationalFactorResamplerProcessSamples(
            resampler, input_block, input_block_size);
        float* output_block = RationalFactorResamplerOutput(resampler);

        ABSL_CHECK(expected_output_block_size == output_block_size);

        /* Append output_block to the `streaming` array. */
        ABSL_CHECK(streaming_size + output_block_size <= total_output_size);
        memcpy(streaming + streaming_size, output_block,
               sizeof(float) * output_block_size);
        streaming_size += output_block_size;
      }
      RationalFactorResamplerFree(resampler);
      free(input_block);

      ABSL_CHECK(n == kTotalInputSize);
      ABSL_CHECK(streaming_size == total_output_size);

      /* Streaming vs. nonstreaming outputs should match. */
      int m;
      for (m = 0; m < total_output_size; ++m) {
        ABSL_CHECK(fabs(streaming[m] - nonstreaming[m]) < 1e-6f);
      }

      free(nonstreaming);
      free(streaming);
    }
  }

  free(input);
}

/* Resampling a sine wave should produce again a sine wave. */
void TestResampleSineWave() {
  puts("TestResampleSineWave");
  const float kFrequency = 1100.7f;

  const int kInputSize = 100;
  float* input = ABSL_CHECK_NOTNULL(malloc(sizeof(float) * kInputSize));

  int i;
  for (i = 0; i < kNumRates; ++i) {
    const float input_sample_rate_hz = kRates[i];

    /* Generate sine wave. */
    PhasorRotator oscillator;
    PhasorRotatorInit(&oscillator, kFrequency, input_sample_rate_hz);
    int n;
    for (n = 0; n < kInputSize; ++n) {
      input[n] = PhasorRotatorSin(&oscillator);
      PhasorRotatorNext(&oscillator);
    }

    int j;
    for (j = 0; j < kNumRates; ++j) {
      const float output_sample_rate_hz = kRates[j];

      RationalFactorResampler* resampler = ABSL_CHECK_NOTNULL(
          RationalFactorResamplerMake(input_sample_rate_hz,
                                      output_sample_rate_hz,
                                      kInputSize,
                                      NULL));

      /* Run resampler on the sine wave samples. */
      const int output_size = RationalFactorResamplerProcessSamples(
          resampler, input, kInputSize);
      const float* output = RationalFactorResamplerOutput(resampler);

      RationalFactorResamplerOptions options =
          kRationalFactorResamplerDefaultOptions;
      RationalFactorResamplerKernel kernel;
      ABSL_CHECK(RationalFactorResamplerKernelInit(
          &kernel, input_sample_rate_hz, output_sample_rate_hz,
          options.filter_radius_factor, options.cutoff_proportion,
          options.kaiser_beta));

      const double expected_size = (kInputSize - kernel.radius) / kernel.factor;
      ABSL_CHECK(fabs(output_size - expected_size) <= 1.0);

      /* Compare output to sine wave generated at the output sample rate. */
      PhasorRotatorInit(&oscillator, kFrequency, output_sample_rate_hz);
      /* We ignore the first few output samples because they depend on input
       * samples at negative times, which are extrapolated as zeros.
       */
      const int num_to_ignore = 1 + (int) floor(kernel.radius / kernel.factor);
      int m;
      for (m = 0; m < output_size; ++m) {
        if (m >= num_to_ignore) {
          float expected = PhasorRotatorSin(&oscillator);
          ABSL_CHECK(fabs(output[m] - expected) < 0.005f);
        }
        PhasorRotatorNext(&oscillator);
      }

      RationalFactorResamplerFree(resampler);
    }
  }

  free(input);
}

/* Test resampling a chirp. */
void TestResampleChirp() {
  puts("TestResampleChirp");

  int i;
  for (i = 0; i < kNumRates; ++i) {
    const float input_sample_rate_hz = kRates[i];
    const float kDurationSeconds = 0.025f;
    const int input_size = (int) (kDurationSeconds * input_sample_rate_hz);
    float* input = ABSL_CHECK_NOTNULL(malloc(sizeof(float) * input_size));

    /* Generate chirp signal, sweeping linearly from 0 to max_frequency_hz. */
    const float max_frequency_hz = 0.45f * input_sample_rate_hz;
    const float chirp_slope = max_frequency_hz / kDurationSeconds;

    int n;
    for (n = 0; n < input_size; ++n) {
      const float t = n / input_sample_rate_hz;
      input[n] = (float) sin(M_PI * chirp_slope * t * t);
    }

    int j;
    for (j = 0; j < kNumRates; ++j) {
      const float output_sample_rate_hz = kRates[j];

      RationalFactorResampler* resampler = ABSL_CHECK_NOTNULL(
          RationalFactorResamplerMake(input_sample_rate_hz,
                                      output_sample_rate_hz,
                                      input_size,
                                      NULL));

      /* Run resampler on the chirp. */
      const int output_size = RationalFactorResamplerProcessSamples(
          resampler, input, input_size);
      const float* output = RationalFactorResamplerOutput(resampler);

      RationalFactorResamplerOptions options =
          kRationalFactorResamplerDefaultOptions;
      RationalFactorResamplerKernel kernel;
      ABSL_CHECK(RationalFactorResamplerKernelInit(
          &kernel, input_sample_rate_hz, output_sample_rate_hz,
          options.filter_radius_factor, options.cutoff_proportion,
          options.kaiser_beta));
      /* Get kernel's cutoff frequency. */
      const float cutoff_hz = kernel.radians_per_sample
          * input_sample_rate_hz / (2 * M_PI);

      /* Compare output to chirp generated at the output sample rate. */
      int m;
      for (m = 0; m < output_size; ++m) {
        const float t = m / output_sample_rate_hz;
        /* Compute the chirp's instantaneous frequency at t. */
        const float chirp_frequency_hz = chirp_slope * t;

        /* Skip samples in the transition between passband and stopband. */
        if (fabs(chirp_frequency_hz - cutoff_hz) < 0.3f * cutoff_hz) {
          continue;
        }

        float expected;
        if (chirp_frequency_hz < cutoff_hz) {
          expected = (float) sin(M_PI * chirp_slope * t * t);
        } else {
          /* Expect output near zero when chirp frequency is above cutoff_hz. */
          expected = 0.0f;
        }
        ABSL_CHECK(fabs(output[m] - expected) < 0.04f);
      }

      RationalFactorResamplerFree(resampler);
    }

    free(input);
  }
}

/* Test sample dropping behavior when input_size > max_input_size. */
void TestInputSizeExceedsMax() {
  puts("TestInputSizeExceedsMax");
  const int kInputSize = 120;
  const int kMaxInputSize = 50;

  /* Generate random input samples. */
  float* input = ABSL_CHECK_NOTNULL(malloc(sizeof(float) * kInputSize));
  int n;
  for (n = 0; n < kInputSize; ++n) {
    input[n] = -0.5f + ((float) rand()) / RAND_MAX;
  }

  int i;
  for (i = 0; i < kNumRates; ++i) {
    int j;
    for (j = 0; j < kNumRates; ++j) {
      const float input_sample_rate_hz = kRates[i];
      const float output_sample_rate_hz = kRates[j];

      RationalFactorResampler* resampler = ABSL_CHECK_NOTNULL(
          RationalFactorResamplerMake(input_sample_rate_hz,
                                      output_sample_rate_hz,
                                      kMaxInputSize,
                                      NULL));

      /* Process the first 50 samples. */
      RationalFactorResamplerProcessSamples(resampler, input, 50);
      /* Process remaining 70 samples, exceeding kMaxInputSize. The result will
       * be as if resampler had been reset and processed the last 50 samples.
       */
      const int output_size = RationalFactorResamplerProcessSamples(
          resampler, input + 50, 70);

      float* output_with_drop = ABSL_CHECK_NOTNULL(malloc( /* Save the output. */
            sizeof(float) * output_size));
      memcpy(output_with_drop, RationalFactorResamplerOutput(resampler),
             sizeof(float) * output_size);

      /* Compare with explicitly resetting and processing last 25 samples. */
      RationalFactorResamplerReset(resampler);
      const int expected_size = RationalFactorResamplerProcessSamples(
          resampler, input + 70, 50);
      const float* expected = RationalFactorResamplerOutput(resampler);

      ABSL_CHECK(output_size == expected_size);
      int m;
      for (m = 0; m < output_size; ++m) {
        ABSL_CHECK(fabs(output_with_drop[m] - expected[m]) < 1e-6f);
      }

      free(output_with_drop);
      RationalFactorResamplerFree(resampler);
    }
  }

  free(input);
}

int main(int argc, char** argv) {
  srand(0);
  TestCompareWithReferenceResampler(4.0f);
  TestCompareWithReferenceResampler(5.0f);
  TestCompareWithReferenceResampler(17.0f);
  TestStreamingRandomBlockSizes();
  TestResampleSineWave();
  TestResampleChirp();
  TestInputSizeExceedsMax();

  puts("PASS");
  return EXIT_SUCCESS;
}
