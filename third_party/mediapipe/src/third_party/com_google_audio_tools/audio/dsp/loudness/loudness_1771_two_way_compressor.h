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

// This class implements dynamic range compression with the following features:
//
//   1. Two-way compression (upwards compression below a threshold and downwards
//   compression above a threshold) is used to compress the dynamic range about
//   a given center loudness. Please see the documentation in
//   audio/dsp/hifi/dynamic_range_control_functions.h for a more detailed
//   explantation of a two-way compressor.
//
//   2. Signal loudness is measured using the ITU-R BS.1771-1 recommendation
//   for metering audio, which uses a frequency-weighed streaming style RMS
//   measurement. See
//   https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1771-1-201201-I!!PDF-E.pdf
//   for more information about this algorithm.
//
// For more conventional dynamics compressors, please refer to the dynamic
// range control library found in audio/dsp/hifi/dynamic_range_control, which
// provides a similar interface, but is more suitable to general-purpose
// compression. Additionally, due to the method of level estimation used, this
// library is not suited for use as a brick-wall limiter or noise gate.
//
// This class is vectorized using Eigen for fast computation and
// supports processing multichannel inputs.
//
// Usage:
//
// Getting set up.
//
//    /* Set up Loudness1771TwoWayCompressor */
//    Loudness1771TwoWayCompressorParams params;
//    // Configure the parameters as needed...
//
//    Loudness1771TwoWayCompressor compressor;
//    compressor.Init(params, num_channels, block_size_samples, sample_rate_hz)
//
// Please refer to the documentation in audio/dsp/hifi/dynamic_range_control.h
// for usage.
//
// NOTE: ComputeGainSignalOnly and ProcessBlock both alter the state of
// the class. Therefore, only one of these methods should be called for an
// instance of the class.

#ifndef AUDIO_DSP_LOUDNESS_LOUDNESS_1771_TWO_WAY_COMPRESSOR_H_
#define AUDIO_DSP_LOUDNESS_LOUDNESS_1771_TWO_WAY_COMPRESSOR_H_

#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>

#include "audio/dsp/attack_release_envelope.h"
#include "audio/dsp/decibels.h"
#include "audio/dsp/fixed_delay_line.h"
#include "audio/dsp/hifi/dynamic_range_control_functions.h"
#include "audio/dsp/loudness/streaming_loudness_1771.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "third_party/eigen3/Eigen/Core"

namespace audio_dsp {

struct Loudness1771TwoWayCompressorParams {
  // Put everything in a defined state.
  Loudness1771TwoWayCompressorParams()
      : input_gain_db(0.0f),
        output_gain_db(0.0f),
        attack_s(0.001f),
        release_s(0.05f),
        lookahead_s(0.0f) {
    // See note below.
    streaming_loudness_params =
        StreamingLoudnessParams::Stereo1771Params(0.4f, 48000.0f);
  }

  // Applied before the nonlinearity.
  float input_gain_db;

  // Applied at the output to make up for lost signal energy.
  float output_gain_db;

  // Parameters to configure the loudness measurement filter. Please see the
  // documentation in audio/dsp/loudness/streaming_loudness_1771.h for an
  // explanation of how to configure the filter.
  // NOTE: by default, these parameters are configured for a stereo audio
  // signal. These must be reconfigured for use with an input signals of a
  // different channel count or the compressor will fail.
  StreamingLoudnessParams streaming_loudness_params;

  // Parameters to configure a two-way compression nonlinearity.
  // Loudness1771TwoWayCompressor computes the gain changes needed for the
  // of several compression nonlinearities at once and then applies the combined
  // gains as opposed to DynamicRangeControl, which computes and applies only
  // one nonlinearity at a time. Please see the documentation in
  // audio/dsp/hifi/dynamic_range_control_functions.h for requirements and
  // suggestions for configuring these parameters.
  TwoWayCompressionParams two_way_compression_params;

