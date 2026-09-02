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
#include "iamf/cli/iamf_encoder.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/downmixer_manager.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/layout_renderer_factory.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/cli/obu_sequencer_streaming_iamf.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/encoder_control_metadata.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/channel_label_utils.h"
#include "iamf/cli/proto_conversion/downmixing_reconstruction_util.h"
#include "iamf/cli/proto_conversion/proto_to_obu/arbitrary_obu_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_element_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_frame_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/codec_config_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/ia_sequence_header_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/metadata_obu_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/mix_presentation_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/parameter_block_generator.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/metadata_obu.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/types.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace iamf_tools {

namespace {

using ::iamf_tools_cli_proto::ChannelLabelMessage;
using ::iamf_tools_cli_proto::ParameterBlockObuMetadata;

absl::Status InitAudioFrameDecoderForAllAudioElements(
    const DescriptorObus::AudioElementsById& audio_elements,
    AudioFrameDecoder& audio_frame_decoder) {
  for (const auto& [unused_audio_element_id, audio_element] : audio_elements) {
    if (audio_element.codec_config == nullptr) {
      // Skip stray audio elements. We won't know how to decode their
      // substreams.
      continue;
    }

    RETURN_IF_NOT_OK(audio_frame_decoder.InitDecodersForSubstreams(
        audio_element.substream_id_to_labels, *audio_element.codec_config));
  }
  return absl::OkStatus();
}

// Arranges the `ArbitraryObu`s into the non-timestamped (descriptor) and
// timestamped lists.
void ArrangeArbitraryObus(
    std::list<ArbitraryObu>& original_arbitrary_obus,
    std::list<ArbitraryObu>& descriptor_arbitrary_obus,
    absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>&
        timestamp_to_arbitrary_obus) {
  while (!original_arbitrary_obus.empty()) {
    auto& arbitrary_obu = original_arbitrary_obus.front();
    // Arrange to the timestamps slot (if present), or the untimestamped slot,
    // which implies the OBUs are descriptor OBUs.
    auto& list_to_move_to =
        arbitrary_obu.insertion_tick_.has_value()
            ? timestamp_to_arbitrary_obus[*arbitrary_obu.insertion_tick_]
            : descriptor_arbitrary_obus;
    list_to_move_to.push_back(std::move(arbitrary_obu));
    original_arbitrary_obus.pop_front();
  }
}

void SpliceArbitraryObus(
    auto iter_to_splice,
    absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>&
        timestamp_to_arbitrary_obus,
    std::list<ArbitraryObu>& temporal_unit_arbitrary_obus) {
  if (iter_to_splice == timestamp_to_arbitrary_obus.end()) {
    return;
  }
  temporal_unit_arbitrary_obus.splice(temporal_unit_arbitrary_obus.end(),
                                      iter_to_splice->second);
  timestamp_to_arbitrary_obus.erase(iter_to_splice);
}

void PrintAudioFrames(const std::list<AudioFrameWithData>& audio_frames) {
  int i = 0;
  for (const auto& audio_frame_with_data : audio_frames) {
    ABSL_VLOG(1) << "Audio Frame OBU[" << i << "]";

    audio_frame_with_data.obu.PrintObu();
    ABSL_VLOG(1) << "    audio frame.start_timestamp= "
                 << audio_frame_with_data.start_timestamp;
    ABSL_VLOG(1) << "    audio frame.end_timestamp= "
                 << audio_frame_with_data.end_timestamp;

    i++;
  }
}

void ClearSamples(
    absl::flat_hash_map<DecodedUleb128, LabelSamplesMap>& samples) {
  // Clear cached samples for this iteration of data OBU generation.
  for (auto& [unused_audio_element_id, labeled_samples] : samples) {
    for (auto& [unused_label, samples_vector] : labeled_samples) {
      samples_vector.clear();
    }
  }
}

absl::Status PushTemporalUnitToObuSequencers(
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ArbitraryObu>& temporal_unit_arbitrary_obus,
    std::vector<std::unique_ptr<ObuSequencerBase>>& obu_sequencers,
    ObuSequencerStreamingIamf& streaming_obu_sequencer,
    std::vector<uint8_t>& temporal_unit_obus) {
  // Create (and sanitize) a `TemporalUnitView`.
  auto temporal_unit_view = TemporalUnitView::Create(
      parameter_blocks, audio_frames, temporal_unit_arbitrary_obus);
  if (!temporal_unit_view.ok()) {
    return temporal_unit_view.status();
  }
  // Push it to all the `ObuSequencer`s.
  for (auto& obu_sequencer : obu_sequencers) {
    RETURN_IF_NOT_OK(obu_sequencer->PushTemporalUnit(*temporal_unit_view));
  }
  RETURN_IF_NOT_OK(
      streaming_obu_sequencer.PushTemporalUnit(*temporal_unit_view));
  // Fill the output with the final view.
  const auto previous_temporal_unit_obus =
      streaming_obu_sequencer.GetPreviousSerializedTemporalUnit();
  temporal_unit_obus = {previous_temporal_unit_obus.begin(),
                        previous_temporal_unit_obus.end()};
  return absl::OkStatus();
}

// Closes the mix presentation finalizer, overwrites the output mix
// presentation OBUs, and sets the flag to indicate that the OBUs are finalized.
absl::Status FinalizeDescriptors(
    bool validate_user_loudness,
    RenderingMixPresentationFinalizer& mix_presentation_finalizer,
    DescriptorObus::MixPresentationObus& mix_presentation_obus,
    bool& mix_presentation_obus_finalized) {
  if (mix_presentation_obus_finalized) {
    // Skip finalizing twice, in case this is called multiple times.
    return absl::OkStatus();
  }
  ABSL_LOG(INFO) << "Finalizing mix presentation OBUs";

  RETURN_IF_NOT_OK(mix_presentation_finalizer.FinalizePushingTemporalUnits());
  auto finalized_mix_presentation_obus =
      mix_presentation_finalizer.GetFinalizedMixPresentationObus(
          validate_user_loudness);
  if (!finalized_mix_presentation_obus.ok()) {
    return finalized_mix_presentation_obus.status();
  }

  mix_presentation_obus = *std::move(finalized_mix_presentation_obus);
  mix_presentation_obus_finalized = true;
  return absl::OkStatus();
}

// Closes the mix presentation finalizer, overwrites the output mix
// presentation OBUs, and sets the flag to indicate that the OBUs are finalized.
absl::Status FinalizeObuSequencers(
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const std::list<MetadataObu>& metadata_obus,
    const DescriptorObus::CodecConfigsById& codec_config_obus,
    const DescriptorObus::AudioElementsById& audio_elements,
    const DescriptorObus::MixPresentationObus& mix_presentation_obus,
    const std::list<ArbitraryObu>& descriptor_arbitrary_obus,
    std::vector<std::unique_ptr<ObuSequencerBase>>& obu_sequencers,
    ObuSequencerStreamingIamf& streaming_obu_sequencer,
    bool& obu_sequencers_finalized) {
  if (obu_sequencers_finalized) {
    // Skip finalizing twice, in case this is called multiple times.
    return absl::OkStatus();
  }
  ABSL_LOG(INFO) << "Finalizing OBU sequencers";

  // Close all of the `ObuSequencer`s.
  for (auto& obu_sequencer : obu_sequencers) {
    RETURN_IF_NOT_OK(obu_sequencer->UpdateDescriptorObusAndClose(
        ia_sequence_header_obu, metadata_obus, codec_config_obus,
        audio_elements, mix_presentation_obus, descriptor_arbitrary_obus));
  }
  RETURN_IF_NOT_OK(streaming_obu_sequencer.UpdateDescriptorObusAndClose(
      ia_sequence_header_obu, metadata_obus, codec_config_obus, audio_elements,
      mix_presentation_obus, descriptor_arbitrary_obus));

  obu_sequencers_finalized = true;
  return absl::OkStatus();
}

}  // namespace

