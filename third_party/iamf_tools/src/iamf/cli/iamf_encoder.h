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
#ifndef CLI_IAMF_ENCODER_H_
#define CLI_IAMF_ENCODER_H_

#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/layout_renderer_factory.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/cli/obu_sequencer_streaming_iamf.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_frame_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/parameter_block_generator.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/include/iamf_tools/iamf_encoder_interface.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/metadata_obu.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/types.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace iamf_tools {

/*!\brief A class that encodes an IA Sequence and generates OBUs.
 *
 * Descriptor OBUs are generated once at the beginning, and data OBUs are
 * generated iteratively for each temporal unit (TU). The use pattern of this
 * class is:
 *   // Call factory function.
 *   auto encoder = IamfEncoder::Create(...);
 *   if(!encoder.ok()) {
 *     // Handle error.
 *   }
 *
 * Typically, after creation, this class should be used as per the
 * documentation of `IamfEncoderInterface`.
 *
 * For historical reasons, this implementation has some additional functions in
 * this class that are not derived from the interface. These are:
 *   - `GetAudioElements`
 *   - `GetMixPresentationObus`
 *   - `GetDescriptorArbitraryObus`
 *   - `GetInputTimestamp`
 *
 * Several of these functions pertain to examining the output OBUs, and are
 * deprecated.
 *
 * `GetInputTimestamp` is used help the test suite determine the timestamp of
 * the parameter blocks to be fed into th encoder. A typical user would not know
 * all of the parameter blocks beforehand, so they would not need this
 * additional function to help arrange them.
 *
 * Note the timestamps corresponding to parameter blocks and audio frames
 * in `Encode()` might be different from that of the output OBUs obtained in
 * `OutputTemporalUnit()`, because some codecs introduce a frame of delay. We
 * thus distinguish the concepts of input and output timestamps
 * (`input_timestamp` and `output_timestamp`) in the code below.
 */
class IamfEncoder : public api::IamfEncoderInterface {
 public:
  /*!\brief Factory to create `ObuSequencerBases`. */
  typedef absl::AnyInvocable<std::vector<std::unique_ptr<ObuSequencerBase>>()
                                 const>
      ObuSequencerFactory;

  /*!\brief Factory that returns no `ObuSequencerBases`s.
   *
   * For convenience to use with `Create`.
   */
  static std::vector<std::unique_ptr<ObuSequencerBase>> CreateNoObuSequencers();

  /*!\brief Factory function to create an `IamfEncoder`.
   *
   * \param user_metadata Input user metadata describing the IAMF stream.
   * \param layout_renderer_factory Factory to create layout renderers for use
   *        in measuring the loudness.
   * \param loudness_calculator_factory Factory to create loudness calculators
   *        to measure the loudness of the output layouts.
   * \param sample_processor_factory Factory to create processors for use after
   *        rendering.
   * \param obu_sequencer_factory Factory to create `ObuSequencerBases`.
   * \return Encoder on success, or a specific status on failure.
   */
  static absl::StatusOr<std::unique_ptr<IamfEncoder>> Create(
      const iamf_tools_cli_proto::UserMetadata& user_metadata,
      const LayoutRendererFactory* absl_nullable layout_renderer_factory,
      const LoudnessCalculatorFactoryBase* absl_nullable
          loudness_calculator_factory,
      const RenderingMixPresentationFinalizer::SampleProcessorFactory&
          sample_processor_factory,
      const ObuSequencerFactory& obu_sequencer_factory);

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
  absl::Status GetDescriptorObus(
      bool redundant_copy, std::vector<uint8_t>& descriptor_obus,
      bool& output_obus_are_finalized) const override;

  /*!\brief Gets the encoder delay in samples.
   *
   * Certain settings or codecs have an inherent delay in the encoding process.
   * E.g. pre-skip in Opus. Typically the output Audio Frame OBUs will signal
   * this delay by signalling `samples_to_trim_at_start`. This delay may be
   * useful to help synchronize the input audio with the output temporal units.
   *
   * \return Encoder delay in samples.
   */
  uint32_t GetEncoderDelay() const override;

  /*!\brief Returns whether this encoder is generating data OBUs.
   *
   * \return True if still generating data OBUs.
   */
  bool GeneratingTemporalUnits() const override;

  /*!\brief Gets the input timestamp of the data OBU generation iteration.
   *
   * \param input_timestamp Result of input timestamp.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status GetInputTimestamp(InternalTimestamp& input_timestamp);

  /*!\brief Adds audio data and parameter block metadata for one temporal unit.
   *
   * The best practice is to not call this function with samples after
   * `FinalizeEncode()`. But it is OK if you do -- just that the added
   * samples will be ignored and not encoded.
   *
   * Typically, an entire frame of audio should be added at once, and any
   * associated parameter block metadata. The number of audio samples, was
   * configured based on the `CodecConfigObu` metadata at encoder creation.
   *
   * \param temporal_unit_data Temporal unit to add.
   */
  absl::Status Encode(
      const api::IamfTemporalUnitData& temporal_unit_data) override;

