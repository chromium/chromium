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
#ifndef INCLUDE_IAMF_TOOLS_IAMF_ENCODER_INTERFACE_H_
#define INCLUDE_IAMF_TOOLS_IAMF_ENCODER_INTERFACE_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf_tools_encoder_api_types.h"

namespace iamf_tools {
namespace api {

/*!\brief Encodes an IAMF bitstream.
 *
 *  // Get an encoder.
 *  std::unique_ptr<IamfEncoderInterface> encoder = ...;
 *
 *  // Reusable buffer, later redundant copies won't change size.
 *  std::vector<uint8_t> descriptor_obus;
 *  // Control flow is adjusted when the output is being "streamed" to a
 *  // consumer, such as via a livestream to many users. Certain implementations
 *  // may automatically pack the OBUs correctly when (such as to a file),
 *  // without following the extra streaming control flow.
 *  if (streaming) {
 *    bool kNoRedundantCopy = false;
 *    RETURN_IF_NOT_OK(
 *        encoder->GetDescriptorObus(kNoRedundantCopy, descriptor_obus));
 *    // Broadcast the "initial" descriptor OBUs, to allow consumers to sync.
 *  }
 *  // If not streaming, it is safe to skip the call. Safe to use anyway.
 *
 *  // Reusable input buffer. Hardcode an LFE and stereo audio element to show
 *  // multi-dimensionality. Hardcode 1024 samples per frame. The same
 *  // backing allocation can be used for each frame.
 *  using ChannelLabel::Label;
 *  const absl::Span<const double> kEmptyFrame;
 * IamfTemporalUnitData temporal_unit_data = {
 *      .parameter_block_id_to_metadata = {},
 *      .audio_element_id_to_data =
 *          {
 *              {0, {{LFE, kEmptyFrame}}},
 *              {1, {{L2, kEmptyFrame}, {R2, kEmptyFrame}}},
 *          },
 *  };
 *  // Reusable buffer. It will grow towards the maximum size of an output
 *  //  temporal unit.
 *  std::vector<uint8_> temporal_unit_obus;
 *
 *  // Repeat descriptors every so often to help clients sync. In practice,
 *  // an API user would determine something based on their use case. For
 *  // example to aim for ~5 seconds of output audio between descriptors.
 *  const int kDescriptorRepeatInterval = 100;
 *  while (encoder->GeneratingTemporalUnits()) {
 *    if (streaming && iteration_count % kDescriptorRepeatInterval == 0) {
 *      bool kRedundantCopy = true;
 *      // Broadcast the redundant descriptor OBUs.
 *      RETURN_IF_NOT_OK(
 *          encoder->GetDescriptorObus(kRedundantCopy, descriptor_obus));
 *    }
 *
 *    // Fill `temporal_unit_data` for this frame.
 *    for (each audio element) {
 *      for (each channel label from the current element) {
 *        // Fill the slot in `temporal_unit_data` for this audio element and
 *        // channel label.
 *      }
 *    }
 *    // Fill any parameter blocks.
 *    for (each parameter block metadata) {
 *      // Fill the slot in `temporal_unit_data` for this parameter block.
 *    }
 *
 *    RETURN_IF_NOT_OK(encoder->Encode(temporal_unit_data));
 *
 *    if (done_receiving_all_audio) {
 *      encoder->FinalizeEncode();
 *    }
 *
 *    // Flush OBUs for the next temporal unit.
 *    encoder->OutputTemporalUnit(temporal_unit_obus);
 *    if (streaming) {
 *      // Broadcast the temporal unit descriptor OBUs.
 *    }
 *    // Otherwise, they are automatically flushed to the file.
 *  }
 *  // Data generation is done. Perform some cleanup.
 *  if (streaming) {
 *    bool kNoRedundantCopy = false;
 *    RETURN_IF_NOT_OK(
 *        encoder->GetDescriptorObus(kNoRedundantCopy, descriptor_obus));
 *    // If any consumers require accurate descriptors (loudness), notify them.
 *  }
 *  // Otherwise, they were already flushed to file.
 */
class IamfEncoderInterface {
 public:
  virtual ~IamfEncoderInterface() = default;

  /*!\brief Gets the latest descriptor OBUs.
   *
   * When `GeneratingTemporalUnits` returns true, these represent preliminary
   * descriptor OBUs. After `GeneratingTemporalUnits` returns false, these
   * represent the finalized OBUs.
   *
   * When streaming IAMF, it is important to regularly provide
   * "redundant copies" which help downstream clients sync. The exact
   * cadence is not mandated and depends on use case.
   *
   * Mix Presentation OBUs contain loudness information, which is only
   * possible to know after all data OBUs are generated. Other OBUs with
   * metadata may also be updated (e.g. fields representing the number of
   * samples). Typically, after encoding is finished, a final call to get
   * non-redundant OBUs with accurate loudness information is encouraged.
   * Auxiliary fields in other descriptor OBUs may also change.
   *
   * \param redundant_copy True to request a "redundant" copy.
   * \param descriptor_obus Finalized OBUs.
   * \param output_obus_are_finalized `true` when the output OBUs are
   *        finalized. `false` otherwise.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  virtual absl::Status GetDescriptorObus(
      bool redundant_copy, std::vector<uint8_t>& descriptor_obus,
      bool& output_obus_are_finalized) const = 0;

  /*!\brief Gets the encoder delay in samples.
   *
   * Certain settings or codecs have an inherent delay in the encoding process.
   * E.g. pre-skip in Opus. Typically the output Audio Frame OBUs will signal
   * this delay by signalling `samples_to_trim_at_start`. This delay may be
   * useful to help synchronize the input audio with the output temporal units.
   *
   * \return Encoder delay in samples.
   */
  virtual uint32_t GetEncoderDelay() const = 0;

  /*!\brief Returns whether this encoder is generating temporal units.
   *
   * \return True until the last temporal unit is output, then false.
   */
  virtual bool GeneratingTemporalUnits() const = 0;

  /*!\brief Adds audio data and parameter block metadata for one temporal unit.
   *
   * Typically, an entire frame of audio should be added at once, and any
   * associated parameter block metadata. The number of audio samples, was
   * configured based on the `CodecConfigObu` metadata at encoder creation.
   *
   * \param temporal_unit_data Temporal unit to add.
   */
  virtual absl::Status Encode(
      const IamfTemporalUnitData& temporal_unit_data) = 0;

  /*!\brief Outputs data OBUs corresponding to one temporal unit.
   *
   * \param temporal_unit_obus Output OBUs corresponding to this temporal unit.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  virtual absl::Status OutputTemporalUnit(
      std::vector<uint8_t>& temporal_unit_obus) = 0;

  /*!\brief Finalizes the process of adding samples.
   *
   * This will signal the underlying codecs to flush all remaining samples,
   * as well as trim samples from the end.
   *
   * After this is called, the encoder should be flushed (with
   * `OutputTemporalUnit`) until `GeneratingTemporalUnits` returns false.
   */
  virtual absl::Status FinalizeEncode() = 0;
};

}  // namespace api
}  // namespace iamf_tools

#endif  // INCLUDE_IAMF_TOOLS_IAMF_ENCODER_INTERFACE_H_