  // Exponential time constants associated with the gain estimator. Depending on
  // the application, these may range form 1ms to over 1s.
  //
  // We use the definition of time constant typical in signal processing: a
  // filter's step response reaches (1 - 1/e) of its final value after one time
  // constant. Some commercial DRCs will define their time constants such that
  // the filter is within some small tolerance of its final value after one time
  // constant (meaning that to get similar results, time constants for this DRC
  // should be set about a factor of 4 smaller).
  //
  // It should also be noted that the loudness estimation scheme uses its own
  // exponential smoothing filter to perform RMS on the signal, which may
  // further cause differences between the tuning values of this compressor and
  // a commercial DRC.
  float attack_s;
  float release_s;

  // Delay the incoming audio to make the gain control more predictive. This
  // advances the compression action relative to the signal.
  // This parameter must not change after initialization.
  float lookahead_s;
};

// Apply two-way dynamic range compression to an audio signal using level
// estimation from the ITU-1771 specification for measuring loudness.
class Loudness1771TwoWayCompressor {
 public:
  Loudness1771TwoWayCompressor()
      : num_channels_(0) {}

  // sample_rate_hz is the audio sample rate.
  // max_block_size_samples is the largest number of samples per channel that
  // may be processed at a time. The size of the block does not have any
  // significant effect on the output audio.
  // Note: Init may only be called once in the lifetime of the class.
  bool Init(const Loudness1771TwoWayCompressorParams& params, int num_channels,
            int max_block_size_samples, float sample_rate_hz);

  void Reset();

  // InputType and OutputType are Eigen 2D blocks (ArrayXXf or MatrixXf) or a
  // similar mapped type containing audio samples in column-major format.
  // The number of rows must be equal to num_channels, and the number of cols
  // must be less than or equal to max_block_size_samples.
  //
  // Templating is necessary for supporting Eigen::Map output types.
  template <typename InputType, typename OutputType>
  bool ProcessBlock(const InputType& input, OutputType* output) {
    if (!std::is_same<typename InputType::Scalar, float>::value){
      LOG(ERROR) << "Array input type must be float.";
      return false;
    }
    if (!std::is_same<typename OutputType::Scalar, float>::value) {
      LOG(ERROR) << "Array output type must be float.";
      return false;
    }
    if (input.cols() < 1 || input.cols() > max_block_size_samples_) {
      LOG(ERROR) << "Number of columns (frames) in input must be between 1 "
                 << "and " << max_block_size_samples_
                 <<" but received " << input.cols() << ".";
      return false;
    }
    if (input.rows() != num_channels_) {
      LOG(ERROR) << "Number of rows (channels) in input must be equal to "
                 << num_channels_ << " but received " << input.rows() << ".";
      return false;
    }
    if (input.cols() != output->cols() || input.rows() != output->rows()) {
      LOG(ERROR) << "Size of the input and output arrays must be the same.";
      return false;
    }

    // Map the needed amount of space for computations.
    VectorType workmap = workspace_.head(input.cols());

    ComputeGainSignalOnly(input, &workmap);
    if (!ApplyGainToSignal(input, workmap, output)) {
      return false;
    }

    return true;
  }

  // Convenience wrapper around ProcessBlock, where the signal in input is
  // assumed to be interleaved multi-channel audio samples where the number of
  // channels is specified by the call to Init.
  // See ProcessBlock for details.
  absl::StatusOr<std::vector<float>> ProcessVector(
      const std::vector<float>& input) {
    if (num_channels_ == 0) {
      return absl::UnavailableError(
          "Instance must be initialized by calling Init().");
    }
    const size_t num_channels = num_channels_;
    const size_t num_frames = input.size() / num_channels;

    Eigen::Map<const Eigen::ArrayXXf> input_block(
        input.data(), num_channels, num_frames);

    std::vector<float> output_block(num_channels * num_frames);
    Eigen::Map<Eigen::ArrayXXf> output_map(output_block.data(),
                                           num_channels, num_frames);
    if (ProcessBlock(input_block, &output_map)) {
      return output_block;
    }
    return absl::InternalError("ProcessBlock failed.  Check logs.");
  }

