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

#ifndef CLI_OBU_PROCESSOR_H_
#define CLI_OBU_PROCESSOR_H_

#include <cstdint>
#include <list>
#include <memory>
#include <optional>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

class ObuProcessor {
 public:
  /*!\brief Creates the OBU processor.
   *
   * Creation succeeds only if the descriptor OBUs are successfully processed.
   *
   * \param is_exhaustive_and_exact Whether the bitstream provided is meant to
   *        include all descriptor OBUs and no other data. This should only be
   *        set to true if the user knows the exact boundaries of their set of
   *        descriptor OBUs.
   * \param read_bit_buffer Pointer to the read bit buffer that reads the IAMF
   *        bitstream.
   * \param output_insufficient_data True iff the bitstream provided is
   *        insufficient to process all descriptor OBUs and there is no other
   *        error.
   * \return std::unique_ptr<ObuProcessor> on success. `nullptr` on failure.
   */
  static std::unique_ptr<ObuProcessor> absl_nullable Create(
      bool is_exhaustive_and_exact, ReadBitBuffer* absl_nonnull read_bit_buffer,
      bool& output_insufficient_data);

  /*!\brief Move constructor. */
  ObuProcessor(ObuProcessor&& obu_processor) = delete;

  /*!\brief Creates the OBU processor for rendering.
   *
   * Creation succeeds only if the descriptor OBUs are successfully processed
   * and all rendering modules are successfully initialized.
   *
   * Configures the OBU processor to render and manage a single Mix Presentation
   * and a single Layout.
   *
   * \param desired_profile_versions Profiles that are permitted to be used
   *        selecting the mix presentation.
   * \param desired_mix_presentation_id Optionally specifies the ID of the Mix
   *        Presentation to select.  If not specified, the layout can be used
   *        or defaults will be used.  The ID of the selected Mix can be
   *        verified with `GetOutputMixPresentationId`.
   * \param desired_layout Optionally, specifies the desired layout that will be
   *        used to render the audio, if available in the mix presentations.
   *        The actually selected Layout can be verified with `GetOutputLayout`.
   * \param is_exhaustive_and_exact Whether the bitstream provided is meant to
   *        include all descriptor OBUs and no other data. This should only be
   *        set to true if the user knows the exact boundaries of their set of
   *        descriptor OBUs.
   * \param read_bit_buffer Pointer to the read bit buffer that reads the IAMF
   *        bitstream.
   * \param trimming_settings Trimming settings to apply during rendering.
   * \param output_insufficient_data True iff the bitstream provided is
   *        insufficient to process all descriptor OBUs and there is no other
   *        error.
   * \return Pointer to an ObuProcessor on success. `nullptr` on failure.
   */
  static std::unique_ptr<ObuProcessor> absl_nullable CreateForRendering(
      const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
      const std::optional<uint32_t>& desired_mix_presentation_id,
      const std::optional<Layout>& desired_layout, bool is_exhaustive_and_exact,
      ReadBitBuffer* absl_nonnull read_bit_buffer,
      TrimmingSettings trimming_settings, bool& output_insufficient_data);

  /*!\brief Gets the sample rate of the output audio.
   *
   * \return Sample rate of the output audio, or a specific error code on
   *         failure.
   */
  absl::StatusOr<uint32_t> GetOutputSampleRate() const;

  /*!\brief Gets the frame size of the output audio.
   *
   * Useful to determine the maximum number of samples per
   * `RenderTemporalUnitAndMeasureLoudness` call.
   *
   * \return Number of samples in per frame of the output audio, or a specific
   *         specific error code on failure.
   */
  absl::StatusOr<uint32_t> GetOutputFrameSize() const;

  /*!\brief Gets the selected Mix Presentation ID.
   *
   * Can only be used when created for rendering.
   *
   * \return Mix presentation ID of the output audio, or a specific error code
   *         on failure.
   */
  absl::StatusOr<DecodedUleb128> GetOutputMixPresentationId() const;

  /*!\brief Gets the selected output Layout.
   *
   * Can only be used when created for rendering.
   *
   * \return Layout of the output audio, or a specific error code on failure.
   */
  absl::StatusOr<Layout> GetOutputLayout() const;

