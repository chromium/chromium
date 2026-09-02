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
#ifndef CLI_CODEC_FLAC_ENCODER_H_
#define CLI_CODEC_FLAC_ENCODER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/substream_channel_count.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_encoder.h"

namespace iamf_tools {

struct FlacFrame {
  // Partial audio frame with data associated with this FLAC frame. Its
  // `audio_frame_` is built up in the call(s) to `LibFlacWriteCallback`.
  std::unique_ptr<AudioFrameWithData> audio_frame_with_data;

  // Number of samples represented by raw data.
  unsigned int num_samples = 0;
};

/*!\brief Encodes FLAC frames `FlacEncoder` using `libflac`.
 *
 * The `libflac` encoder works asynchronously. `EncodeAudioFrame()` passes data
 * to `libflac` to start encoding a frame. `libflac` calls the callback
 * functions (i.e. `LibFlacWriteCallback` and `LibFlacMetadataCallback`) as the
 * data is processed. The callback functions track the state of the frames in
 * various member variables of this class.
 *
 * Data associated with the frames are stored in `frame_index_to_frame_`
 * until they are fully encoded. Any finished frame will be moved to
 * `EncoderBase::finalized_audio_frames_` and can be moved into the output
 * list provided to `Pop()`.
 *
 * `Finalize()` function closes the encoder. When the `STREAMINFO` metadata
 * block is produced, the last batch of Audio Frame OBUs are encoded and
 * available to be popped.
 */
class FlacEncoder : public EncoderBase {
 public:
  /*!\brief Constructor.
   *
   * \param flac_encoder_metadata Encoder metadata for FLAC.
   * \param codec_config Codec config for FLAC.
   * \param channel_count Number of channels in the substream.
   */
  FlacEncoder(
      const iamf_tools_cli_proto::FlacEncoderMetadata& flac_encoder_metadata,
      const CodecConfigObu& codec_config, SubstreamChannelCount channel_count)
      : EncoderBase(codec_config, channel_count),
        encoder_metadata_(flac_encoder_metadata),
        decoder_config_(std::get<FlacDecoderConfig>(
            codec_config.GetCodecConfig().decoder_config)) {}

  ~FlacEncoder() override;

  /*!\brief Encodes an audio frame.
   *
   * \param samples Samples arranged in (channel, time) axes. The samples are
   *        left-justified and stored in the upper `input_bit_depth` bits.
   * \param partial_audio_frame_with_data Unique pointer to take ownership of.
   *        The underlying `audio_frame_` is modified. All other fields are
   *        blindly passed along.
   * \return `absl::OkStatus()` on success. Success does not necessarily mean
   *         the frame was finished. A specific status on failure.
   */
  absl::Status EncodeAudioFrame(
      const std::vector<std::vector<int32_t>>& samples,
      std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data)
      override;

  /*!\brief Finalizes the encoder.
   *
   * This function MUST be called to ensure all audio frames are popped from
   * the encoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Finalize() override;

 private:
  // `libflac` uses callbacks to signal the frames are done. Let the callback
  // functions be friends so they can update state information in
  // `next_frame_index_`, frame_index_to_frame_`, and `finished_`.
  friend FLAC__StreamEncoderWriteStatus LibFlacWriteCallback(
      const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[],
      size_t bytes, unsigned int samples, unsigned int current_frame,
      void* client_data);
  friend void LibFlacMetadataCallback(const FLAC__StreamEncoder* encoder,
                                      const FLAC__StreamMetadata* metadata,
                                      void* client_data);

  /*!\brief Initializes the underlying encoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status InitializeEncoder() override;

  const iamf_tools_cli_proto::FlacEncoderMetadata encoder_metadata_;
  const FlacDecoderConfig decoder_config_;

  // A pointer to the `libflac` encoder.
  FLAC__StreamEncoder* encoder_ = nullptr;

  // Tracks the next frame index to use. This data is associated with the
  // `current_frame` argument to `flac_write_callback`.
  unsigned int next_frame_index_ = 0;

  // The buffer of any unfinished frames, keyed and sorted by the frame
  // index.
  absl::btree_map<unsigned int, FlacFrame> frame_index_to_frame_
      ABSL_GUARDED_BY(mutex_) = {};
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_FLAC_ENCODER_H_
