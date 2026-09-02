/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_DOWNMIXER_MANAGER_H_
#define CLI_DOWNMIXER_MANAGER_H_

#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/downmixer.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/substream_frames.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace iamf_tools {

struct SubstreamData {
  uint32_t substream_id;

  // Frames of samples that will be stored alongside the audio frame OBUs,
  // including the "virtual" samples that are padded and will be trimmed when
  // decoded. Used for comparison with decoded samples to compute recon gains.
  SubstreamFrames<InternalSampleType> frames_in_obu;

  // Frames of samples to pass to encoder.
  SubstreamFrames<int32_t> frames_to_encode;

  // One or two elements; corresponding to the output gain to be applied to
  // each channel.
  std::vector<double> output_gains_linear;
  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
};

/*!\brief Manages data and processing to down-mix audio elements.
 *
 * Down-mixers are used to down-mix the input channels to the substream
 * channels. Typically there are down-mixers for scalable channel audio
 * elements with more than one layer. Down-mixers are created according to
 * https://aomediacodec.github.io/iamf/#iamfgeneration-scalablechannelaudio-downmixmechanism
 */
class DownmixerManager {
 public:
  struct DownmixingConfig {
    std::list<DownMixer> down_mixers;
    SubstreamIdLabelsMap substream_id_to_labels;
    LabelGainMap label_to_output_gain;
  };

  /*!\brief Creates a `DownmixerManager` with the given configuration.
   *
   * This is most useful from the context of an encoder. For example, to encode
   * a scalable channel audio element with two layers, the input channels are
   * down-mixed according to various rules in the spec.
   *
   * Initializes metadata for each input audio element ID. The metadata includes
   * information about the channels and the specific down-mixers needed for
   * that audio element.
   *
   * \param id_to_config_map Map of Audio Element IDs to `DownmixingConfig`.
   * \return `DownmixerManager` on success.
   */
  static std::unique_ptr<DownmixerManager> Make(
      absl::flat_hash_map<DecodedUleb128, DownmixingConfig> id_to_config_map);

  /*!\brief Creates a `DownmixerManager` for passthrough (no downmixing).
   *
   * This is most useful from the context of a transmuxer or decoder where
   * downmixing is not needed, but we still need to manage substream metadata.
   *
   * \param audio_elements Map of Audio Element ID to `AudioElementWithData`.
   * \return `DownmixerManager` on success.
   */
  static std::unique_ptr<DownmixerManager> MakeForPassthrough(
      const DescriptorObus::AudioElementsById& audio_elements);

  /*!\brief Down-mixes samples of input channels to substreams.
   *
   * \param audio_element_id Audio Element ID of these substreams.
   * \param down_mixing_params Down mixing parameters to use. Ignored when
   *        there is no associated down-mixer.
   * \param input_label_to_samples Samples in input channels organized by the
   *        channel labels.
   * \param substream_id_to_substream_data Mapping from substream IDs to
   *        substream data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DownMixSamplesToSubstreams(
      DecodedUleb128 audio_element_id,
      const DownMixingParams& down_mixing_params,
      LabelSamplesMap& input_label_to_samples,
      absl::flat_hash_map<uint32_t, SubstreamData>&
          substream_id_to_substream_data);

  /*!\brief Returns whether there are down-mixers for an Audio Element ID.
   *
   * \param audio_element_id Audio Element ID.
   * \return `true` if there are down-mixers for an audio element, `false` if
   *         there are no associated down-mixers or the audio element ID is
   *         unknown.
   */
  bool HasDownMixers(DecodedUleb128 audio_element_id) const;

 private:
  /*!\brief Private constructor.
   *
   * For use with factory functions.
   *
   * \param audio_element_id_to_downmixer_metadata Mapping from audio element ID
   *        to downmixer metadata.
   */
  explicit DownmixerManager(
      absl::flat_hash_map<DecodedUleb128, DownmixingConfig>
          audio_element_id_to_downmixing_config)
      : audio_element_id_to_downmixing_config_(
            std::move(audio_element_id_to_downmixing_config)) {}

  absl::flat_hash_map<DecodedUleb128, DownmixingConfig>
      audio_element_id_to_downmixing_config_;
};

}  // namespace iamf_tools

#endif  // CLI_DOWNMIXER_MANAGER_H_