  /*!\brief The output of processing a Temporal Unit. */
  struct OutputTemporalUnit {
    std::list<AudioFrameWithData> output_audio_frames;
    std::list<ParameterBlockWithData> output_parameter_blocks;
    InternalTimestamp output_timestamp;
  };

  // TODO(b/379819959): Also handle Temporal Delimiter OBUs.
  /*!\brief Processes all OBUs from a Temporal Unit from the stored IA Sequence.
   *
   * \param eos_is_end_of_sequence Whether reaching the end of the stream
   *        should be considered as the end of the sequence, and therefore the
   *        end of the temporal unit.
   * \param output_temporal_unit Contains the data from the temporal unit that
   *        is processed.
   * \param continue_processing Whether the processing should be continued.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  absl::Status ProcessTemporalUnit(
      bool eos_is_end_of_sequence,
      std::optional<OutputTemporalUnit>& output_temporal_unit,
      bool& continue_processing);

  /*!\brief Renders a temporal unit and measures loudness.
   *
   * `InitializeForRendering()` must be called before calling this.
   *
   * \param timestamp Timestamp of this temporal unit. Used to verify that
   *        the input OBUs actually belong to the same temporal unit.
   * \param parameter_blocks_with_data Parameter Blocks with the requisite data.
   * \param audio_frames_with_data Audio Frames to decode in place.
   * \return Output rendered samples, or a specific status on failure. These
   *         should be used immediately after this function is called; they will
   *         be invalidated after the next call to
   *         `RenderTemporalUnitAndMeasureLoudness()`, as well as after the
   *         `ObuProcessor` is destroyed. A specific status on failure.
   */
  absl::StatusOr<absl::Span<const absl::Span<const InternalSampleType>>>
  RenderTemporalUnitAndMeasureLoudness(
      InternalTimestamp timestamp,
      const std::list<ParameterBlockWithData>& parameter_blocks,
      std::list<AudioFrameWithData>& audio_frames);

  /*!\brief Gets a view of the IA Sequence Header.
   *
   * \return View of the IA Sequence Header.
   */
  const IASequenceHeaderObu& GetIaSequenceHeaderView() const;

  /*!\brief Gets a view of the Codec Config OBUs.
   *
   * \return View of the Codec Config OBUs.
   */
  const DescriptorObus::CodecConfigsById& GetCodecConfigsByIdView() const;

  /*!\brief Gets a view of the Audio Element with metadata.
   *
   * \return View of the Audio Elements with metadata.
   */
  const DescriptorObus::AudioElementsById& GetAudioElementsView() const;

  /*!\brief Gets a view of the Mix Presentation OBUs.
   *
   * \return View of the Mix Presentation OBUs.
   */
  const DescriptorObus::MixPresentationObus& GetMixPresentationObusView() const;

 private:
  /*!\brief Models required to render audio for playback.
   *
   * The IAMF v1.0.0 specification describes several stages of processing in
   * Figure 2. This struct contains classes which help perform audio processing
   * in stages to ultimately produce audio content that can be played back.
   */
  struct RenderingModels {
    // Substream IDs that are relevant to the rendering, below models are only
    // initialized for these substreams.
    absl::flat_hash_set<DecodedUleb128> relevant_substream_ids;
    // "Codec Decoder", according to Figure 2 in IAMF specification.
    AudioFrameDecoder audio_frame_decoder;
    // "Element Reconstructor", according to Figure 2 in IAMF specification.
    DemixingManager demixing_manager;
    // Combined "Renderer" and "Mixer", according to Figure 2 in IAMF
    // specification.
    RenderingMixPresentationFinalizer mix_presentation_finalizer;
  };

  /*!\brief Private constructor used only by Create() and CreateForRendering().
   *
   * \param read_bit_buffer Pointer to the read bit buffer that reads the IAMF
   *        bitstream.
   * \return ObuProcessor instance.
   */
  explicit ObuProcessor(ReadBitBuffer* absl_nonnull buffer)
      : read_bit_buffer_(buffer) {}

  /*!\brief Configures the audio processing pipeline for rendering.
   *
   * \param audio_elements Audio elements, irrelevant ones will be ignored.
   * \param simplified_mix_presentation Simplified mix presentation to render.
   * \param trimming_settings Trimming settings to pass to the renderer factory.
   */
  static absl::StatusOr<RenderingModels>
  ConfigureSimplifiedAudioProcessingPipeline(
      const DescriptorObus::AudioElementsById& audio_elements,
      const MixPresentationObu& simplified_mix_presentation,
      TrimmingSettings trimming_settings);

