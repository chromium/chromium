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

#include "iamf/cli/obu_processor.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/descriptor_obu_parser.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/layout_renderer_factory.h"
#include "iamf/cli/obu_processor_utils.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/profile_filter.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status UpdateParameterStatesIfNeeded(
    const DescriptorObus::AudioElementsById& audio_elements_with_data,
    const GlobalTimingModule& global_timing_module,
    ParametersManager& parameters_manager) {
  std::optional<InternalTimestamp> global_timestamp;
  RETURN_IF_NOT_OK(
      global_timing_module.GetGlobalAudioFrameTimestamp(global_timestamp));
  // Not ready to update the states yet.
  if (!global_timestamp.has_value()) {
    return absl::OkStatus();
  }

  // The audio frames for all audio elements are finished; update the
  // parameters manager.
  for (const auto& [audio_element_id, unused_element] :
       audio_elements_with_data) {
    RETURN_IF_NOT_OK(parameters_manager.UpdateDemixingState(audio_element_id,
                                                            *global_timestamp));
    RETURN_IF_NOT_OK(parameters_manager.UpdateReconGainState(
        audio_element_id, *global_timestamp));
  }
  return absl::OkStatus();
}

absl::Status GetAndStoreAudioFrameWithData(
    const ObuHeader& header, const int64_t payload_size,
    const DescriptorObus::AudioElementsById& audio_elements_with_data,
    const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>&
        substream_id_to_audio_element,
    ReadBitBuffer& read_bit_buffer, GlobalTimingModule& global_timing_module,
    ParametersManager& parameters_manager,
    std::optional<AudioFrameWithData>& output_audio_frame_with_data) {
  output_audio_frame_with_data.reset();
  auto audio_frame_obu =
      AudioFrameObu::CreateFromBuffer(header, payload_size, read_bit_buffer);
  if (!audio_frame_obu.ok()) {
    return audio_frame_obu.status();
  }
  const auto substream_id = audio_frame_obu->GetSubstreamId();
  const auto audio_element_iter =
      substream_id_to_audio_element.find(substream_id);
  if (audio_element_iter == substream_id_to_audio_element.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "No audio element found having substream ID: ", substream_id));
  }
  const auto& audio_element_with_data = *audio_element_iter->second;
  auto audio_frame_with_data = ObuWithDataGenerator::GenerateAudioFrameWithData(
      audio_element_with_data, *audio_frame_obu, global_timing_module,
      parameters_manager);
  if (!audio_frame_with_data.ok()) {
    return audio_frame_with_data.status();
  }
  output_audio_frame_with_data = *audio_frame_with_data;

  RETURN_IF_NOT_OK(UpdateParameterStatesIfNeeded(
      audio_elements_with_data, global_timing_module, parameters_manager));

  return absl::OkStatus();
}

const ParamDefinition* PeekParameterIdAndLookupParamDefinition(
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    ReadBitBuffer& read_bit_buffer) {
  auto parameter_id = ParameterBlockObu::PeekParameterId(read_bit_buffer);
  if (!parameter_id.ok()) {
    return nullptr;
  }
  const auto parameter_definition_it =
      param_definition_variants.find(*parameter_id);
  if (parameter_definition_it == param_definition_variants.end()) {
    return nullptr;
  }

  const auto cast_to_base_pointer = [](const auto& param_definition) {
    return static_cast<const ParamDefinition*>(&param_definition);
  };
  return std::visit(cast_to_base_pointer, parameter_definition_it->second);
}

absl::Status GetAndStoreParameterBlockWithData(
    const ObuHeader& header, const int64_t payload_size,
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    ReadBitBuffer& read_bit_buffer, GlobalTimingModule& global_timing_module,
    std::optional<ParameterBlockWithData>& output_parameter_block_with_data) {
  const ParamDefinition* parameter_definition =
      PeekParameterIdAndLookupParamDefinition(param_definition_variants,
                                              read_bit_buffer);
  if (parameter_definition == nullptr) {
    return absl::InvalidArgumentError(
        "Found a stray parameter block OBU (no matching parameter "
        "definition).");
  }

  auto parameter_block_obu = ParameterBlockObu::CreateFromBuffer(
      header, payload_size, *parameter_definition, read_bit_buffer);
  if (!parameter_block_obu.ok()) {
    return parameter_block_obu.status();
  }

  std::optional<InternalTimestamp> global_timestamp;
  RETURN_IF_NOT_OK(
      global_timing_module.GetGlobalAudioFrameTimestamp(global_timestamp));
  if (!global_timestamp.has_value()) {
    return absl::InvalidArgumentError(
        "Global timestamp has no value while generating a parameter "
        "block");
  }

  // Process the newly parsed parameter block OBU.
  auto parameter_block_with_data =
      ObuWithDataGenerator::GenerateParameterBlockWithData(
          *global_timestamp, global_timing_module,
          std::move(*parameter_block_obu));
  if (!parameter_block_with_data.ok()) {
    return parameter_block_with_data.status();
  }
  output_parameter_block_with_data = std::move(*parameter_block_with_data);

  return absl::OkStatus();
}

