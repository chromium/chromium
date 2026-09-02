/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef INCLUDE_IAMF_TOOLS_IAMF_DECODER_INTERFACE_H_
#define INCLUDE_IAMF_TOOLS_IAMF_DECODER_INTERFACE_H_

#include <cstddef>
#include <cstdint>

#include "iamf_tools_api_types.h"

namespace iamf_tools {
namespace api {

/*!\brief The class and entrypoint for decoding IAMF bitstreams.
 *
 * The functions below constitute our IAMF Iterative Decoder API. Below is a
 * sample usage of the API.
 *
 * Thread safety: a single decoder instance is not thread-safe. All calls on a
 * given instance (Decode, GetOutputTemporalUnit, Reset, etc.) must be made from
 * one thread at a time; they mutate shared internal state without locking.
 * Distinct decoder instances are independent and may be used on separate
 * threads concurrently.
 *
 * Example Reconfigurable Standalone IAMF Usage
 * using iamf_tools::api::IamfDecoderFactory;
 * using iamf_tools::api::IamfDecoderInterface;
 *
 * IamfDecoderFactory::Settings settings = {
 *   // Decoder can be configured here, for example, to request stereo output.
 *   .requested_mix = {
 *       .output_layout = OutputLayout::kItu2051_SoundSystemA_0_2_0
 *   },
 * };
 * std::unique_ptr<IamfDecoderInterface> decoder =
 *     IamfDecoderFactory::Create(settings);
 * for chunk of data in iamf stream:
 *    auto status = decoder->Decode(chunk, chunk_size)
 *    if (!status.ok() {
 *      // Calls returning a status can fail. Check the status to detect errors.
 *    }
 *    if (IsDescriptorProcessingComplete()) {
 *      // Can call various getters here to get info about the output.
 *      status = decoder->GetOutputLayout(selected_mix);
 *      status = decoder->GetNumberOfOutputChannels(output_num_channels);
 *      output_sample_type = decoder->GetOutputSampleType();
 *      status = decoder->GetSampleRate(output_sample_rate);
 *      status = decoder->GetFrameSize(output_frame_size);
 *    }
 * if (end_of_stream) {
 *    decoder->SignalEndOfDecoding();
 *    // Get remaining audio
 *    while (decoder->IsTemporalUnitAvailable()) {
 *      decoder->GetOutputTemporalUnit(output_buffer, bytes_written)
 *      // Do something with the decoded audio like
 *      MyPlaybackFunction(output_buffer, bytes_written);
 *    }
 * }
 */
class IamfDecoderInterface {
 public:
  virtual ~IamfDecoderInterface() = default;

  /*!\brief Decodes the bitstream provided.
   *
   * Supports both descriptor OBUs, temporal units, and partial versions of
   * both. User can provide as much data as they would like. To receive decoded
   * temporal units, GetOutputTemporalUnit() should be called. If
   * GetOutputTemporalUnit() has not been called, this function guarantees that
   * any temporal units received thus far have not been lost. If descriptors are
   * processed for the first time, function will exit before processing any
   * temporal units. This provides the user a chance to configure the decoder as
   * they see fit. See sample usages for more details.
   *
   * \param input_buffer Bitstream to decode.
   * \param input_buffer_size Size in bytes of the input buffer.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  virtual IamfStatus Decode(const uint8_t* input_buffer,
                            size_t input_buffer_size) = 0;

  /*!\brief Outputs the next temporal unit of decoded audio.
   *
   * If no decoded data is available, bytes_written will be 0. The user can
   * continue calling until bytes_written is 0, as there may be more than one
   * temporal unit available. At this point, the user should call Decode() again
   * with more data.
   *
   * The output PCM is arranged based on the configured `OutputLayout` and
   * `OutputSampleType`.
   *
   * \param output_buffer Output buffer to receive bytes.  Must be large enough
   *        to receive bytes.  Maximum necessary size can be determined by
   *        GetFrameSize * GetNumberOfOutputChannels * bit depth (as determined
   *        by GetOutputSampleType).
   * \param output_buffer_size Available size in bytes of the output buffer.
   * \param bytes_written Output param for the number of bytes written to the
   *        output_bytes.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  virtual IamfStatus GetOutputTemporalUnit(uint8_t* output_buffer,
                                           size_t output_buffer_size,
                                           size_t& bytes_written) = 0;

  /*!\brief Returns true iff a decoded temporal unit is available.
   *
   * This function can be used to determine when the user should call
   * GetOutputTemporalUnit().
   *
   * \return true iff a decoded temporal unit is available.
   */
  virtual bool IsTemporalUnitAvailable() const = 0;

