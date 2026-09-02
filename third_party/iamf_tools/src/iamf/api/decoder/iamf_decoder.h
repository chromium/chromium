/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef API_DECODER_IAMF_DECODER_H_
#define API_DECODER_IAMF_DECODER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_set>

#include "iamf/include/iamf_tools/iamf_decoder_interface.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"

namespace iamf_tools {
namespace api {

/*!\brief A wrapper for decoding IAMF.
 *
 * !WARNING! Do not depend on this class directly.
 * Use the IamfDecoderFactory to produce a pointer to an IamfDecoderInterface.
 */
class IamfDecoder : public api::IamfDecoderInterface {
 public:
  /*!\brief Settings for the `IamfDecoder`. */
  struct Settings {
    // Specifies the desired output Mix Presentation ID and/or layout.
    //
    // See `RequestedMix` struct for details on how the contents are
    // used and prioritized.
    //
    // The resulting Mix Presentation ID and layout will be retrievable after
    // Descriptor OBUs have been processed.
    RequestedMix requested_mix;

    // Specify a different ordering for the output samples.  Only specific
    // orderings are available, custom or granular control is not possible.
    ChannelOrdering channel_ordering = ChannelOrdering::kIamfOrdering;

    // Specifies the desired profile versions. Clients should explicitly provide
    // the profiles they are interested in. Otherwise, the default value will
    // evolve in the future, based on recommendations or additions to the IAMF
    // spec.
    //
    // If the descriptor OBUs do not contain a mix presentation which is
    // suitable for one of the matching profiles the decoder will return an
    // error. Typically all profiles the client is capable of handling should
    // be provided, to ensure compatibility with as many mixes as possible.
    std::unordered_set<ProfileVersion> requested_profile_versions = {
        ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile,
        ProfileVersion::kIamfBaseEnhancedProfile};

    // Specifies the desired bit depth for the output samples.
    OutputSampleType requested_output_sample_type =
        OutputSampleType::kInt32LittleEndian;

    // Specifies the trimming settings for decoding audio frames.
    TrimmingSettings trimming_settings;
  };

  // Dtor cannot be inline (so it must be declared and defined in the source
  // file) because this class holds a (unique) pointer to the partial class,
  // DecoderState.  Moves must be declared and defined because dtor is defined.
  ~IamfDecoder();
  IamfDecoder(IamfDecoder&&);
  IamfDecoder& operator=(IamfDecoder&&);

  /*!\brief Creates an IamfDecoder.
   *
   * This function should be used for pure streaming applications in which the
   * descriptor OBUs are not known in advance.
   *
   * \param settings Settings to configure the decoder.
   * \param output_decoder An output param for the decoder upon success.
   * \return Ok status upon success. Other specific statuses on  failure.
   */
  static IamfStatus Create(const IamfDecoder::Settings& settings,
                           std::unique_ptr<IamfDecoder>& output_decoder);

  /*!\brief Creates an IamfDecoder from a known set of descriptor OBUs.
   *
   * This function should be used for applications in which the descriptor OBUs
   * are known in advance. When creating the decoder via this mode, future calls
   * to decode must pass complete temporal units.
   *
   * \param settings Settings to configure the decoder.
   * \param input_buffer Bitstream containing all the descriptor OBUs and
   *        only descriptor OBUs.
   * \param input_buffer_size Size in bytes of the input buffer.
   * \param output_decoder An output param for the decoder upon success.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  static IamfStatus CreateFromDescriptors(
      const IamfDecoder::Settings& settings, const uint8_t* input_buffer,
      size_t input_buffer_size, std::unique_ptr<IamfDecoder>& output_decoder);

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
  IamfStatus Decode(const uint8_t* input_buffer,
                    size_t input_buffer_size) override;

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
  IamfStatus GetOutputTemporalUnit(uint8_t* output_buffer,
                                   size_t output_buffer_size,
                                   size_t& bytes_written) override;

  /*!\brief Returns true iff a decoded temporal unit is available.
   *
   * This function can be used to determine when the user should call
   * GetOutputTemporalUnit().
   *
   * \return true iff a decoded temporal unit is available.
   */
  bool IsTemporalUnitAvailable() const override;

