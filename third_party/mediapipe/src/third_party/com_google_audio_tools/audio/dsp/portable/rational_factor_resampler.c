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

#include "audio/dsp/portable/rational_factor_resampler.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "audio/dsp/portable/rational_factor_resampler_kernel.h"

const RationalFactorResamplerOptions kRationalFactorResamplerDefaultOptions = {
  /*max_denominator=*/1000,
  /*rational_approximation_options=*/NULL,
  /*filter_radius_factor=*/5.0f,
  /*cutoff_proportion=*/0.9f,
  /*kaiser_beta=*/5.658f,
};

struct RationalFactorResampler {
  /* Buffer of delayed input samples with capacity for num_taps samples. When
   * calling to ProcessSamples(), unconsumed input samples are stored in this
   * buffer so that they are available in the next call to ProcessSamples().
   */
  float* delayed_input;
  /* Polyphase filters, stored backward so that they can be applied as a dot
   * product. `filters[num_taps * p + k]` is the kth coefficient for phase p.
   */
  float* filters;
  /* Output buffer. Its capacity is large enough to hold the output from
   * resampling an input with size up to max(max_input_size, FlushSize).
   */
  float* output;
  /* Number of samples currently in the `delayed_input` buffer. */
  int delayed_input_size;
  /* Number of taps in each filter phase. */
  int num_taps;
  /* Radius of the filters in units of input samples. */
  int radius;
  /* Max supported input size in calls to ProcessSamples(). */
  int max_input_size;
  /* The rational approximating the requested resampling factor,
   *
   *   factor_numerator / factor_denominator
   *   ~= input_sample_rate_hz / output_sample_rate_hz,
   *
   * where factor_denominator is also the number of filter phases.
   */
  int factor_numerator;
  int factor_denominator;
  /* Equal to floor(factor_numerator / factor_denominator). */
  int factor_floor;
  /* Phase step between successive output samples, equal to
   * factor_numerator % factor_denominator.
   */
  int phase_step;
  int phase;
};

RationalFactorResampler* RationalFactorResamplerMake(
    float input_sample_rate_hz,
    float output_sample_rate_hz,
    int max_input_size,
    const RationalFactorResamplerOptions* options) {
  if (!options) { options = &kRationalFactorResamplerDefaultOptions; }
  RationalFactorResamplerKernel kernel;
  if (!RationalFactorResamplerKernelInit(
          &kernel, input_sample_rate_hz, output_sample_rate_hz,
          /*filter_radius_factor=*/options->filter_radius_factor,
          /*cutoff_proportion=*/options->cutoff_proportion,
          /*kaiser_beta=*/options->kaiser_beta) ||
      max_input_size <= 0 ||
      options->max_denominator <= 0) {
    return NULL;
  }

  const int radius = (int) ceil(kernel.radius);
  /* We create the polyphase filters h_p by sampling the kernel h(x) as
   *
   *   h_p[k] := h(p/b + k),  p = 0, 1, ..., b - 1,
   *
   * as described in the .h file. Since h(x) is nonzero for |x| <= radius,
   * h_p[k] is nonzero when |p/b + k| <= radius, or
   *
   *   -radius - p/b <= k <= radius - p/b.
   *
   * Independently of p, the nonzero support of h_p[k] is within
   *
   *   -radius - (b - 1)/b <= k <= radius.
   *
   * Since k and radius are integers, we can round the lower bound up to
   * conclude the nonzero support is within |k| <= radius. Therefore, we sample
   * h(p/b + k) for |k| <= radius, and the number of taps is 2 * radius + 1.
   */
  const int num_taps = 2 * radius + 1;
  /* Approximate resampling factor as a rational number, > 1 if downsampling. */
  int factor_numerator;
  int factor_denominator;
  RationalApproximation(kernel.factor,
                        options->max_denominator,
                        options->rational_approximation_options,
                        &factor_numerator,
                        &factor_denominator);
  /* For flushing, max_input_size must be at least num_taps - 1. */
  if (num_taps - 1 > max_input_size) {
    max_input_size = num_taps - 1;
  }
  /* Get the max possible output size for the given max input size. */
  const int max_output = (int) ((((int64_t) max_input_size) * factor_denominator
                                 + factor_numerator - 1) / factor_numerator);

  RationalFactorResampler* resampler = (RationalFactorResampler*) malloc(
      sizeof(RationalFactorResampler));
  if (resampler == NULL) {
    return NULL;
  }

  resampler->filters = NULL;
  resampler->delayed_input = NULL;
  resampler->output = NULL;

  /* Allocate internal buffers. */
  if (!(resampler->filters = (float*) malloc(
          sizeof(float) * factor_denominator * num_taps)) ||
      !(resampler->delayed_input = (float*) malloc(sizeof(float) * num_taps)) ||
      !(resampler->output = (float*) malloc(sizeof(float) * max_output))) {
    RationalFactorResamplerFree(resampler);
    return NULL;
  }

  resampler->num_taps = num_taps;
  resampler->radius = radius;
  resampler->max_input_size = max_input_size;
  resampler->factor_numerator = factor_numerator;
  resampler->factor_denominator = factor_denominator;
  resampler->factor_floor =
      factor_numerator / factor_denominator; /* Integer divide. */
  resampler->phase_step = factor_numerator % factor_denominator;

  /* Compute polyphase resampling filter coefficients. */
  float* coeffs = resampler->filters;
  int phase;
  for (phase = 0; phase < factor_denominator; ++phase) {
    const double offset = ((double) phase) / factor_denominator;
    int k;
    for (k = -radius; k <= radius; ++k) {
      /* Store filter backwards so that convolution becomes a dot product. */
      coeffs[radius - k] = (float) RationalFactorResamplerKernelEval(
          &kernel, offset + k);
    }
    coeffs += num_taps;
  }

  RationalFactorResamplerReset(resampler);
  return resampler;
}

