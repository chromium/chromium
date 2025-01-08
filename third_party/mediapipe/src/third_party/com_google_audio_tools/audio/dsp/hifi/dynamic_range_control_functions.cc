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

#include "audio/dsp/hifi/dynamic_range_control_functions.h"

namespace audio_dsp {
namespace internal {

ParamError VerifyParams(const TwoWayCompressionParams& params) {
  DynamicRangeControlNonlinearityParams expander_params =
      params.expander_region;
  DynamicRangeControlNonlinearityParams upwards_compressor_params =
      params.upwards_compressor_region;
  DynamicRangeControlNonlinearityParams soft_compressor_params =
      params.soft_compressor_region;
  DynamicRangeControlNonlinearityParams hard_compressor_params =
      params.hard_compressor_region;

  // Verify that the thresholds are in ascending order for the expander, upwards
  // compressor, soft compressor, and hard compressor, respectively.
  if (expander_params.threshold_db > upwards_compressor_params.threshold_db) {
    LOG(ERROR)
        << "The threshold of the expander must be less than or equal to the\n"
           "threshold of the upwards compressor.";
    return kThresholdError;
  } else if (upwards_compressor_params.threshold_db >
             soft_compressor_params.threshold_db) {
    LOG(ERROR)
        << "The threshold of the upwards compressor must be less than or\n"
           "equal to the threshold of the soft compressor.";
    return kThresholdError;
  } else if (soft_compressor_params.threshold_db >
             hard_compressor_params.threshold_db) {
    LOG(ERROR)
        << "The threshold of the soft compressor must be less than or equal\n"
           "to the threshold of the hard compressor.";
    return kThresholdError;
  } else if (expander_params.ratio < 1.0f) {
    LOG(ERROR)
        << "The ratio of the expander must be greater than or equal to 1.";
    return kRatioError;
  } else if (upwards_compressor_params.ratio < 1.0f) {
    LOG(ERROR) << "The ratio of the upwards compressor must be greater than or "
                  "equal to "
                  "1.";
    return kRatioError;
  } else if (soft_compressor_params.ratio > hard_compressor_params.ratio) {
    LOG(ERROR)
        << "The ratio of the soft compressor must be less than or equal to\n"
           "the ratio of the hard compressor";
    return kRatioError;
  } else {
    return kNoParamError;
  }
}

}  // namespace internal
}  // namespace audio_dsp
