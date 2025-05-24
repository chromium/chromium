#include "audio/dsp/portable/auto_gain_control.h"

#include <math.h>
#include <stdlib.h>

#include "audio/dsp/portable/fast_fun.h"

int AutoGainControlInit(AutoGainControlState* state,
                        float sample_rate_hz,
                        float time_constant_s,
                        float agc_strength,
                        float power_floor) {
  if (state == NULL ||
      !(sample_rate_hz > 0.0f) ||
      !(time_constant_s > 0.0f) ||
      !(0.0f <= agc_strength && agc_strength <= 1.0f) ||
      !(power_floor > 0.0f)) {
    return 0;
  }

  /* Warm up length is one time constant. */
  state->num_warm_up_samples = (int)(sample_rate_hz * time_constant_s + 0.5f);
  state->smoother_coeff =
      (float)(1 - exp(-1 / (time_constant_s * sample_rate_hz)));
  state->exponent = -0.5f * agc_strength;
  state->power_floor = power_floor;
  AutoGainControlReset(state);
  return 1;
}

void AutoGainControlReset(AutoGainControlState* state) {
  state->smoothed_power = 1.0f;
  state->warm_up_counter = 1;
}

/* This function gets called from AutoGainControlProcessSample if we are still
 * warming up the smoothed power estimate. During warm up, we accumulate the
 * power sum in state->smoothed_power (where x[n] is the nth input sample):
 *
 *   state->smoothed_power = 1 + x[0]^2 + ... + x[n]^2.
 *
 * The initial "1" term adjusts the average toward 1 so that the AGC acts
 * conservatively for small n.
 */
void AutoGainControlWarmUpProcess(AutoGainControlState* state,
                                  float power_sample) {
  ++state->warm_up_counter;
  /* Accumulate power, smoothed_power = 1 + x[0]^2 + ... + x[n]^2. */
  state->smoothed_power += power_sample;

  if (state->warm_up_counter > state->num_warm_up_samples) {
    /* Warm up is done. Divide to get the average power,
     *   smoothed_power = (1 + x[0]^2 + ... + x[n]^2) / (1 + n).
     */
    state->smoothed_power /= state->warm_up_counter;
  }
}

float AutoGainControlGetGain(const AutoGainControlState* state) {
  /* During warm up, use the average
   *
   *   smoothed_power = (1 + x[0]^2 + ... + x[n]^2) / (1 + n).
   *
   * After warm up, use the exponentially-weighted moving average
   *
   *   smoothed_power = smoother_coeff * sum_k (1 - smoother_coeff)^k x[n-k]^2.
   */
  float smoothed_power = (state->warm_up_counter <= state->num_warm_up_samples)
      ? state->smoothed_power / state->warm_up_counter
      : state->smoothed_power;
  /* Compute the gain = (power_floor + smoothed_power)^(-agc_strength / 2). */
  return FastPow(state->power_floor + smoothed_power, state->exponent);
}