void RationalFactorResamplerFree(RationalFactorResampler* resampler) {
  if (resampler) {
    free(resampler->output);
    free(resampler->delayed_input);
    free(resampler->filters);
    free(resampler);
  }
}

void RationalFactorResamplerReset(RationalFactorResampler* resampler) {
  assert(resampler != NULL);
  int i;
  for (i = 0; i < resampler->radius; ++i) {
    resampler->delayed_input[i] = 0.0f;
  }

  resampler->phase = 0;
  resampler->delayed_input_size = resampler->radius;
}

void RationalFactorResamplerGetRationalFactor(
    const RationalFactorResampler* resampler,
    int* factor_numerator, int* factor_denominator) {
  assert(resampler != NULL);
  assert(factor_numerator != NULL);
  assert(factor_denominator != NULL);
  *factor_numerator = resampler->factor_numerator;
  *factor_denominator = resampler->factor_denominator;
}

float* RationalFactorResamplerOutput(const RationalFactorResampler* resampler) {
  return resampler->output;
}

int RationalFactorResamplerMaxInputSize(
    const RationalFactorResampler* resampler) {
  return resampler->max_input_size;
}

int RationalFactorResamplerFlushSize(const RationalFactorResampler* resampler) {
  assert(resampler != NULL);
  /* ProcessSamples() continues until there are less than num_taps input
   * samples. By appending (num_taps - 1) zeros to the input, we gaurantee that
   * after the call to ProcessSamples(), delayed_input is only zeros.
   *
   * NOTE: For API simplicity, this flush size is intentionally constant for a
   * resampler instance. It may be larger than necessary for the current state.
   * The flushed output has up to `num_taps / factor` more zeros than necessary.
   * For common parameters and sample rates, this is 10 to 30 samples and under
   * 2 ms, short enough that simplicity outweighs this minor inefficiency.
   */
  return resampler->num_taps - 1;
}

int RationalFactorResamplerNextOutputSize(
    const RationalFactorResampler* resampler, int input_size) {
  assert(resampler != NULL);
  const int min_consumed_input =
      1 + input_size + resampler->delayed_input_size - resampler->num_taps;
  if (min_consumed_input <= 0) { return 0; }

  return (int) ((((int64_t) min_consumed_input) * resampler->factor_denominator
                 - resampler->phase + resampler->factor_numerator - 1)
                / resampler->factor_numerator);
}