// Returns a list of pointers to the supported mix presentations. Empty if none
// are supported.
std::list<MixPresentationObu*> GetSupportedMixPresentations(
    const absl::flat_hash_set<ProfileVersion> requested_profiles,
    const DescriptorObus::AudioElementsById& audio_elements,
    DescriptorObus::MixPresentationObus& mix_presentation_obus) {
  // Find a mix presentation and layout that agrees with the requested profiles.
  std::list<MixPresentationObu*> supported_mix_presentations;
  std::string cumulative_error_message;
  for (auto iter = mix_presentation_obus.begin();
       iter != mix_presentation_obus.end(); ++iter) {
    auto profiles = requested_profiles;
    const auto status = ProfileFilter::FilterProfilesForMixPresentation(
        audio_elements, *iter, profiles);
    if (status.ok()) {
      supported_mix_presentations.push_back(&*iter);
    }
    absl::StrAppend(&cumulative_error_message, status.message(), "\n");
  }
  ABSL_LOG(INFO) << "Filtered mix presentations: " << cumulative_error_message
                 << ". Number of supported mix presentations: "
                 << supported_mix_presentations.size();
  return supported_mix_presentations;
}

void GetSampleRateAndFrameSize(
    const DescriptorObus::CodecConfigsById& output_codec_config_obus,
    std::optional<uint32_t>& output_sample_rate,
    std::optional<uint32_t>& output_frame_size) {
  if (output_codec_config_obus.size() != 1) {
    ABSL_LOG(WARNING) << "Expected exactly one codec config OBUs, but found "
                      << output_codec_config_obus.size();
    return;
  }
  const auto& first_codec_config_obu = output_codec_config_obus.begin()->second;
  output_sample_rate = first_codec_config_obu.GetOutputSampleRate();
  output_frame_size = first_codec_config_obu.GetNumSamplesPerFrame();
}