  /*!\brief Outputs data OBUs corresponding to one temporal unit.
   *
   * \param temporal_unit_obus Output OBUs corresponding to this temporal unit.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status OutputTemporalUnit(
      std::vector<uint8_t>& temporal_unit_obus) override;

  /*!\brief Finalizes the process of encoding.
   *
   * This will signal the underlying codecs to flush all remaining samples,
   * as well as trim samples from the end.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status FinalizeEncode() override;

  /*!\brief Outputs a const reference to the Audio Elements.
   *
   * \return Const reference to the Audio Elements.
   */
  // TODO(b/273469020): Remove remnants of the OBU-based API.
  [[deprecated("Use GetDescriptorObus() instead.")]]
  const DescriptorObus::AudioElementsById& GetAudioElements() const;

  /*!\brief Outputs a const reference to the preliminary Mix Presentation OBUs.
   *
   * When `GeneratingTemporalUnits()` is true, this function will return the
   * preliminary mix presentation OBUs. These are not finalized, and thus almost
   * certainly do not contain measured loudness metadata.
   *
   * After `GeneratingTemporalUnits()` is false, this function will return the
   * finalized mix presentation OBUs. These contain accurate mix presentation
   * metadata.
   *
   * \param output_is_finalized `true` when the output OBUs have been finalized.
   *        `false` when the output OBUs are preliminary.
   * \return Mix Presentation OBUs.
   */
  // TODO(b/273469020): Remove remnants of the OBU-based API.
  [[deprecated("Use GetDescriptorObus() instead.")]]
  const DescriptorObus::MixPresentationObus& GetMixPresentationObus(
      bool& output_is_finalized) const;

  /*!\brief Outputs a const reference to the Descriptor Arbitrary OBUs.
   *
   * \return Const reference to the Descriptor Arbitrary OBUs.
   */
  // TODO(b/273469020): Remove remnants of the OBU-based API.
  [[deprecated("Use GetDescriptorObus() instead.")]]
  const std::list<ArbitraryObu>& GetDescriptorArbitraryObus() const;

 private:
  /*!\brief Private constructor.
   *
   * Moves from the input arguments Some arguments are wrapped in unique
   * pointers to ensure pointer or reference stability after move.
   *
   * \param validate_user_loudness Whether to validate the user-provided
   *        loudness.
   * \param ia_sequence_header_obu Generated IA Sequence Header OBU.
   * \param codec_config_obus Map of Codec Config ID to generated Codec Config
   *        OBUs.
   * \param audio_elements Map of Audio Element IDs to generated OBUs with data.
   * \param mix_presentation_obus List of preliminary Mix Presentation OBUs.
   * \param descriptor_arbitrary_obus List of Descriptor Arbitrary OBUs.
   * \param timestamp_to_arbitrary_obus Arbitrary OBUs arranged by their
   *        insertion timestamp.
   * \param parameter_id_to_metadata Mapping from parameter IDs to per-ID
   *        parameter metadata.
   * \param param_definition_variants Parameter definitions for the IA Sequence.
   * \param parameters_manager Manager to support internal querying
   *        of parameters.
   * \param demixing_manager Manager to demix audio elements.
   * \param audio_frame_generator Audio frame generator.
   * \param audio_frame_decoder Decodes the original audio frames, to facilitate
   *        recon gain computation.
   * \param global_timing_module Manages global timing information.
   */
  IamfEncoder(
      bool validate_user_loudness, IASequenceHeaderObu&& ia_sequence_header_obu,
      std::list<MetadataObu>&& metadata_obus,
      std::unique_ptr<DescriptorObus::CodecConfigsById> codec_config_obus,
      std::unique_ptr<DescriptorObus::AudioElementsById> audio_elements,
      DescriptorObus::MixPresentationObus&& mix_presentation_obus,
      std::list<ArbitraryObu>&& descriptor_arbitrary_obus,
      absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>&&
          timestamp_to_arbitrary_obus,
      std::unique_ptr<
          absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>>
          param_definition_variants,
      ParameterBlockGenerator&& parameter_block_generator,
      std::unique_ptr<ParametersManager> parameters_manager,
      const DemixingManager& demixing_manager,
      std::unique_ptr<AudioFrameGenerator> audio_frame_generator,
      AudioFrameDecoder&& audio_frame_decoder,
      std::unique_ptr<GlobalTimingModule> global_timing_module,
      RenderingMixPresentationFinalizer&& mix_presentation_finalizer,
      std::vector<std::unique_ptr<ObuSequencerBase>>&& obu_sequencers,
      std::unique_ptr<ObuSequencerStreamingIamf> streaming_obu_sequencer)
      : validate_user_loudness_(validate_user_loudness),
        ia_sequence_header_obu_(std::move(ia_sequence_header_obu)),
        metadata_obus_(std::move(metadata_obus)),
        codec_config_obus_(std::move(codec_config_obus)),
        audio_elements_(std::move(audio_elements)),
        mix_presentation_obus_(std::move(mix_presentation_obus)),
        descriptor_arbitrary_obus_(std::move(descriptor_arbitrary_obus)),
        timestamp_to_arbitrary_obus_(std::move(timestamp_to_arbitrary_obus)),
        param_definition_variants_(std::move(param_definition_variants)),
        parameter_block_generator_(std::move(parameter_block_generator)),
        parameters_manager_(std::move(parameters_manager)),
        demixing_manager_(demixing_manager),
        audio_frame_generator_(std::move(audio_frame_generator)),
        audio_frame_decoder_(std::move(audio_frame_decoder)),
        global_timing_module_(std::move(global_timing_module)),
        mix_presentation_finalizer_(std::move(mix_presentation_finalizer)),
        obu_sequencers_(std::move(obu_sequencers)),
        streaming_obu_sequencer_(std::move(streaming_obu_sequencer)) {}

