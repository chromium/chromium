/*
 * Copyright 2020 Google LLC
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

#include "audio/dsp/signal_generator.h"

namespace audio_dsp {

using ::Eigen::ArrayXf;

ArrayXf GenerateSineEigen(int num_samples, float sample_rate, float frequency,
                          float amplitude, float phase_begin) {
  const float kRadiansPerSample = 2 * M_PI * frequency / sample_rate;
  ArrayXf signal =
      (ArrayXf::LinSpaced(num_samples, 0, num_samples - 1) * kRadiansPerSample -
       phase_begin)
          .sin() *
      amplitude;
  return signal;
}

}  // namespace audio_dsp