absl::Status ProcessTemporalUnitObu(
    const DescriptorObus::AudioElementsById& audio_elements_with_data,
    const DescriptorObus::CodecConfigsById& codec_config_obus,
    const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>&
        substream_id_to_audio_element,
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    ParametersManager& parameters_manager, ReadBitBuffer& read_bit_buffer,
    GlobalTimingModule& global_timing_module,
    std::optional<AudioFrameWithData>& output_audio_frame_with_data,
    std::optional<ParameterBlockWithData>& output_parameter_block_with_data,
    std::optional<TemporalDelimiterObu>& output_temporal_delimiter,
    bool& continue_processing) {
  continue_processing = true;
  output_audio_frame_with_data.reset();
  output_parameter_block_with_data.reset();
  output_temporal_delimiter.reset();

  auto header_metadata = ObuHeader::PeekObuTypeAndTotalObuSize(read_bit_buffer);
  if (!header_metadata.ok()) {
    if (header_metadata.status().code() ==
        absl::StatusCode::kResourceExhausted) {
      // Can't read header because there is not enough data. This is not an
      // error, but we're done processing for now.
      continue_processing = false;
      return absl::OkStatus();
    } else {
      // Some other error occurred, propagate it.
      return header_metadata.status();
    }
  }

  if (read_bit_buffer.NumBytesAvailable() < header_metadata->total_obu_size) {
    // This is a temporal unit OBU for which we don't have enough data. This is
    // not an error, but we're done processing for now.
    continue_processing = false;
    return absl::OkStatus();
  }

  const int64_t position_before_header = read_bit_buffer.Tell();

  // Read in the header and determines the size of the payload in bytes.
  ObuHeader header;
  int64_t payload_size;
  RETURN_IF_NOT_OK(header.ReadAndValidate(read_bit_buffer, payload_size));

  // Typically we should expect {`kObuIaAudioFrameX`,`kObuIaParameterBlock`,
  // `kObuIaTemporalDelimiter`}. We also want to detect an `kIaSequenceHeader`
  // which would signal the start of a new IA Sequence, and to gracefully
  // handle "reserved" OBUs.
  absl::Status parsed_obu_status = absl::OkStatus();
  switch (header.obu_type) {
    case kObuIaAudioFrame:
    case kObuIaAudioFrameId0:
    case kObuIaAudioFrameId1:
    case kObuIaAudioFrameId2:
    case kObuIaAudioFrameId3:
    case kObuIaAudioFrameId4:
    case kObuIaAudioFrameId5:
    case kObuIaAudioFrameId6:
    case kObuIaAudioFrameId7:
    case kObuIaAudioFrameId8:
    case kObuIaAudioFrameId9:
    case kObuIaAudioFrameId10:
    case kObuIaAudioFrameId11:
    case kObuIaAudioFrameId12:
    case kObuIaAudioFrameId13:
    case kObuIaAudioFrameId14:
    case kObuIaAudioFrameId15:
    case kObuIaAudioFrameId16:
    case kObuIaAudioFrameId17:
      parsed_obu_status = GetAndStoreAudioFrameWithData(
          header, payload_size, audio_elements_with_data,
          substream_id_to_audio_element, read_bit_buffer, global_timing_module,
          parameters_manager, output_audio_frame_with_data);
      break;

    case kObuIaParameterBlock:
      parsed_obu_status = GetAndStoreParameterBlockWithData(
          header, payload_size, param_definition_variants, read_bit_buffer,
          global_timing_module, output_parameter_block_with_data);
      break;
    case kObuIaTemporalDelimiter: {
      // This implementation does not process by temporal unit. Safely ignore
      // it.
      const auto& temporal_delimiter = TemporalDelimiterObu::CreateFromBuffer(
          header, payload_size, read_bit_buffer);
      if (!temporal_delimiter.ok()) {
        parsed_obu_status = temporal_delimiter.status();
        break;
      }
      output_temporal_delimiter = *temporal_delimiter;
      break;
    }
    case kObuIaSequenceHeader:
      if (!header.obu_redundant_copy) {
        // OK. The user of this function will need to reconfigure its state to
        // process the next IA sequence.
        ABSL_LOG(INFO) << "Detected the start of the next IA Sequence.";
        continue_processing = false;
        break;
      }
      // Ok for any IAMF v1.1.0 descriptor OBUs we can skip over redundant
      // copies.
      [[fallthrough]];
    case kObuIaCodecConfig:
    case kObuIaAudioElement:
    case kObuIaMixPresentation:
      if (!header.obu_redundant_copy) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Unexpected non-reserved OBU obu_type= ", header.obu_type));
      }
      // Consume and discard the OBU. IAMF allows us to ignore it (even if the
      // redundant flag is misleading).
      ABSL_LOG(INFO) << "Safely ignoring a redundant OBU.";
      RETURN_IF_NOT_OK(read_bit_buffer.IgnoreBytes(payload_size));
      break;
    default:
      // TODO(b/329705373): Read in the data as an `ArbitraryOBU` and output
      //                    it from this function.
      ABSL_LOG(INFO) << "Detected a reserved OBU. Safely ignoring it.";
      RETURN_IF_NOT_OK(read_bit_buffer.IgnoreBytes(payload_size));
      break;
  }
  if (!parsed_obu_status.ok()) {
    // The spec is permissive in bypassing OBUs that we don't yet understand.
    // These may signal some future features. Ignore the OBU, downstream OBUs
    // that reference it will be ignored.
    ABSL_LOG(WARNING) << "Bypassing OBU: " << header.obu_type
                      << " with status: " << parsed_obu_status
                      << " and seeking past it.";
    RETURN_IF_NOT_OK(read_bit_buffer.Seek(position_before_header));
    RETURN_IF_NOT_OK(
        read_bit_buffer.IgnoreBytes(header_metadata->total_obu_size));
  }

  if (!continue_processing) {
    // Rewind the position to before the last header was read.
    ABSL_LOG(INFO) << "position_before_header: " << position_before_header;
    RETURN_IF_NOT_OK(read_bit_buffer.Seek(position_before_header));
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status ObuProcessor::InitializeInternal(bool is_exhaustive_and_exact,
                                              bool& output_insufficient_data) {
  // Process the descriptor OBUs.
  ABSL_LOG(INFO) << "Starting Descriptor OBU processing";
  absl::StatusOr<DescriptorObus> parsed_obus =
      DescriptorObuParser::ProcessDescriptorObus(
          is_exhaustive_and_exact, *read_bit_buffer_, output_insufficient_data);
  if (!parsed_obus.ok()) {
    return parsed_obus.status();
  }
  descriptors_ = *std::move(parsed_obus);

  ABSL_LOG(INFO) << "Processed Descriptor OBUs";
  RETURN_IF_NOT_OK(CollectAndValidateParamDefinitions(
      descriptors_.audio_elements, descriptors_.mix_presentation_obus,
      param_definition_variants_));
  GetSampleRateAndFrameSize(descriptors_.codec_config_obus, output_sample_rate_,
                            output_frame_size_);
  // Mapping from substream IDs to pointers to audio element with data.
  for (const auto& [audio_element_id, audio_element_with_data] :
       descriptors_.audio_elements) {
    for (const auto& [substream_id, unused_labels] :
         audio_element_with_data.substream_id_to_labels) {
      auto [unused_iter, inserted] = substream_id_to_audio_element_.insert(
          {substream_id, &audio_element_with_data});
      if (!inserted) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Duplicated substream ID: ", substream_id,
            " associated with audio element ID: ", audio_element_id));
      }
    }
  }
  global_timing_module_ = GlobalTimingModule::Create(
      descriptors_.audio_elements, param_definition_variants_);
  if (global_timing_module_ == nullptr) {
    return absl::InvalidArgumentError(
        "Failed to initialize the global timing module");
  }
  auto temp_parameters_manager =
      ParametersManager::Create(descriptors_.audio_elements);
  if (!temp_parameters_manager.ok()) {
    return temp_parameters_manager.status();
  }
  ABSL_CHECK_NE(*temp_parameters_manager, nullptr);
  parameters_manager_ = *std::move(temp_parameters_manager);
  return absl::OkStatus();
}