  // Compute the gains that would be applied to the signal so that more
  // processing can be done prior to applying them. The output gains are linear.
  // Gain is a monaural signal, with length input.cols(). Note that if GainType
  // is not resizable, it will be presized to the right size.
  template <typename InputType, typename GainType>
  void ComputeGainSignalOnly(const InputType& input, GainType* gain) {
    gain->resize(input.cols(), 1);
    // Compute the ITU-1771 loudness. This loudness is a monaural signal. The
    // loudness measurements are in decibels, so the measurement must be
    // converted back to average power before attack/release smoothing.
    Eigen::ArrayXf loudness(gain->size());
    loudness_meter_.ProcessBlock(input, &loudness);
    ComputeGainFromDetectedSignal(loudness, gain);
  }

  // Convenience wrapper around ComputeGainSignalOnly, where the signal in input
  // is assumed to be interleaved multi-channel audio samples where the number
  // of channels is specified by the call to Init.
  // See ComputeGainSignalOnly for details.
  absl::StatusOr<std::vector<float>> ComputeGainSignalOnlyVector(
      const std::vector<float>& input) {
    if (num_channels_ == 0) {
      return absl::UnavailableError(
          "Instance must be initialized by calling Init().");
    }
    const size_t num_channels = num_channels_;
    const size_t num_frames = input.size() / num_channels;
    if (num_frames > max_block_size_samples_) {
      return absl::InvalidArgumentError("Input signal is too large.");
    }

    Eigen::Map<const Eigen::ArrayXXf> input_block(
        input.data(), num_channels, num_frames);

    Eigen::ArrayXf gain(num_frames);
    VectorType v = gain.head(num_frames);
    ComputeGainSignalOnly(input_block, &v);
    std::vector<float> output(gain.begin(), gain.end());
    return output;
  }

 private:
  using VectorType = Eigen::VectorBlock<Eigen::ArrayXf, Eigen::Dynamic>;

  // Apply a gain signal uniformly to an input signal.
  // gain is a monoaural signal of linear gain factors. It has length
  // input.cols().
  // In-place processing is supported (&input = output).
  template <typename GainType, typename InputType, typename OutputType>
  bool ApplyGainToSignal(const InputType& input, const GainType& gain,
                         OutputType* output) {
    if (input.cols() != gain.size()) {
      LOG(ERROR) << "The number of gain words does not match the number of "
                    "samples per channel of the input.";
      return false;
    }

    // Scale the input sample-wise by the gains.
    Eigen::Map<const Eigen::ArrayXXf> delayed =
        lookahead_delay_.ProcessBlock(input);
    *output = delayed.rowwise() * gain.transpose();
    return true;
  }

  // Compute the linear gain to apply to the input. data_ptr is a rectified
  // signal envelope on input, and after calling this function it contains the
  // linear gain to apply to the signal.
  void ComputeGainFromDetectedSignal(const Eigen::ArrayXf& loudness,
                                     VectorType* gain_ptr);

  void ApplyAttackReleaseInLinearDomain(VectorType* data_ptr);
  void ComputeGainInDecibelDomain(VectorType* data_ptr);

  int num_channels_;
  float sample_rate_hz_;
  int max_block_size_samples_;

  Eigen::ArrayXf workspace_;
  Eigen::ArrayXf workspace_drc_output_;

  Loudness1771TwoWayCompressorParams params_;

  FixedDelayLine lookahead_delay_;

  StreamingLoudness1771 loudness_meter_;
  std::unique_ptr<AttackReleaseEnvelope> envelope_;
};

}  // namespace audio_dsp

#endif  // AUDIO_DSP_LOUDNESS_LOUDNESS_1771_TWO_WAY_COMPRESSOR_H_