std::vector<std::unique_ptr<ObuSequencerBase>>
IamfEncoder::CreateNoObuSequencers() {
  return {};
}

absl::StatusOr<std::unique_ptr<IamfEncoder>> IamfEncoder::Create(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const LayoutRendererFactory* absl_nullable layout_renderer_factory,
    const LoudnessCalculatorFactoryBase* absl_nullable
        loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory,
    const ObuSequencerFactory& obu_sequencer_factory) {
  // IA Sequence Header OBU. Only one is allowed.
  if (user_metadata.ia_sequence_header_metadata_size() != 1) {
    return absl::InvalidArgumentError(
        "Only one IA Sequence Header allowed in an IA Sequence.");
  }

  std::optional<IASequenceHeaderObu> ia_sequence_header_obu;
  IaSequenceHeaderGenerator ia_sequence_header_generator(
      user_metadata.ia_sequence_header_metadata(0));
  RETURN_IF_NOT_OK(
      ia_sequence_header_generator.Generate(ia_sequence_header_obu));
  RETURN_IF_NOT_OK(
      ValidateHasValue(ia_sequence_header_obu, "IA Sequence Header"));

  // Metadata OBUs.
  std::list<MetadataObu> metadata_obus;
  MetadataObuGenerator metadata_obu_generator(
      user_metadata.metadata_obu_metadata());
  RETURN_IF_NOT_OK(metadata_obu_generator.Generate(metadata_obus));

  // Codec Config OBUs.
  // Held in a `unique_ptr`, so the underlying map can be moved without
  // invalidating pointers.
  auto codec_config_obus = std::make_unique<DescriptorObus::CodecConfigsById>();
  CodecConfigGenerator codec_config_generator(
      user_metadata.codec_config_metadata());
  RETURN_IF_NOT_OK(codec_config_generator.Generate(*codec_config_obus));

  // Audio Element OBUs.
  // Held in a `unique_ptr`, so the underlying map can be moved without
  // invalidating pointers.
  auto audio_elements = std::make_unique<DescriptorObus::AudioElementsById>();
  AudioElementGenerator audio_element_generator(
      user_metadata.audio_element_metadata());
  RETURN_IF_NOT_OK(
      audio_element_generator.Generate(*codec_config_obus, *audio_elements));

  // Generate the majority of Mix Presentation OBUs - loudness will be
  // calculated after all temporal units have been pushed.
  DescriptorObus::MixPresentationObus mix_presentation_obus;
  MixPresentationGenerator mix_presentation_generator(
      user_metadata.mix_presentation_metadata());
  RETURN_IF_NOT_OK(mix_presentation_generator.Generate(
      user_metadata.encoder_control_metadata().add_build_information_tag(),
      mix_presentation_obus));
  // Initialize a mix presentation mix presentation finalizer. Requires
  // rendering data for every submix to accurately compute loudness.
  auto mix_presentation_finalizer = RenderingMixPresentationFinalizer::Create(
      layout_renderer_factory, loudness_calculator_factory, *audio_elements,
      sample_processor_factory, mix_presentation_obus);
  if (!mix_presentation_finalizer.ok()) {
    return mix_presentation_finalizer.status();
  }

  // Generate Arbitrary OBUs.
  std::list<ArbitraryObu> unorganized_arbitrary_obus;
  ArbitraryObuGenerator arbitrary_obu_generator(
      user_metadata.arbitrary_obu_metadata());
  RETURN_IF_NOT_OK(
      arbitrary_obu_generator.Generate(unorganized_arbitrary_obus));
  // Arrange the `ArbitraryObu`s into the non-timestamped (descriptor) and
  // timestamped lists.
  std::list<ArbitraryObu> descriptor_arbitrary_obus;
  absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>
      timestamp_to_arbitrary_obus;
  ArrangeArbitraryObus(unorganized_arbitrary_obus, descriptor_arbitrary_obus,
                       timestamp_to_arbitrary_obus);

  // Collect and validate consistency of all `ParamDefinition`s in all
  // Audio Element and Mix Presentation OBUs.
  auto param_definition_variants = std::make_unique<
      absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>>();

  RETURN_IF_NOT_OK(CollectAndValidateParamDefinitions(
      *audio_elements, mix_presentation_obus, *param_definition_variants));

  // Initialize the global timing module.
  auto global_timing_module =
      GlobalTimingModule::Create(*audio_elements, *param_definition_variants);
  if (global_timing_module == nullptr) {
    return absl::InvalidArgumentError(
        "Failed to initialize the global timing module");
  }

  // Initialize the parameter block generator.
  ParameterBlockGenerator parameter_block_generator(
      user_metadata.test_vector_metadata().override_computed_recon_gains(),
      *param_definition_variants);
  RETURN_IF_NOT_OK(parameter_block_generator.Initialize(*audio_elements));

  // Put generated parameter blocks in a manager that supports easier queries.
  auto parameters_manager = ParametersManager::Create(*audio_elements);
  if (!parameters_manager.ok()) {
    return parameters_manager.status();
  }
  ABSL_CHECK_NE(*parameters_manager, nullptr);

  // Down-mix the audio samples and then demix audio samples while decoding
  // them. This is useful to create multi-layer audio elements and to determine
  // the recon gain parameters and to measuring loudness.
  absl::StatusOr<
      absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>>
      audio_element_id_to_downmixing_config =
          CreateAudioElementIdToDownmixingConfig(user_metadata,
                                                 *audio_elements);
  if (!audio_element_id_to_downmixing_config.ok()) {
    return audio_element_id_to_downmixing_config.status();
  }

  const auto reconstruction_config =
      DemixingManager::CreateIdToReconstructionConfig(*audio_elements);
  auto demixing_manager = DemixingManager::Create(reconstruction_config);
  if (!demixing_manager.ok()) {
    return demixing_manager.status();
  }

  auto audio_frame_generator = AudioFrameGenerator::Create(
      user_metadata.audio_frame_metadata(),
      user_metadata.codec_config_metadata(), *audio_elements,
      DownmixerManager::Make(*std::move(audio_element_id_to_downmixing_config)),
      **parameters_manager, *global_timing_module);
  if (!audio_frame_generator.ok()) {
    return audio_frame_generator.status();
  }
  ABSL_CHECK_NE(*audio_frame_generator, nullptr);

  // Initialize the audio frame decoder. It is needed to determine the recon
  // gain parameters and measure the loudness of the mixes.
  AudioFrameDecoder audio_frame_decoder;
  RETURN_IF_NOT_OK(InitAudioFrameDecoderForAllAudioElements(
      *audio_elements, audio_frame_decoder));

  // Create the streaming OBU sequencer.
  const auto leb_generator =
      CreateLebGenerator(user_metadata.test_vector_metadata().leb_generator());
  if (leb_generator == nullptr) {
    return absl::InvalidArgumentError("Failed to create LebGenerator.");
  }
  auto streaming_obu_sequencer = std::make_unique<ObuSequencerStreamingIamf>(
      user_metadata.temporal_delimiter_metadata().enable_temporal_delimiters(),
      *leb_generator);

  // Create auxiliary `ObuSequencer`s, and feed the initial descriptor OBUs to
  // them.
  auto obu_sequencers = obu_sequencer_factory();
  for (auto& obu_sequencer : obu_sequencers) {
    // Sanitize the sequencers, because they are tagged as non-nullable.
    ABSL_CHECK_NE(obu_sequencer, nullptr);
    RETURN_IF_NOT_OK(obu_sequencer->PushDescriptorObus(
        *ia_sequence_header_obu, metadata_obus, *codec_config_obus,
        *audio_elements, mix_presentation_obus, descriptor_arbitrary_obus));
  }

  RETURN_IF_NOT_OK(streaming_obu_sequencer->PushDescriptorObus(
      *ia_sequence_header_obu, metadata_obus, *codec_config_obus,
      *audio_elements, mix_presentation_obus, descriptor_arbitrary_obus));

  // Construct the `IamfEncoder`. Move various OBUs, models, etc. into it.
  return absl::WrapUnique(new IamfEncoder(
      user_metadata.test_vector_metadata().validate_user_loudness(),
      *std::move(ia_sequence_header_obu), std::move(metadata_obus),
      std::move(codec_config_obus), std::move(audio_elements),
      std::move(mix_presentation_obus), std::move(descriptor_arbitrary_obus),
      std::move(timestamp_to_arbitrary_obus),
      std::move(param_definition_variants),
      std::move(parameter_block_generator), std::move(*parameters_manager),
      *demixing_manager, *std::move(audio_frame_generator),
      std::move(audio_frame_decoder), std::move(global_timing_module),
      std::move(*mix_presentation_finalizer), std::move(obu_sequencers),
      std::move(streaming_obu_sequencer)));
}