  /*!\brief Performs internal initialization of the OBU processor.
   *
   * Only used by Create() and CreateForRendering().
   *
   * \param is_exhaustive_and_exact Whether the bitstream provided is meant to
   *        include all descriptor OBUs and no other data. This should only be
   *        set to true if the user knows the exact boundaries of their set of
   *        descriptor OBUs.
    \param output_insufficient_data True iff the bitstream provided is
   *        insufficient to process all descriptor OBUs and there is no other
   *        error.
   * \return `absl::OkStatus()` if initialization is successful. A specific
   *         status on failure.
   */
  absl::Status InitializeInternal(bool is_exhaustive_and_exact,
                                  bool& output_insufficient_data);

  /*!\brief Initializes the OBU processor for rendering.
   *
   * Must be called after `Initialize()` is called.
   *
   * \param desired_profile_versions Profiles that are permitted to be used
   *        selecting the mix presentation.
   * \param desired_mix_presentation_id Optionally specifies the ID of the Mix
   *        Presentation to select.  If not specified, the layout can be used
   *        or defaults will be used.  The ID of the selected Mix can be
   *        verified with `GetOutputMixPresentationId`.
   * \param desired_layout Optionally, specifies the desired layout that will be
   *        used to render the audio, if available in the mix presentations.
   *        The actually selected Layout can be verified with `GetOutputLayout`.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  absl::Status InitializeForRendering(
      const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
      const std::optional<uint32_t>& desired_mix_presentation_id,
      const std::optional<Layout>& desired_layout,
      TrimmingSettings trimming_settings);

  struct DecodingLayoutInfo {
    DecodedUleb128 mix_presentation_id;
    Layout layout;
  };

  struct TemporalUnitData {
    std::list<ParameterBlockWithData> parameter_blocks;
    std::list<AudioFrameWithData> audio_frames;

    std::optional<TemporalDelimiterObu> temporal_delimiter;
    std::optional<InternalTimestamp> timestamp;

    bool Empty() const {
      return parameter_blocks.empty() && audio_frames.empty();
    }

    void Clear() {
      audio_frames.clear();
      parameter_blocks.clear();
      temporal_delimiter.reset();
      timestamp.reset();
    }

    template <class T>
    static void AddDataToCorrectTemporalUnit(
        TemporalUnitData& current_temporal_unit,
        TemporalUnitData& next_temporal_unit, T&& obu_with_data) {
      const InternalTimestamp new_timestamp = obu_with_data.start_timestamp;
      if (!current_temporal_unit.timestamp.has_value()) {
        current_temporal_unit.timestamp = new_timestamp;
      }
      if (*current_temporal_unit.timestamp == new_timestamp) {
        current_temporal_unit.GetList<T>().push_back(
            std::forward<T>(obu_with_data));
      } else {
        next_temporal_unit.GetList<T>().push_back(
            std::forward<T>(obu_with_data));
        next_temporal_unit.timestamp = new_timestamp;
      }
    }

   private:
    template <class T>
    std::list<T>& GetList() {
      if constexpr (std::is_same_v<T, ParameterBlockWithData>) {
        return parameter_blocks;
      } else if constexpr (std::is_same_v<T, AudioFrameWithData>) {
        return audio_frames;
      }
    }
  };

  std::optional<uint32_t> output_sample_rate_;
  std::optional<uint32_t> output_frame_size_;

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      param_definition_variants_;
  absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element_;
  std::unique_ptr<GlobalTimingModule> global_timing_module_;
  std::unique_ptr<ParametersManager> parameters_manager_;
  ReadBitBuffer* absl_nonnull read_bit_buffer_;

  // Contains target layout information for rendering.
  DecodingLayoutInfo decoding_layout_info_;

  // Cached data when processing temporal units.
  TemporalUnitData current_temporal_unit_;
  TemporalUnitData next_temporal_unit_;

  // Modules used for rendering, present iff `CreateForRendering()` was called.
  std::optional<RenderingModels> rendering_models_;

  // Parsed descriptor OBUs.
  DescriptorObus descriptors_;
};
}  // namespace iamf_tools
#endif  // CLI_OBU_PROCESSOR_H_
