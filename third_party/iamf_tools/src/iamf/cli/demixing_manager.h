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

#ifndef CLI_DEMIXING_MANAGER_H_
#define CLI_DEMIXING_MANAGER_H_

#include <list>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/demixer.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

typedef absl::flat_hash_map<DecodedUleb128, LabeledFrame> IdLabeledFrameMap;

/*!\brief Manages data and processing to demix audio elements.
 *
 * This class relates to the "Element Reconstructor" as used in the IAMF
 * specifications. "An Element Reconstructor re-assembles the Audio Elements by
 * combining the Channel Group(s) guided by Descriptors and Parameter
 * Substream(s)." This class does not apply the reconstruction gain, so
 * additional post processing is needed to finish audio element reconstruction.
 *
 * Demixers are used to recreate the original audio from the substreams.
 * Demixers are created according to
 * https://aomediacodec.github.io/iamf/#processing-scalablechannelaudio.
 */
class DemixingManager {
 public:
  struct DemixingMetadataForAudioElementId {
    std::list<Demixer> demixers;
    SubstreamIdLabelsMap substream_id_to_labels;
    LabelGainMap label_to_output_gain;
  };

  struct ReconstructionConfig {
    const AudioElementObu* absl_nonnull audio_element_obu;
    SubstreamIdLabelsMap substream_id_to_labels;
    LabelGainMap label_to_output_gain;
  };

  /*!\brief Creates a map of ID to `ReconstructionConfig`.
   *
   * \param audio_elements Audio Elements to source `AudioElementObu`,
   *        `substream_id_to_labels` and `label_to_output_gain` from.
   * \return Map of Audio Element ID to `ReconstructionConfig`.
   */
  static absl::flat_hash_map<DecodedUleb128, ReconstructionConfig>
  CreateIdToReconstructionConfig(
      const DescriptorObus::AudioElementsById& audio_elements);

  /*!\brief Initializes for reconstruction (demixing) the input audio elements.
   *
   * This is most useful from the context of a decoder. For example, to decode
   * a scalable channel audio element with two layers, the substreams are
   * demixed according to various rules in the spec.
   *
   * Initializes metadata for each input audio element ID. The metadata includes
   * information about the channels and the specific demixers needed for that
   * audio element.
   *
   * \param id_to_config Map of Audio Element IDs to `ReconstructionConfig`.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::StatusOr<DemixingManager> Create(
      const absl::flat_hash_map<DecodedUleb128, ReconstructionConfig>&
          id_to_config);

  /*!\brief Demix original audio samples.
   *
   * This is most useful when the original (before lossy codec) samples are
   * known, such as when encoding original audio.
   *
   * \param audio_frames Audio Frames.
   * \return Output data structure for samples, or a specific status on failure.
   */
  absl::StatusOr<IdLabeledFrameMap> DemixOriginalAudioSamples(
      const std::list<AudioFrameWithData>& audio_frames) const;

  /*!\brief Demix decoded audio samples.
   *
   * This is most useful when the decoded (after lossy codec) samples are
   * known, such as when decoding an IA Sequence, or when analyzing the effect
   * of a lossy codec to determine appropriate recon gain values.
   *
   * \param decoded_audio_frames Decoded Audio Frames.
   * \return Output data structure for samples, or a specific status on failure.
   */
  absl::StatusOr<IdLabeledFrameMap> DemixDecodedAudioSamples(
      const std::list<AudioFrameWithData>& decoded_audio_frames) const;

  /*!\brief Gets the demixers associated with an Audio Element ID.
   *
   * \param audio_element_id Audio Element ID
   * \param demixers Output pointer to the list of demixers.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::StatusOr<const std::list<Demixer>* absl_nonnull> GetDemixers(
      DecodedUleb128 audio_element_id) const;

 private:
  /*!\brief Private constructor.
   *
   * For use with `Create`.
   *
   * \param audio_element_id_to_demixing_metadata Mapping from audio element ID
   *        to demixing metadata.
   */
  explicit DemixingManager(
      absl::flat_hash_map<DecodedUleb128, DemixingMetadataForAudioElementId>&&
          audio_element_id_to_demixing_metadata)
      : audio_element_id_to_demixing_metadata_(
            std::move(audio_element_id_to_demixing_metadata)) {}

  const absl::flat_hash_map<DecodedUleb128, DemixingMetadataForAudioElementId>
      audio_element_id_to_demixing_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_DEMIXING_MANAGER_H_