absl::Status IamfEncoder::GetDescriptorObus(
    bool redundant_copy, std::vector<uint8_t>& descriptor_obus,
    bool& output_obus_are_finalized) const {
  if (redundant_copy) {
    // TODO(b/424474611): Implement this path, to facilitate streaming.
    return absl::UnimplementedError(
        "Redundant copy requested, but not implemented by this encoder.");
  }
  // Grab the latest from the streaming sequencer.
  const auto& descriptor_obus_span =
      streaming_obu_sequencer_->GetSerializedDescriptorObus();
  descriptor_obus = {descriptor_obus_span.begin(), descriptor_obus_span.end()};
  output_obus_are_finalized = sequencers_finalized_;
  return absl::OkStatus();
}

uint32_t IamfEncoder::GetEncoderDelay() const {
  return audio_frame_generator_ == nullptr
             ? 0
             : audio_frame_generator_->GetEncoderDelay();
}

bool IamfEncoder::GeneratingTemporalUnits() const {
  // Once the `AudioFrameGenerator` is done, and there are no more extraneous
  // timestamped arbitrary OBUs, we are done.
  return (audio_frame_generator_ != nullptr) &&
         (audio_frame_generator_->TakingSamples() ||
          audio_frame_generator_->GeneratingFrames() ||
          !timestamp_to_arbitrary_obus_.empty());
}