  /*!\brief Returns true iff the descriptor OBUs have been parsed.
   *
   * This function can be used for determining when configuration setters that
   * rely on Descriptor OBU parsing can be called.
   *
   * \return true iff the Descriptor OBUs have been parsed.
   */
  virtual bool IsDescriptorProcessingComplete() const = 0;

  /*!\brief Gets the number of output channels.
   *
   * N.B.: This function can only be used after all Descriptor OBUs have been
   * parsed, i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \param output_num_channels Output param for the number of output channels
   *        upon success.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  virtual IamfStatus GetNumberOfOutputChannels(
      int& output_num_channels) const = 0;

  /*!\brief Gets the output mix that will be used to render the audio.
   *
   * The actual Layout used for rendering may not be the same as requested when
   * creating the IamfDecoder, if the requested ID was invalid or the Layout
   * could not be used. This function allows verifying the actual Layout used
   * after Descriptor OBU parsing is complete.
   *
   * N.B.: This function can only be used after all Descriptor OBUs have been
   * parsed, i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \param output_selected_mix Output param for the mix upon success.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  virtual IamfStatus GetOutputMix(SelectedMix& output_selected_mix) const = 0;

  /*!\brief Returns the current OutputSampleType.
   *
   * The value is either the values specified in the Settings or a default
   * which may vary based on content.
   *
   * N.B.: This function can only be used after all Descriptor OBUs have been
   * parsed, i.e. IsDescriptorProcessingComplete() returns true.
   */
  virtual OutputSampleType GetOutputSampleType() const = 0;

  /*!\brief Gets the sample rate of the output audio.
   *
   * The value is from the content of the IAMF bitstream.
   *
   * N.B.: This function can only be used after all Descriptor OBUs have been
   * parsed, i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \param output_sample_rate Output param for the sample rate upon success.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  virtual IamfStatus GetSampleRate(uint32_t& output_sample_rate) const = 0;

  /*!\brief Gets the number of samples per frame.
   *
   *
   * Returns the number of samples per frame per channel of the output audio.
   * The total number of samples in a frame is the number of channels times
   * this number, the frame size.
   *
   * N.B.: This function can only be used after all Descriptor OBUs have been
   * parsed, i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \param output_frame_size Output param for the frame size upon success.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  virtual IamfStatus GetFrameSize(uint32_t& output_frame_size) const = 0;

  /*!\brief Resets the decoder to a clean state ready to decode new data.
   *
   * A clean state refers to a state in which descriptors OBUs have been parsed,
   * but no other data has been parsed.
   *
   * Useful for seeking applications.
   *
   * This function can only be used if the decoder was created with
   * IamfDecoderFactory::CreateFromDescriptors().
   *
   * This function will result in all decoded temporal units that have not been
   * retrieved by GetOutputTemporalUnit() to be lost. It will also result in any
   * pending data in the internal buffer being lost.
   *
   * return Ok status upon success. Other specific statuses on failure.
   */
  virtual IamfStatus Reset() = 0;

  /*!\brief Resets the decoder with a new layout and a clean state.
   *
   * A clean state refers to a state in which descriptors OBUs have been parsed,
   * but no other data has been parsed.
   *
   * Useful for dynamic playback layout changes.
   *
   * This function can only be used if the decoder was created with
   * IamfDecoderFactory::CreateFromDescriptors().
   *
   * This function will result in all decoded temporal units that have not been
   * retrieved by GetOutputTemporalUnit() to be lost. It will also result in any
   * pending data in the internal buffer being lost.
   *
   * return Ok status upon success. Other specific statuses on failure.
   */
  virtual IamfStatus ResetWithNewMix(const RequestedMix& requested_mix,
                                     SelectedMix& selected_mix) = 0;

  /*!\brief Signals to the decoder that no more data will be provided.
   *
   * Decode cannot be called after this method has been called, unless Reset()
   * is called first.
   *
   * User should call GetOutputTemporalUnit() until it returns no bytes after
   * calling this function to get any remaining output.
   *
   * \return Ok status upon success. Other specific statuses on failure.
   */
  virtual IamfStatus SignalEndOfDecoding() = 0;
};
}  // namespace api
}  // namespace iamf_tools

#endif  // INCLUDE_IAMF_TOOLS_IAMF_DECODER_INTERFACE_H_
