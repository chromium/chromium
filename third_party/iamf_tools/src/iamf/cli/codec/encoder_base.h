/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_CODEC_ENCODER_BASE_H_
#define CLI_CODEC_ENCODER_BASE_H_

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/substream_channel_count.h"

namespace iamf_tools {

class EncoderBase {
 public:
  /*!\brief Constructor.
   *
   * After constructing `Initialize()` MUST be called and return successfully
   * before using most functionality of the encoder.
   *
   * - Call `EncodeAudioFrame()` to encode an audio frame. The encoding may
   *   happen asynchronously.
   * - Call `FramesAvailable()` to see if there is any finished frame.
   * - Call `Pop()` to retrieve finished frames one at a time, in the order
   *   they were received by `EncodeAudioFrame()`.
   * - Call `Finalize()` to close the encoder, telling it to finish encoding
   *   any remaining frames, which can be retrieved via subsequent `Pop()`s.
   *   After calling `Finalize()`, any subsequent call to `EncodeAudioFrame()`
   *   will fail.
   *
   * \param codec_config Codec Config OBU for the encoder.
   * \param channel_count Number of substream channels for the encoder.
   */
  EncoderBase(const CodecConfigObu& codec_config,
              SubstreamChannelCount channel_count)
      : num_samples_per_frame_(codec_config.GetNumSamplesPerFrame()),
        input_sample_rate_(codec_config.GetInputSampleRate()),
        output_sample_rate_(codec_config.GetOutputSampleRate()),
        input_pcm_bit_depth_(codec_config.GetBitDepthToMeasureLoudness()),
        channel_count_(channel_count) {}

  /*!\brief Destructor. */
  virtual ~EncoderBase() = 0;

  /*!\brief Initializes `EncoderBase`.
   *
   * \param validate_codec_delay If true, validates the Codec Config OBU fields
   *        related to codec delay agree with the encoder.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize(bool validate_codec_delay);

  /*!\brief Encodes an audio frame.
   *
   * \param samples Samples arranged in (channel, time) axes. The samples are
   *        left-justified and stored in the upper `input_pcm_bit_depth_` bits.
   * \param partial_audio_frame_with_data Unique pointer to take ownership of.
   *        The underlying `audio_frame_` is modified. All other fields are
   *        blindly passed along.
   * \return `absl::OkStatus()` on success. Success does not necessarily mean
   *         the frame was finished. A specific status on failure.
   */
  virtual absl::Status EncodeAudioFrame(
      const std::vector<std::vector<int32_t>>& samples,
      std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) = 0;

  /*!\brief Gets whether there are frames available.
   *
   * Available frames can be retrieved by `Pop()`.
   *
   * \return True if there is any finished audio frame.
   */
  bool FramesAvailable() const {
    absl::MutexLock lock(mutex_);
    return !finalized_audio_frames_.empty();
  }

  /*!\brief Pop the first finished audio frame (if any).
   *
   * \param audio_frames List to append the first finished frame to.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Pop(std::list<AudioFrameWithData>& audio_frames) {
    absl::MutexLock lock(mutex_);
    if (!finalized_audio_frames_.empty()) {
      audio_frames.splice(audio_frames.end(), finalized_audio_frames_,
                          finalized_audio_frames_.begin());
    }
    return absl::OkStatus();
  }

  /*!\brief Finalizes the encoder, signaling it to finish any remaining frames.
   *
   * This function MUST be called at most once before popping the last batch
   * of encoded audio frames.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status Finalize() {
    absl::MutexLock lock(mutex_);
    finished_ = true;
    return absl::OkStatus();
  }

  /*!\brief Gets whether the encoder has been closed.
   *
   * \return True if the encoder has been closed.
   */
  bool Finished() const {
    absl::MutexLock lock(mutex_);
    return finished_ && finalized_audio_frames_.empty();
  }

  /*!\brief Gets the required number of samples to delay at the start.
   *
   * Sometimes this is called "pre-skip". This represents the number of initial
   * "junk" samples output from the encoder. In IAMF this represents the
   * recommended amount of samples to trim at the start of a substream.
   *
   * \return Number of samples to delay at the start of the substream.
   */
  uint32_t GetNumberOfSamplesToDelayAtStart() const {
    return required_samples_to_delay_at_start_;
  }

  const uint32_t num_samples_per_frame_;
  const uint32_t input_sample_rate_;
  const uint32_t output_sample_rate_;
  const uint8_t input_pcm_bit_depth_;
  const SubstreamChannelCount channel_count_;

 protected:
  /*!\brief Initializes the child class.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status InitializeEncoder() = 0;

  /*!\brief Initializes `required_samples_to_delay_at_start_`.
   *
   * \param validate_codec_delay If true, validates the Codec Config OBU fields
   *        related to codec delay agree with the encoder.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status SetNumberOfSamplesToDelayAtStart(
      bool /*validate_codec_delay*/) {
    required_samples_to_delay_at_start_ = 0;
    return absl::OkStatus();
  }

  /*!\brief Validates `Finalize()` has not yet been called.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status ValidateNotFinalized() {
    if (Finished()) {
      return absl::InvalidArgumentError(
          "Encoding is disallowed after `Finalize()` has been called");
    }
    return absl::OkStatus();
  }

  /*!\brief Validates `samples` has the correct number of channels and ticks.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status ValidateInputSamples(
      const std::vector<std::vector<int32_t>>& samples) const;

  uint32_t required_samples_to_delay_at_start_ = 0;

  // Mutex to guard simultaneous access to data members.
  mutable absl::Mutex mutex_;

  std::list<AudioFrameWithData> finalized_audio_frames_
      ABSL_GUARDED_BY(mutex_) = {};

  // Whether the encoding has been closed.
  bool finished_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_ENCODER_BASE_H_