absl::Status IamfEncoder::GetInputTimestamp(
    InternalTimestamp& input_timestamp) {
  std::optional<InternalTimestamp> timestamp;
  RETURN_IF_NOT_OK(
      global_timing_module_->GetGlobalAudioFrameTimestamp(timestamp));
  if (!timestamp.has_value()) {
    return absl::InvalidArgumentError("Global timestamp has no value");
  }
  input_timestamp = *timestamp;
  return absl::OkStatus();
}

absl::Status IamfEncoder::Encode(
    const api::IamfTemporalUnitData& temporal_unit_data) {
  // Parameter blocks need to cover any delayed or trimmed frames. They may be
  // needed even if `finalize_encode_called_` is true.
  for (const auto& [parameter_block_id, raw_parameter_block_metadata] :
       temporal_unit_data.parameter_block_id_to_metadata) {
    ParameterBlockObuMetadata parameter_block_metadata;
    if (!parameter_block_metadata.ParseFromString(
            raw_parameter_block_metadata)) {
      return absl::InvalidArgumentError(
          "Failed to deserialize a `ParameterBlockObuMetadata` protocol "
          "buffer.");
    }
    RETURN_IF_NOT_OK(
        parameter_block_generator_.AddMetadata(parameter_block_metadata));
  }

  if (finalize_encode_called_) {
    // Avoid adding any samples after they are finalized.
    if (!temporal_unit_data.audio_element_id_to_data.empty()) {
      ABSL_LOG_FIRST_N(WARNING, 3)
          << "Calling `Encode()` with samples after "
             "`FinalizeEncode()` drops the audio samples.";
    }

    return absl::OkStatus();
  }

  // TODO(b/428968283): Validate that the input labels are consistent with the
  //                    expected labels in the audio elements.
  for (const auto& [audio_element_id, labeled_samples] :
       temporal_unit_data.audio_element_id_to_data) {
    for (const auto& [label, samples] : labeled_samples) {
      ChannelLabelMessage channel_label_message;
      if (!channel_label_message.ParseFromString(label)) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Failed to deserialize `ChannelLabelMessage` protocol buffer."));
      }
      auto internal_label = ChannelLabelUtils::ProtoToLabel(
          channel_label_message.channel_label());
      if (!internal_label.ok()) {
        return internal_label.status();
      }

      if (samples.empty()) {
        continue;
      }

      // Cache the samples as the internal type.
      auto& cached_samples =
          id_to_labeled_samples_[audio_element_id][*internal_label];
      cached_samples.resize(samples.size());
      // Cast double to InternalSampleType.
      std::transform(samples.begin(), samples.end(), cached_samples.begin(),
                     [](double sample) {
                       return static_cast<InternalSampleType>(sample);
                     });
    }
  }

  return absl::OkStatus();
}