const IASequenceHeaderObu& ObuProcessor::GetIaSequenceHeaderView() const {
  return descriptors_.ia_sequence_header;
}

const DescriptorObus::CodecConfigsById& ObuProcessor::GetCodecConfigsByIdView()
    const {
  return descriptors_.codec_config_obus;
}

const DescriptorObus::AudioElementsById& ObuProcessor::GetAudioElementsView()
    const {
  return descriptors_.audio_elements;
}

const DescriptorObus::MixPresentationObus&
ObuProcessor::GetMixPresentationObusView() const {
  return descriptors_.mix_presentation_obus;
}

std::unique_ptr<ObuProcessor> absl_nullable ObuProcessor::Create(
    bool is_exhaustive_and_exact, ReadBitBuffer* absl_nonnull read_bit_buffer,
    bool& output_insufficient_data) {
  // `output_insufficient_data` indicates a specific error condition and so is
  // true iff we've received valid data but need more of it.
  output_insufficient_data = false;
  if (read_bit_buffer == nullptr) {
    return nullptr;
  }
  std::unique_ptr<ObuProcessor> obu_processor =
      absl::WrapUnique(new ObuProcessor(read_bit_buffer));
  if (const auto status = obu_processor->InitializeInternal(
          is_exhaustive_and_exact, output_insufficient_data);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }
  return obu_processor;
}

std::unique_ptr<ObuProcessor> absl_nullable ObuProcessor::CreateForRendering(
    const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
    const std::optional<uint32_t>& desired_mix_presentation_id,
    const std::optional<Layout>& desired_layout, bool is_exhaustive_and_exact,
    ReadBitBuffer* absl_nonnull read_bit_buffer,
    TrimmingSettings trimming_settings, bool& output_insufficient_data) {
  // `output_insufficient_data` indicates a specific error condition and so is
  // true iff we've received valid data but need more of it.
  output_insufficient_data = false;
  if (read_bit_buffer == nullptr) {
    return nullptr;
  }
  std::unique_ptr<ObuProcessor> obu_processor =
      absl::WrapUnique(new ObuProcessor(read_bit_buffer));
  if (const auto status = obu_processor->InitializeInternal(
          is_exhaustive_and_exact, output_insufficient_data);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }

  if (const auto status = obu_processor->InitializeForRendering(
          desired_profile_versions, desired_mix_presentation_id, desired_layout,
          trimming_settings);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }
  return obu_processor;
}