  const bool validate_user_loudness_;

  // Descriptor OBUs.
  IASequenceHeaderObu ia_sequence_header_obu_;
  std::list<MetadataObu> metadata_obus_;
  // Held in a `unique_ptr`, so the underlying map can be moved without
  // invalidating pointers. At least `audio_elements_` depend on this.
  std::unique_ptr<DescriptorObus::CodecConfigsById> codec_config_obus_;
  // Held in a `unique_ptr`, so the underlying map can be moved without
  // invalidating pointers. At least `audio_frame_generator_` and any output
  // `AudioFrameWithData` depend on this.
  std::unique_ptr<DescriptorObus::AudioElementsById> audio_elements_;
  DescriptorObus::MixPresentationObus mix_presentation_obus_;
  std::list<ArbitraryObu> descriptor_arbitrary_obus_;

  // Arbitrary OBUs arranged by their insertion tick.
  absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>
      timestamp_to_arbitrary_obus_;

  // State to add additional logging for the first temporal unit.
  bool first_temporal_unit_for_debugging_ = false;

  // Mapping from parameter IDs to parameter definitions.
  // Parameter block generator owns a reference to this map. Wrapped in
  // `std::unique_ptr` for reference stability after move.
  std::unique_ptr<
      const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>>
      param_definition_variants_;

  // Saved parameter blocks generated in one iteration.
  std::list<ParameterBlockWithData> temp_mix_gain_parameter_blocks_;
  std::list<ParameterBlockWithData> temp_demixing_parameter_blocks_;
  std::list<ParameterBlockWithData> temp_recon_gain_parameter_blocks_;

  // Cached mapping from Audio Element ID to labeled samples added in the same
  // iteration.
  absl::flat_hash_map<DecodedUleb128, LabelSamplesMap> id_to_labeled_samples_;

  // Whether the `FinalizeEncode()` has been called.
  bool finalize_encode_called_ = false;

  // Various generators and modules used when generating data OBUs iteratively.
  // Some are held in `unique_ptr` for reference stability after move.
  ParameterBlockGenerator parameter_block_generator_;
  std::unique_ptr<ParametersManager> parameters_manager_;
  const DemixingManager demixing_manager_;
  std::unique_ptr<AudioFrameGenerator> audio_frame_generator_;
  AudioFrameDecoder audio_frame_decoder_;
  std::unique_ptr<GlobalTimingModule> global_timing_module_;

  // Modules to render the output layouts and measure their loudness.
  RenderingMixPresentationFinalizer mix_presentation_finalizer_;
  // True after the mix presentation OBUs are finalized.
  bool mix_presentation_obus_finalized_ = false;

  // Optional sequencers to generate OBUs.
  std::vector<std::unique_ptr<ObuSequencerBase>> obu_sequencers_;
  // Backing sequencer, to back output of serialized OBUs. Held as a specific
  // class, because it has extra functions not available in the base class.
  std::unique_ptr<ObuSequencerStreamingIamf> streaming_obu_sequencer_;
  // True after the sequencers have been finalized.
  bool sequencers_finalized_ = false;
};

}  // namespace iamf_tools

#endif  // CLI_IAMF_ENCODER_H_