absl::Status IamfEncoder::OutputTemporalUnit(
    std::vector<uint8_t>& temporal_unit_obus) {
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  std::list<ArbitraryObu> temporal_unit_arbitrary_obus;

  // Generate mix gain and demixing parameter blocks.
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateDemixing(
      *global_timing_module_, temp_demixing_parameter_blocks_));
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateMixGain(
      *global_timing_module_, temp_mix_gain_parameter_blocks_));

  // Add the newly generated demixing parameter blocks to the parameters
  // manager so they can be easily queried by the audio frame generator.
  for (const auto& demixing_parameter_block : temp_demixing_parameter_blocks_) {
    parameters_manager_->AddDemixingParameterBlock(&demixing_parameter_block);
  }

  for (const auto& [audio_element_id, labeled_samples] :
       id_to_labeled_samples_) {
    for (const auto& [label, samples] : labeled_samples) {
      // Skip adding empty `samples` to the audio frame generator.
      if (samples.empty()) {
        continue;
      }
      RETURN_IF_NOT_OK(
          audio_frame_generator_->AddSamples(audio_element_id, label, samples));
    }
  }
  ClearSamples(id_to_labeled_samples_);

  if (finalize_encode_called_) {
    RETURN_IF_NOT_OK(audio_frame_generator_->Finalize());
  }

  RETURN_IF_NOT_OK(audio_frame_generator_->OutputFrames(audio_frames));
  if (audio_frames.empty()) {
    // Some audio codec will only output an encoded frame after the next
    // frame "pushes" the old one out. So we wait until the next iteration to
    // retrieve it.
    ABSL_VLOG(1) << "No audio frames generated for this temporal unit.";

    if (finalize_encode_called_) {
      // At the end of the sequence, there could be some extraneous arbitrary
      // OBUs that are not associated with any audio frames. Pop the next set.
      SpliceArbitraryObus(timestamp_to_arbitrary_obus_.begin(),
                          timestamp_to_arbitrary_obus_,
                          temporal_unit_arbitrary_obus);

      // There will be no further audio frames. Descriptors can be closed.
      // Carefully close them before writing out Arbitrary OBUs, which may
      // marked as erronous.
      if (!GeneratingTemporalUnits()) {
        RETURN_IF_NOT_OK(FinalizeDescriptors(
            validate_user_loudness_, mix_presentation_finalizer_,
            mix_presentation_obus_, mix_presentation_obus_finalized_));
      }

      if (!temporal_unit_arbitrary_obus.empty()) {
        RETURN_IF_NOT_OK(PushTemporalUnitToObuSequencers(
            parameter_blocks, audio_frames, temporal_unit_arbitrary_obus,
            obu_sequencers_, *streaming_obu_sequencer_, temporal_unit_obus));
      }

      if (!GeneratingTemporalUnits()) {
        // The final extraneous OBUs have been pushed out. Take this opportunity
        // to finalize the sequencers.
        return FinalizeObuSequencers(
            ia_sequence_header_obu_, metadata_obus_, *codec_config_obus_,
            *audio_elements_, mix_presentation_obus_,
            descriptor_arbitrary_obus_, obu_sequencers_,
            *streaming_obu_sequencer_, sequencers_finalized_);
      }
    }
    return absl::OkStatus();
  }
  // All generated audio frame should be in the same temporal unit; they all
  // have the same timestamps.
  const InternalTimestamp output_start_timestamp =
      audio_frames.front().start_timestamp;
  const InternalTimestamp output_end_timestamp =
      audio_frames.front().end_timestamp;

  // Decode the audio frames in place. The decoded samples are required to
  // determine the demixed frames.
  for (auto& audio_frame : audio_frames) {
    RETURN_IF_NOT_OK(audio_frame_decoder_.Decode(audio_frame));
    ABSL_CHECK_EQ(output_start_timestamp, audio_frame.start_timestamp);
    ABSL_CHECK_EQ(output_end_timestamp, audio_frame.end_timestamp);
  }

  // Demix the original and decoded audio frames, differences between them are
  // useful to compute the recon gain parameters.
  const auto id_to_labeled_frame =
      demixing_manager_.DemixOriginalAudioSamples(audio_frames);
  if (!id_to_labeled_frame.ok()) {
    return id_to_labeled_frame.status();
  }
  const auto id_to_labeled_decoded_frame =
      demixing_manager_.DemixDecodedAudioSamples(audio_frames);
  if (!id_to_labeled_decoded_frame.ok()) {
    return id_to_labeled_decoded_frame.status();
  }

  // Recon gain parameter blocks are generated based on the original and
  // demixed audio frames.
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateReconGain(
      *id_to_labeled_frame, *id_to_labeled_decoded_frame,
      *global_timing_module_, temp_recon_gain_parameter_blocks_));

  // Move all generated parameter blocks belonging to this temporal unit to
  // the output.
  for (auto* temp_parameter_blocks :
       {&temp_mix_gain_parameter_blocks_, &temp_demixing_parameter_blocks_,
        &temp_recon_gain_parameter_blocks_}) {
    auto last_same_timestamp_iter = std::find_if(
        temp_parameter_blocks->begin(), temp_parameter_blocks->end(),
        [output_start_timestamp](const auto& parameter_block) {
          return parameter_block.start_timestamp > output_start_timestamp;
        });
    parameter_blocks.splice(parameter_blocks.end(), *temp_parameter_blocks,
                            temp_parameter_blocks->begin(),
                            last_same_timestamp_iter);
  }

  // Pop out the arbitrary OBUs belonging to this temporal unit.
  SpliceArbitraryObus(timestamp_to_arbitrary_obus_.find(output_start_timestamp),
                      timestamp_to_arbitrary_obus_,
                      temporal_unit_arbitrary_obus);
  // Print the first and last temporal units.
  if (!first_temporal_unit_for_debugging_ || !GeneratingTemporalUnits()) {
    PrintAudioFrames(audio_frames);
    first_temporal_unit_for_debugging_ = true;
  }

  RETURN_IF_NOT_OK(mix_presentation_finalizer_.PushTemporalUnit(
      *id_to_labeled_frame, output_start_timestamp, output_end_timestamp,
      parameter_blocks));
  RETURN_IF_NOT_OK(PushTemporalUnitToObuSequencers(
      parameter_blocks, audio_frames, temporal_unit_arbitrary_obus,
      obu_sequencers_, *streaming_obu_sequencer_, temporal_unit_obus));

  if (GeneratingTemporalUnits()) {
    return absl::OkStatus();
  }
  // The final data OBUs have been pushed out. Take this opportunity to
  // finalize the IA Sequence.
  RETURN_IF_NOT_OK(FinalizeDescriptors(
      validate_user_loudness_, mix_presentation_finalizer_,
      mix_presentation_obus_, mix_presentation_obus_finalized_));
  return FinalizeObuSequencers(
      ia_sequence_header_obu_, metadata_obus_, *codec_config_obus_,
      *audio_elements_, mix_presentation_obus_, descriptor_arbitrary_obus_,
      obu_sequencers_, *streaming_obu_sequencer_, sequencers_finalized_);
}