absl::StatusOr<uint32_t> ObuProcessor::GetOutputSampleRate() const {
  RETURN_IF_NOT_OK(
      ValidateHasValue(output_sample_rate_,
                       "Output sample rate, was this a trivial IA Sequence?"));
  return *output_sample_rate_;
}

absl::StatusOr<uint32_t> ObuProcessor::GetOutputFrameSize() const {
  RETURN_IF_NOT_OK(
      ValidateHasValue(output_frame_size_,
                       "Output frame size, was this a trivial IA Sequence?"));
  return *output_frame_size_;
}

absl::StatusOr<DecodedUleb128> ObuProcessor::GetOutputMixPresentationId()
    const {
  if (rendering_models_.has_value()) {
    return decoding_layout_info_.mix_presentation_id;
  }
  return absl::FailedPreconditionError("Not initialized for rendering.");
}

absl::StatusOr<Layout> ObuProcessor::GetOutputLayout() const {
  if (rendering_models_.has_value()) {
    return decoding_layout_info_.layout;
  }
  return absl::FailedPreconditionError("Not initialized for rendering.");
}

absl::Status ObuProcessor::InitializeForRendering(
    const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
    const std::optional<uint32_t>& desired_mix_presentation_id,
    const std::optional<Layout>& desired_layout,
    TrimmingSettings trimming_settings) {
  if (descriptors_.mix_presentation_obus.empty()) {
    return absl::InvalidArgumentError("No mix presentation OBUs found.");
  }
  if (descriptors_.audio_elements.empty()) {
    return absl::InvalidArgumentError("No audio element OBUs found.");
  }

  const std::list<MixPresentationObu*> supported_mix_presentations =
      GetSupportedMixPresentations(desired_profile_versions,
                                   descriptors_.audio_elements,
                                   descriptors_.mix_presentation_obus);
  if (supported_mix_presentations.empty()) {
    return absl::NotFoundError("No supported mix presentation OBUs found.");
  }
  absl::StatusOr<SelectedMixPresentation> selected_mix_presentation =
      FindMixPresentationAndLayout(descriptors_.audio_elements,
                                   supported_mix_presentations, desired_layout,
                                   desired_mix_presentation_id);
  if (!selected_mix_presentation.ok()) {
    return selected_mix_presentation.status();
  }
  // Cache the information.
  decoding_layout_info_ = {
      .mix_presentation_id =
          selected_mix_presentation->mix_presentation->GetMixPresentationId(),
      .layout = selected_mix_presentation->output_layout};

  // Even though the bitstream may have many mixes and layouts, `ObuProcessor`
  // exposes an interface to render one of them at a time.
  //
  // Clone a simplified version of the selected mix presentation, so clients do
  // not pay for mixes they cannot observe.
  absl::StatusOr<MixPresentationObu> simplified_mix_presentation =
      CreateSimplifiedMixPresentationForRendering(
          *selected_mix_presentation->mix_presentation,
          selected_mix_presentation->sub_mix_index,
          selected_mix_presentation->layout_index);
  if (!simplified_mix_presentation.ok()) {
    return simplified_mix_presentation.status();
  }

  // Configure simplified audio pipeline, from the simplified mix presentation.
  absl::StatusOr<RenderingModels> rendering_models =
      ConfigureSimplifiedAudioProcessingPipeline(descriptors_.audio_elements,
                                                 *simplified_mix_presentation,
                                                 trimming_settings);
  if (!rendering_models.ok()) {
    return rendering_models.status();
  }
  rendering_models_.emplace(*std::move(rendering_models));
  return absl::OkStatus();
}