  /*!\brief Returns true iff the descriptor OBUs have been parsed.
   *
   * This function can be used for determining when configuration setters that
   * rely on Descriptor OBU parsing can be called.
   *
   * \return true iff the Descriptor OBUs have been parsed.
   */
  bool IsDescriptorProcessingComplete() const override;

  /*!\brief Gets the layout that will be used to render the audio.
   *
   * The actual Layout used for rendering may not the same as requested when
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
  IamfStatus GetOutputMix(SelectedMix& output_selected_mix) const override;

  /*!\brief Gets the number of output channels.
   *
   * This function can only be used after all Descriptor OBUs have been parsed,
   * i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \param output_num_channels Output param for the number of output channels
   *        upon success.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  IamfStatus GetNumberOfOutputChannels(int& output_num_channels) const override;

  /*!\brief Returns the current OutputSampleType.
   *
   * The value is either the value set from `IamfDecoder::Settings` or a default
   * which may vary based on content.
   *
   * This function can only be used after all Descriptor OBUs have been parsed,
   * i.e. IsDescriptorProcessingComplete() returns true.
   */
  OutputSampleType GetOutputSampleType() const override;

  /*!\brief Gets the sample rate.
   *
   * This function can only be used after all Descriptor OBUs have been parsed,
   * i.e. IsDescriptorProcessingComplete() returns true.
   *
   * \param output_sample_rate Output param for the sample rate upon success.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  IamfStatus GetSampleRate(uint32_t& output_sample_rate) const override;

  /*!\brief Gets the number of samples per frame.
   *
   * This function can only be used after all Descriptor OBUs have been parsed,
   * i.e. IsDescriptorProcessingComplete() returns true.
   *
   * Returns the number of samples per frame per channel of the output audio.
   * The total number of samples in a frame is the number of channels times
   * this number, the frame size.
   *
   * \param output_frame_size Output param for the frame size upon success.
   * \return Ok status upon success. Other specific statuses on failure.
   */
  IamfStatus GetFrameSize(uint32_t& output_frame_size) const override;

  /*!\brief Resets the decoder to a clean state ready to decode new data.
   *
   * A clean state refers to a state in which descriptors OBUs have been parsed,
   * but no other data has been parsed.
   *
   * This function can only be used if the decoder was created with
   * CreateFromDescriptors().
   *
   * This function will result in all decoded temporal units that have not been
   * retrieved by GetOutputTemporalUnit() to be lost. It will also result in any
   * pending data in the internal buffer being lost.
   *
   * return Ok status upon success. Other specific statuses on failure.
   */
  IamfStatus Reset() override;

  /*!\brief Resets the decoder with a new RequestedMix and a clean state.
   *
   * A clean state refers to a state in which descriptors OBUs have been parsed,
   * but no other data has been parsed.
   *
   * Useful for dynamic playback layout changes or changing Mix Presentation ID.
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
                                     SelectedMix& selected_mix) override;

  /*!\brief Signals to the decoder that no more data will be provided.
   *
   * Decode cannot be called after this method has been called, unless Reset()
   * is called first.
   *
   * \return Ok status upon success. Other specific statuses on failure.
   */
  IamfStatus SignalEndOfDecoding() override;

 private:
  // Forward declaration of the internal state of the decoder.
  struct DecoderState;

  // Private constructor only used by Create functions.
  IamfDecoder(std::unique_ptr<DecoderState> state);

  // Internal state of the decoder.
  std::unique_ptr<DecoderState> state_;
};
}  // namespace api
}  // namespace iamf_tools

#endif  // API_DECODER_IAMF_DECODER_H_