absl::Status IamfEncoder::FinalizeEncode() {
  if (finalize_encode_called_) {
    ABSL_LOG_FIRST_N(WARNING, 3)
        << "Calling `FinalizeEncode()` multiple times has no effect.";
    return absl::OkStatus();
  }
  finalize_encode_called_ = true;
  if (GeneratingTemporalUnits()) {
    // There are some data OBUs left to generate.
    return absl::OkStatus();
  }

  // This is a trivial IA sequence. Take this opportunity to finalize the IA
  // Sequence.
  RETURN_IF_NOT_OK(FinalizeDescriptors(
      validate_user_loudness_, mix_presentation_finalizer_,
      mix_presentation_obus_, mix_presentation_obus_finalized_));
  return FinalizeObuSequencers(
      ia_sequence_header_obu_, metadata_obus_, *codec_config_obus_,
      *audio_elements_, mix_presentation_obus_, descriptor_arbitrary_obus_,
      obu_sequencers_, *streaming_obu_sequencer_, sequencers_finalized_);
}

const DescriptorObus::AudioElementsById& IamfEncoder::GetAudioElements() const {
  return *audio_elements_;
}

const DescriptorObus::MixPresentationObus& IamfEncoder::GetMixPresentationObus(
    bool& output_is_finalized) const {
  output_is_finalized = mix_presentation_obus_finalized_;
  return mix_presentation_obus_;
}

const std::list<ArbitraryObu>& IamfEncoder::GetDescriptorArbitraryObus() const {
  return descriptor_arbitrary_obus_;
}

}  // namespace iamf_tools