absl::Status ObuProcessor::ProcessTemporalUnit(
    bool eos_is_end_of_sequence,
    std::optional<OutputTemporalUnit>& output_temporal_unit,
    bool& continue_processing) {
  // Various checks that should have been handled by the factory functions.
  ABSL_CHECK(parameters_manager_ != nullptr);
  ABSL_CHECK(global_timing_module_ != nullptr);
  ABSL_CHECK(read_bit_buffer_ != nullptr);

  continue_processing = true;
  while (continue_processing) {
    std::optional<AudioFrameWithData> audio_frame_with_data;
    std::optional<ParameterBlockWithData> parameter_block_with_data;
    std::optional<TemporalDelimiterObu> temporal_delimiter;
    RETURN_IF_NOT_OK(ProcessTemporalUnitObu(
        descriptors_.audio_elements, descriptors_.codec_config_obus,
        substream_id_to_audio_element_, param_definition_variants_,
        *parameters_manager_, *read_bit_buffer_, *global_timing_module_,
        audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
        continue_processing));

    // Collect OBUs into a temporal unit.
    bool delimiter_end_condition = false;
    if (audio_frame_with_data.has_value()) {
      TemporalUnitData::AddDataToCorrectTemporalUnit(
          current_temporal_unit_, next_temporal_unit_,
          *std::move(audio_frame_with_data));
    } else if (parameter_block_with_data.has_value()) {
      TemporalUnitData::AddDataToCorrectTemporalUnit(
          current_temporal_unit_, next_temporal_unit_,
          *std::move(parameter_block_with_data));
    } else if (temporal_delimiter.has_value()) {
      if (current_temporal_unit_.temporal_delimiter.has_value()) {
        delimiter_end_condition = true;
      }
      current_temporal_unit_.temporal_delimiter = *temporal_delimiter;
    }

    // The current temporal unit is considered finished if any of the
    // following conditions is met:
    // - The end of sequence is reached.
    // - The timestamp has advanced (i.e. when the next temporal unit gets its
    //   timestamp).
    // - A second temporal delimiter is encountered.
    if ((!continue_processing && eos_is_end_of_sequence) ||
        next_temporal_unit_.timestamp.has_value() || delimiter_end_condition) {
      if (current_temporal_unit_.audio_frames.empty() &&
          current_temporal_unit_.parameter_blocks.empty()) {
        break;
      }
      output_temporal_unit = OutputTemporalUnit();
      output_temporal_unit->output_audio_frames =
          std::move(current_temporal_unit_.audio_frames);
      output_temporal_unit->output_parameter_blocks =
          std::move(current_temporal_unit_.parameter_blocks);
      if (current_temporal_unit_.timestamp.has_value()) {
        output_temporal_unit->output_timestamp =
            current_temporal_unit_.timestamp.value();
      }
      current_temporal_unit_ = std::move(next_temporal_unit_);
      next_temporal_unit_ = TemporalUnitData();
      break;
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<absl::Span<const absl::Span<const InternalSampleType>>>
ObuProcessor::RenderTemporalUnitAndMeasureLoudness(
    InternalTimestamp start_timestamp,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    std::list<AudioFrameWithData>& audio_frames) {
  if (audio_frames.empty()) {
    // Nothing to decode, render, or measure loudness of. Consider this
    // trivially OK.
    return absl::Span<const absl::Span<const InternalSampleType>>();
  }
  if (!rendering_models_.has_value()) {
    return absl::FailedPreconditionError(
        "Not initialized for rendering. Did you call "
        "`CreateForRendering()`?");
  }

  // Decode the temporal unit.
  std::optional<InternalTimestamp> end_timestamp;
  for (auto& audio_frame : audio_frames) {
    // `ObuProcessor` renders only a single mix. Substreams may be irrelevant,
    // and the end-user should not pay to decode them.
    if (!rendering_models_->relevant_substream_ids.contains(
            audio_frame.obu.GetSubstreamId())) {
      continue;
    }
    if (!end_timestamp.has_value()) {
      end_timestamp = audio_frame.end_timestamp;
    }
    RETURN_IF_NOT_OK(
        CompareTimestamps(start_timestamp, audio_frame.start_timestamp,
                          "Audio frame has a different start timestamp than "
                          "the temporal unit: "));
    RETURN_IF_NOT_OK(CompareTimestamps(*end_timestamp,
                                       audio_frame.end_timestamp,
                                       "Audio frame has a different end "
                                       "timestamp than the temporal unit: "));
    RETURN_IF_NOT_OK(
        rendering_models_->audio_frame_decoder.Decode(audio_frame));
  }
  if (!end_timestamp.has_value()) {
    return absl::InvalidArgumentError(
        "No relevant audio frames in the temporal unit.");
  }

  // Reconstruct the temporal unit and store the result in the output map.
  const auto& decoded_labeled_frames_for_temporal_unit =
      rendering_models_->demixing_manager.DemixDecodedAudioSamples(
          audio_frames);
  if (!decoded_labeled_frames_for_temporal_unit.ok()) {
    return decoded_labeled_frames_for_temporal_unit.status();
  }

  RETURN_IF_NOT_OK(
      rendering_models_->mix_presentation_finalizer.PushTemporalUnit(
          *decoded_labeled_frames_for_temporal_unit, start_timestamp,
          *end_timestamp, parameter_blocks));

  // `ObuProcessor` renders a simplified Mix Presentation OBU with a single
  // sub-mix and a single layout.
  constexpr int kSubMixIndex = 0;
  constexpr int kLayoutIndex = 0;
  auto rendered_samples = rendering_models_->mix_presentation_finalizer
                              .GetPostProcessedSamplesAsSpan(
                                  decoding_layout_info_.mix_presentation_id,
                                  kSubMixIndex, kLayoutIndex);
  if (!rendered_samples.ok()) {
    return rendered_samples.status();
  }

  RETURN_IF_NOT_OK(ValidateRenderedSamples(
      *rendered_samples,
      output_frame_size_.has_value() ? *output_frame_size_ : 0));

  // TODO(b/379122580): Add a call to `FinalizePushingTemporalUnits`, then a
  //                    final call to `GetPostProcessedSamplesAsSpan` when there
  //                    are no more temporal units to push. Those calls may
  //                    belong elsewhere in the class depending on the
  //                    interface.
  return *rendered_samples;
}

absl::StatusOr<ObuProcessor::RenderingModels>
ObuProcessor::ConfigureSimplifiedAudioProcessingPipeline(
    const DescriptorObus::AudioElementsById& audio_elements,
    const MixPresentationObu& simplified_mix_presentation,
    TrimmingSettings trimming_settings) {
  // The audio elements IDs that are relevant to the selected mix presentation.
  absl::flat_hash_set<DecodedUleb128> relevant_audio_element_ids;
  for (const auto& sub_mix : simplified_mix_presentation.sub_mixes_) {
    for (const auto& audio_element : sub_mix.audio_elements) {
      relevant_audio_element_ids.insert(audio_element.audio_element_id);
    }
  }

  // Configure the `AudioFrameDecoder`, and prepare the structure which
  // configures the `DemixingManager`. Filter out any irrelevant audio
  // elements. Also cache any irrelevant substream IDs to be filtered out in
  // temporal units.
  AudioFrameDecoder audio_frame_decoder;
  absl::flat_hash_map<DecodedUleb128, DemixingManager::ReconstructionConfig>
      id_to_reconstruction_config;
  absl::flat_hash_set<DecodedUleb128> relevant_substream_ids;
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    if (!relevant_audio_element_ids.contains(audio_element_id)) {
      continue;
    }
    relevant_substream_ids.insert(
        audio_element_with_data.obu.audio_substream_ids_.begin(),
        audio_element_with_data.obu.audio_substream_ids_.end());
    RETURN_IF_NOT_OK(audio_frame_decoder.InitDecodersForSubstreams(
        audio_element_with_data.substream_id_to_labels,
        *audio_element_with_data.codec_config));
    id_to_reconstruction_config[audio_element_id] = {
        .audio_element_obu = &audio_element_with_data.obu,
        .substream_id_to_labels =
            audio_element_with_data.substream_id_to_labels,
        .label_to_output_gain = audio_element_with_data.label_to_output_gain};
  }

  absl::StatusOr<DemixingManager> demixing_manager =
      DemixingManager::Create(id_to_reconstruction_config);
  if (!demixing_manager.ok()) {
    return demixing_manager.status();
  }

  // Create the mix presentation finalizer which is used to render the output
  // files. We neither trust the user-provided loudness, nor care about the
  // calculated loudness.
  const LayoutRendererFactory layout_renderer_factory(trimming_settings);
  absl::StatusOr<RenderingMixPresentationFinalizer> mix_presentation_finalizer =
      RenderingMixPresentationFinalizer::Create(
          &layout_renderer_factory, /*loudness_calculator_factory=*/nullptr,
          audio_elements,
          RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
          {simplified_mix_presentation});
  if (!mix_presentation_finalizer.ok()) {
    return mix_presentation_finalizer.status();
  }

  return RenderingModels{
      .relevant_substream_ids = std::move(relevant_substream_ids),
      .audio_frame_decoder = std::move(audio_frame_decoder),
      .demixing_manager = *std::move(demixing_manager),
      .mix_presentation_finalizer = *std::move(mix_presentation_finalizer),
  };
}

}  // namespace iamf_tools