int RationalFactorResamplerProcessSamples(
    RationalFactorResampler* resampler, const float* input, int input_size) {
  assert(resampler != NULL);
  assert(input != NULL);
  assert(input_size >= 0);
  assert(resampler->delayed_input_size < resampler->num_taps);
  assert(resampler->phase < resampler->factor_denominator);

  /* If input_size is too big, drop some samples from the beginning. Drops are
   * of course always bad, no matter how they are handled. The user should set
   * `max_input_size` large enough at construction to avoid drops.
   */
  const int excess_input = input_size - resampler->max_input_size;
  if (excess_input > 0) {
    /* Reset the resampler so that state before the drop does not influence
     * output produced after the drop.
     */
    RationalFactorResamplerReset(resampler);
    input += excess_input;
    input_size -= excess_input;
  }

  float* delayed_input = resampler->delayed_input;
  const int num_taps = resampler->num_taps;

  if (resampler->delayed_input_size + input_size < num_taps) {
    if (input_size > 0) {
      /* Append input to delayed_input. */
      memcpy(delayed_input + resampler->delayed_input_size,
             input, sizeof(float) * input_size);
      resampler->delayed_input_size += input_size;
    }
    return 0; /* Not enough samples available to produce any output yet. */
  }

  float* output = resampler->output;
  const int output_size =
      RationalFactorResamplerNextOutputSize(resampler, input_size);

  const float* filters = resampler->filters;
  const int factor_denominator = resampler->factor_denominator;
  const int factor_floor = resampler->factor_floor;
  const int phase_step = resampler->phase_step;
  int phase = resampler->phase;

  /* Below, the position in the input is (i + phase / factor_denominator) in
   * units of input samples, with `phase` tracking the fractional part.
   */
  int i = 0;
  /* `i` is the start index for applying the filters. To stay within the
   * available `delayed_input_size + input_size` input samples, we need
   *
   *   i + num_taps - 1 < delayed_input_size + input_size,
   *
   * or equivalently i < i_end = delayed_input_size + input_size - num_taps + 1.
   */
  int i_end = resampler->delayed_input_size + input_size - num_taps + 1;
  int output_samples = 0;

  /* Process samples where the filter straddles delayed_input and input. */
  while (i < resampler->delayed_input_size && i < i_end) {
    assert(output_samples < output_size);
    const int num_state = resampler->delayed_input_size - i;
    const int num_input = num_taps - num_state;
    const float* filter = filters + phase * num_taps;
    float sum = 0.0f;
    int k;
    /* Compute the dot product between `filter` and the concatenation of
     * `delayed_input[i:]` and `input[:num_input]`.
     */
    for (k = 0; k < num_state; ++k) {
      sum += filter[k] * delayed_input[i + k];
    }
    for (k = 0; k < num_input; ++k) {
      sum += filter[num_state + k] * input[k];
    }

    output[output_samples] = sum;
    ++output_samples;
    i += factor_floor;
    phase += phase_step;
    if (phase >= factor_denominator) {
      phase -= factor_denominator;
      ++i;
    }
  }

  if (i < resampler->delayed_input_size) {
    /* Ran out of input samples before consuming everything in
     * delayed_input_. Discard the samples of delayed_input_ that have been
     * consumed and append the input.
     */
    assert(output_samples == output_size);
    int remaining = resampler->delayed_input_size - i;
    memmove(delayed_input, delayed_input + i, sizeof(float) * remaining);
    memcpy(delayed_input + remaining, input, sizeof(float) * input_size);
    resampler->delayed_input_size += input_size - i;
    assert(resampler->delayed_input_size < resampler->num_taps);
    resampler->phase = phase;
    return output_size;
  }

  /* Consumed everything in delayed_input_. Now process output samples that
   * depend on only the input.
   */
  i -= resampler->delayed_input_size;
  i_end -= resampler->delayed_input_size;
  while (i < i_end) {
    assert(output_samples < output_size);
    const float* filter = filters + phase * num_taps;
    float sum = 0.0f;
    int k;
    for (k = 0; k < num_taps; ++k) {
      sum += filter[k] * input[i + k];
    }
    output[output_samples] = sum;
    ++output_samples;
    i += factor_floor;
    phase += phase_step;
    if (phase >= factor_denominator) {
      phase -= factor_denominator;
      ++i;
    }
  }

  assert(output_samples == output_size);
  assert(i <= input_size);
  resampler->delayed_input_size = input_size - i;
  memcpy(delayed_input, input + i,
      sizeof(float) * resampler->delayed_input_size);
  resampler->phase = phase;
  return output_size;
}
