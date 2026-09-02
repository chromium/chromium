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
#include "iamf/obu/audio_element.h"

#include <cstdint>
#include <cstdlib>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/param_definitions/extended_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

void LogChannelBased(const ScalableChannelLayoutConfig& channel_config) {
  ABSL_VLOG(1) << "  scalable_channel_layout_config:";
  ABSL_VLOG(1) << "    num_layers= "
               << absl::StrCat(channel_config.GetNumLayers());
  ABSL_VLOG(1) << "    reserved= " << absl::StrCat(channel_config.reserved);
  for (int i = 0; i < channel_config.GetNumLayers(); ++i) {
    ABSL_VLOG(1) << "    channel_audio_layer_configs[" << i << "]:";
    const auto& channel_audio_layer_config =
        channel_config.channel_audio_layer_configs[i];
    ABSL_VLOG(1) << "      loudspeaker_layout= "
                 << absl::StrCat(channel_audio_layer_config.loudspeaker_layout);
    ABSL_VLOG(1) << "      output_gain_is_present_flag= "
                 << absl::StrCat(
                        channel_audio_layer_config.output_gain_is_present_flag);
    ABSL_VLOG(1) << "      recon_gain_is_present_flag= "
                 << absl::StrCat(
                        channel_audio_layer_config.recon_gain_is_present_flag);
    ABSL_VLOG(1) << "      reserved= "
                 << absl::StrCat(channel_audio_layer_config.reserved_a);
    ABSL_VLOG(1) << "      substream_count= "
                 << absl::StrCat(channel_audio_layer_config.substream_count);
    ABSL_VLOG(1) << "      coupled_substream_count= "
                 << absl::StrCat(
                        channel_audio_layer_config.coupled_substream_count);
    if (channel_audio_layer_config.output_gain_is_present_flag == 1) {
      ABSL_VLOG(1) << "      output_gain_flag= "
                   << absl::StrCat(channel_audio_layer_config.output_gain_flag);
      ABSL_VLOG(1) << "      reserved= "
                   << absl::StrCat(channel_audio_layer_config.reserved_b);
      ABSL_VLOG(1) << "      output_gain= "
                   << channel_audio_layer_config.output_gain;
    }
    if (channel_audio_layer_config.expanded_loudspeaker_layout.has_value()) {
      ABSL_VLOG(1) << "      expanded_loudspeaker_layout= "
                   << absl::StrCat(*channel_audio_layer_config
                                        .expanded_loudspeaker_layout);
    } else {
      ABSL_VLOG(1) << "      expanded_loudspeaker_layout= Not present.";
    }
  }
}

absl::Status ValidateNumParameters(size_t num_parameters) {
  // Section 3.6 of IAMF specification says that: "Parsers SHALL support any
  // value of num_parameters."
  //
  // In practice IAMF defines only a small number of parameter types,
  // forbids them from being duplicate, and only permits certain types in Audio
  // Elements.
  //
  // To reduce the risk of allocating massive amounts of memory, we limit the
  // number of parameters.
  if (num_parameters > AudioElementObu::kMaxNumParameters) {
    return absl::UnimplementedError(absl::StrCat(
        "Number of parameters exceeds the maximum supported by the decoder: ",
        num_parameters));
  }
  return absl::OkStatus();
}

// Returns `absl::OkStatus()` if all parameters have a unique
// `param_definition_type` in the OBU. `absl::InvalidArgumentError()`
// otherwise.
absl::Status ValidateUniqueParamDefinitionType(
    const std::vector<AudioElementParam>& audio_element_params) {
  std::vector<ParamDefinition::ParameterDefinitionType>
      collected_param_definition_types;
  collected_param_definition_types.reserve(audio_element_params.size());
  for (const auto& param : audio_element_params) {
    collected_param_definition_types.push_back(param.GetType());
  }

  return ValidateUnique(collected_param_definition_types.begin(),
                        collected_param_definition_types.end(),
                        "audio_element_params");
}

// Writes an element of the `audio_element_params` array of a scalable channel
// `AudioElementObu`.
absl::Status ValidateAndWriteAudioElementParam(const AudioElementParam& param,
                                               WriteBitBuffer& wb) {
  const auto param_definition_type = param.GetType();

  // Write the main portion of the `AudioElementParam`.
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(param_definition_type)));

  if (param_definition_type == ParamDefinition::kParameterDefinitionMixGain) {
    return absl::InvalidArgumentError(
        "Mix Gain parameter type is explicitly forbidden for "
        "Audio Element OBUs.");
  }
  RETURN_IF_NOT_OK(std::visit(
      [&wb](auto& param_definition) {
        return param_definition.ValidateAndWrite(wb);
      },
      param.param_definition));

  return absl::OkStatus();
}

// Writes the `ScalableChannelLayoutConfig` of an `AudioElementObu`.
absl::Status ValidateAndWriteScalableChannelLayout(
    const ScalableChannelLayoutConfig& layout,
    const DecodedUleb128 num_substreams, WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(layout.Validate(num_substreams));

  // Write the main portion of the `ScalableChannelLayoutConfig`.
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(layout.GetNumLayers(), 3));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(layout.reserved, 5));

  // Loop to write the `channel_audio_layer_configs` array.
  for (const auto& layer_config : layout.channel_audio_layer_configs) {
    RETURN_IF_NOT_OK(layer_config.Write(wb));
  }

  return absl::OkStatus();
}

// Reads the `ScalableChannelLayoutConfig` of an `AudioElementObu`.
absl::Status ReadAndValidateScalableChannelLayout(
    ScalableChannelLayoutConfig& layout, const DecodedUleb128 num_substreams,
    ReadBitBuffer& rb) {
  // Read the main portion of the `ScalableChannelLayoutConfig`.
  uint8_t num_layers;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(3, num_layers));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, layout.reserved));

  layout.channel_audio_layer_configs.reserve(num_layers);
  for (int i = 0; i < num_layers; ++i) {
    ChannelAudioLayerConfig layer_config;
    RETURN_IF_NOT_OK(layer_config.Read(rb));
    layout.channel_audio_layer_configs.push_back(layer_config);
  }

  RETURN_IF_NOT_OK(layout.Validate(num_substreams));

  return absl::OkStatus();
}

// Writes the `ObjectsConfig` of an `AudioElementObu`.
absl::Status WriteObjectsConfig(const ObjectsConfig& objects_config,
                                WriteBitBuffer& wb) {
  // Validation is not necessary here because the `ObjectsConfig` struct is
  // already validated when it is created.

  // `object_config_size` is the number of bytes in the extension, plus one for
  // the num_objects field.
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      objects_config.GetObjectsConfigExtensionBytesView().size() + 1, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(objects_config.GetNumObjects(), 8));
  RETURN_IF_NOT_OK(
      wb.WriteUint8Span(objects_config.GetObjectsConfigExtensionBytesView()));
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<ObjectsConfig> ObjectsConfig::Create(
    uint8_t num_objects,
    absl::Span<const uint8_t> objects_config_extension_bytes) {
  if (num_objects > 2 || num_objects == 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected `num_objects` in [1, 2]; got ", num_objects));
  }
  return ObjectsConfig(
      num_objects, std::vector<uint8_t>(objects_config_extension_bytes.begin(),
                                        objects_config_extension_bytes.end()));
}

absl::StatusOr<ObjectsConfig> ObjectsConfig::CreateFromBuffer(
    ReadBitBuffer& rb) {
  uint8_t object_config_size;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, object_config_size));
  if (object_config_size == 0) {
    return absl::InvalidArgumentError(
        "Invalid object_config_size = 0. This should be at least 1.");
  }
  uint8_t num_objects;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, num_objects));
  std::vector<uint8_t> objects_config_extension_bytes;
  objects_config_extension_bytes.resize(object_config_size - 1);
  RETURN_IF_NOT_OK(
      rb.ReadUint8Span(absl::MakeSpan(objects_config_extension_bytes)));
  return Create(num_objects,
                absl::MakeConstSpan(objects_config_extension_bytes));
}

uint8_t ObjectsConfig::GetNumObjects() const { return num_objects_; }

absl::Span<const uint8_t> ObjectsConfig::GetObjectsConfigExtensionBytesView()
    const {
  return objects_config_extension_bytes_;
}

ObjectsConfig::ObjectsConfig(
    uint8_t num_objects, std::vector<uint8_t> objects_config_extension_bytes)
    : num_objects_(num_objects),
      objects_config_extension_bytes_(
          std::move(objects_config_extension_bytes)) {}

absl::Status AudioElementParam::ReadAndValidate(uint32_t audio_element_id,
                                                ReadBitBuffer& rb) {
  // Reads the main portion of the `AudioElementParam`.
  DecodedUleb128 param_definition_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(param_definition_type_uleb));
  const auto param_definition_type =
      static_cast<ParamDefinition::ParameterDefinitionType>(
          param_definition_type_uleb);

  switch (param_definition_type) {
    case ParamDefinition::kParameterDefinitionMixGain: {
      return absl::InvalidArgumentError(
          "Mix Gain parameter type is explicitly forbidden for Audio Element "
          "OBUs.");
    }
    case ParamDefinition::kParameterDefinitionReconGain: {
      auto& recon_gain_param_definition =
          param_definition.emplace<ReconGainParamDefinition>(
              ParamDefinition::BaseArgs{}, audio_element_id);
      RETURN_IF_NOT_OK(recon_gain_param_definition.ReadAndValidate(rb));
      return absl::OkStatus();
    }
    case ParamDefinition::kParameterDefinitionDemixing: {
      auto& demixing_param_definition =
          param_definition.emplace<DemixingParamDefinition>(
              ParamDefinition::BaseArgs{});
      RETURN_IF_NOT_OK(demixing_param_definition.ReadAndValidate(rb));
      return absl::OkStatus();
    }
    default:
      auto& extended_param_definition =
          param_definition.emplace<ExtendedParamDefinition>(
              param_definition_type, ParamDefinition::BaseArgs{});
      RETURN_IF_NOT_OK(extended_param_definition.ReadAndValidate(rb));
      return absl::OkStatus();
  }
}

absl::Status ChannelAudioLayerConfig::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(loudspeaker_layout, 4));
  RETURN_IF_NOT_OK(wb.WriteBoolean(output_gain_is_present_flag));
  RETURN_IF_NOT_OK(wb.WriteBoolean(recon_gain_is_present_flag));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_a, 2));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(substream_count, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(coupled_substream_count, 8));

  if (output_gain_is_present_flag) {
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(output_gain_flag, 6));
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_b, 2));
    RETURN_IF_NOT_OK(wb.WriteSigned16(output_gain));
  }

  if (loudspeaker_layout == kLayoutExpanded) {
    RETURN_IF_NOT_OK(ValidateHasValue(expanded_loudspeaker_layout,
                                      "`expanded_loudspeaker_layout`"));
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(*expanded_loudspeaker_layout, 8));
  }

  return absl::OkStatus();
}

absl::Status ChannelAudioLayerConfig::Read(ReadBitBuffer& rb) {
  uint8_t loudspeaker_layout_uint8;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(4, loudspeaker_layout_uint8));
  loudspeaker_layout = static_cast<ChannelAudioLayerConfig::LoudspeakerLayout>(
      loudspeaker_layout_uint8);
  RETURN_IF_NOT_OK(rb.ReadBoolean(output_gain_is_present_flag));
  RETURN_IF_NOT_OK(rb.ReadBoolean(recon_gain_is_present_flag));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(2, reserved_a));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, substream_count));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, coupled_substream_count));

  if (output_gain_is_present_flag) {
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(6, output_gain_flag));
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(2, reserved_b));
    RETURN_IF_NOT_OK(rb.ReadSigned16(output_gain));
  }

  if (loudspeaker_layout == kLayoutExpanded) {
    uint8_t expanded_loudspeaker_layout_uint8;
    RETURN_IF_NOT_OK(
        rb.ReadUnsignedLiteral(8, expanded_loudspeaker_layout_uint8));
    expanded_loudspeaker_layout =
        static_cast<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>(
            expanded_loudspeaker_layout_uint8);
  }

  return absl::OkStatus();
}

absl::Status ScalableChannelLayoutConfig::Validate(
    DecodedUleb128 num_substreams_in_audio_element) const {
  if (GetNumLayers() == 0 || GetNumLayers() > 6) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected `num_layers` in [1, 6]; got ", GetNumLayers()));
  }

  // Determine whether any binaural layouts are found and the total number of
  // substreams.
  DecodedUleb128 cumulative_substream_count = 0;
  bool has_binaural_layout = false;
  for (const auto& layer_config : channel_audio_layer_configs) {
    if (layer_config.loudspeaker_layout ==
        ChannelAudioLayerConfig::kLayoutBinaural) {
      has_binaural_layout = true;
    }

    cumulative_substream_count +=
        static_cast<DecodedUleb128>(layer_config.substream_count);
  }

  if (cumulative_substream_count != num_substreams_in_audio_element) {
    return absl::InvalidArgumentError(
        "Cumulative substream count from all layers is not equal to "
        "the `num_substreams` in the OBU.");
  }

  if (has_binaural_layout && GetNumLayers() != 1) {
    return absl::InvalidArgumentError(
        "There must be exactly 1 layer if there is a binaural layout.");
  }

  return absl::OkStatus();
}

AudioElementObu::AudioElementObu(
    const ObuHeader& header, DecodedUleb128 audio_element_id,
    AudioElementType audio_element_type, uint8_t reserved,
    DecodedUleb128 codec_config_id,
    absl::Span<const DecodedUleb128> audio_substream_ids,
    const AudioElementConfig& config)
    : ObuBase(header, kObuIaAudioElement),
      audio_substream_ids_(audio_substream_ids.begin(),
                           audio_substream_ids.end()),
      config_(config),
      audio_element_id_(audio_element_id),
      audio_element_type_(audio_element_type),
      reserved_(reserved),
      codec_config_id_(codec_config_id) {}

absl::StatusOr<AudioElementObu> AudioElementObu::CreateForScalableChannelLayout(
    const ObuHeader& header, DecodedUleb128 audio_element_id, uint8_t reserved,
    DecodedUleb128 codec_config_id,
    absl::Span<const DecodedUleb128> audio_substream_ids,
    const ScalableChannelLayoutConfig& scalable_channel_layout_config) {
  RETURN_IF_NOT_OK(ValidateUnique(audio_substream_ids.begin(),
                                  audio_substream_ids.end(),
                                  "Audio substream IDs"));
  RETURN_IF_NOT_OK(
      scalable_channel_layout_config.Validate(audio_substream_ids.size()));
  return AudioElementObu(header, audio_element_id, kAudioElementChannelBased,
                         reserved, codec_config_id, audio_substream_ids,
                         scalable_channel_layout_config);
}

absl::StatusOr<AudioElementObu> AudioElementObu::CreateForAmbisonics(
    const ObuHeader& header, DecodedUleb128 audio_element_id, uint8_t reserved,
    DecodedUleb128 codec_config_id,
    absl::Span<const DecodedUleb128> audio_substream_ids,
    const AmbisonicsConfig& ambisonics_config) {
  RETURN_IF_NOT_OK(ValidateUnique(audio_substream_ids.begin(),
                                  audio_substream_ids.end(),
                                  "Audio substream IDs"));
  RETURN_IF_NOT_OK(
      ValidateContainerSizeEqual("audio_substream_ids", audio_substream_ids,
                                 ambisonics_config.GetNumSubstreams()));
  return AudioElementObu(header, audio_element_id, kAudioElementSceneBased,
                         reserved, codec_config_id, audio_substream_ids,
                         ambisonics_config);
}

absl::StatusOr<AudioElementObu> AudioElementObu::CreateForObjects(
    const ObuHeader& header, DecodedUleb128 audio_element_id, uint8_t reserved,
    DecodedUleb128 codec_config_id, const DecodedUleb128 audio_substream_id,
    const ObjectsConfig& objects_config) {
  return AudioElementObu(header, audio_element_id, kAudioElementObjectBased,
                         reserved, codec_config_id, {audio_substream_id},
                         objects_config);
}

absl::StatusOr<AudioElementObu> AudioElementObu::CreateForExtension(
    const ObuHeader& header, DecodedUleb128 audio_element_id,
    AudioElementType audio_element_type, uint8_t reserved,
    DecodedUleb128 codec_config_id,
    absl::Span<const DecodedUleb128> audio_substream_ids,
    absl::Span<const uint8_t> audio_element_config_bytes) {
  RETURN_IF_NOT_OK(ValidateUnique(audio_substream_ids.begin(),
                                  audio_substream_ids.end(),
                                  "Audio substream IDs"));
  ExtensionConfig extension_config = {
      .audio_element_config_bytes = {audio_element_config_bytes.begin(),
                                     audio_element_config_bytes.end()}};
  return AudioElementObu(header, audio_element_id, audio_element_type, reserved,
                         codec_config_id, audio_substream_ids,
                         extension_config);
}

absl::StatusOr<AudioElementObu> AudioElementObu::CreateFromBuffer(
    const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb) {
  AudioElementObu audio_element_obu(header);
  RETURN_IF_NOT_OK(audio_element_obu.ReadAndValidatePayload(payload_size, rb));
  return audio_element_obu;
}

void AudioElementObu::InitializeParams(const DecodedUleb128 num_parameters) {
  audio_element_params_.reserve(static_cast<size_t>(num_parameters));
}

void AudioElementObu::PrintObu() const {
  ABSL_VLOG(1) << "Audio Element OBU:";
  ABSL_VLOG(1) << "  audio_element_id= " << audio_element_id_;
  ABSL_VLOG(1) << "  audio_element_type= " << absl::StrCat(audio_element_type_);
  ABSL_VLOG(1) << "  reserved= " << absl::StrCat(reserved_);
  ABSL_VLOG(1) << "  codec_config_id= " << codec_config_id_;
  ABSL_VLOG(1) << "  num_substreams= " << GetNumSubstreams();
  for (DecodedUleb128 i = 0; i < GetNumSubstreams(); ++i) {
    const auto& substream_id = audio_substream_ids_[i];
    ABSL_VLOG(1) << "  audio_substream_ids[" << i << "]= " << substream_id;
  }
  ABSL_VLOG(1) << "  num_parameters= " << GetNumParameters();
  for (DecodedUleb128 i = 0; i < GetNumParameters(); ++i) {
    ABSL_VLOG(1) << "  params[" << i << "]";
    std::visit([](const auto& param_definition) { param_definition.Print(); },
               audio_element_params_[i].param_definition);
  }
  if (std::holds_alternative<ScalableChannelLayoutConfig>(config_)) {
    LogChannelBased(std::get<ScalableChannelLayoutConfig>(config_));
  } else if (std::holds_alternative<AmbisonicsConfig>(config_)) {
    std::get<AmbisonicsConfig>(config_).Print();
  }
}

absl::Status AudioElementObu::ValidateAndWritePayload(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(ValidateUniqueParamDefinitionType(audio_element_params_));

  RETURN_IF_NOT_OK(wb.WriteUleb128(audio_element_id_));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(audio_element_type_, 3));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_, 5));
  RETURN_IF_NOT_OK(wb.WriteUleb128(codec_config_id_));
  RETURN_IF_NOT_OK(wb.WriteUleb128(GetNumSubstreams()));

  // Loop to write the audio substream IDs portion of the obu.
  for (const auto& audio_substream_id : audio_substream_ids_) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(audio_substream_id));
  }

  RETURN_IF_NOT_OK(ValidateNumParameters(GetNumParameters()));
  RETURN_IF_NOT_OK(wb.WriteUleb128(GetNumParameters()));

  // Loop to write the parameter portion of the obu.
  for (const auto& audio_element_param : audio_element_params_) {
    RETURN_IF_NOT_OK(
        ValidateAndWriteAudioElementParam(audio_element_param, wb));
  }

  // Write the specific `audio_element_type`'s config.
  switch (audio_element_type_) {
    case kAudioElementChannelBased:
      return ValidateAndWriteScalableChannelLayout(
          std::get<ScalableChannelLayoutConfig>(config_), GetNumSubstreams(),
          wb);
    case kAudioElementSceneBased: {
      const auto& ambisonics_config = std::get<AmbisonicsConfig>(config_);
      RETURN_IF_NOT_OK(ValidateEqual<DecodedUleb128>(
          GetNumSubstreams(), ambisonics_config.GetNumSubstreams(),
          "OBU and `AmbisonicsConfig` substream count"));
      return ambisonics_config.ValidateAndWrite(wb);
    }
    case kAudioElementObjectBased:
      return WriteObjectsConfig(std::get<ObjectsConfig>(config_), wb);
    default: {
      const auto& extension_config = std::get<ExtensionConfig>(config_);
      RETURN_IF_NOT_OK(
          wb.WriteUleb128(extension_config.audio_element_config_bytes.size()));
      RETURN_IF_NOT_OK(wb.WriteUint8Span(
          absl::MakeConstSpan(extension_config.audio_element_config_bytes)));

      return absl::OkStatus();
    }
  }
}

absl::Status AudioElementObu::ReadAndValidatePayloadDerived(
    int64_t /*payload_size*/, ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadULeb128(audio_element_id_));
  uint8_t audio_element_type;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(3, audio_element_type));
  audio_element_type_ = static_cast<AudioElementType>(audio_element_type);
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, reserved_));
  RETURN_IF_NOT_OK(rb.ReadULeb128(codec_config_id_));
  DecodedUleb128 num_substreams;
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_substreams));

  // Loop to read the audio substream IDs portion of the obu.
  for (DecodedUleb128 i = 0; i < num_substreams; ++i) {
    DecodedUleb128 audio_substream_id;
    RETURN_IF_NOT_OK(rb.ReadULeb128(audio_substream_id));
    audio_substream_ids_.push_back(audio_substream_id);
  }
  RETURN_IF_NOT_OK(ValidateUnique(audio_substream_ids_.begin(),
                                  audio_substream_ids_.end(),
                                  "Audio substream IDs"));

  DecodedUleb128 num_parameters;
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_parameters));
  RETURN_IF_NOT_OK(ValidateNumParameters(num_parameters));

  // Loop to read the parameter portion of the obu.
  for (DecodedUleb128 i = 0; i < num_parameters; ++i) {
    AudioElementParam audio_element_param;
    RETURN_IF_NOT_OK(
        audio_element_param.ReadAndValidate(audio_element_id_, rb));
    audio_element_params_.push_back(std::move(audio_element_param));
  }

  // Validate that parameter definition types are unique before proceeding
  // to the config-specific parsing (which returns directly from each case).
  RETURN_IF_NOT_OK(ValidateUniqueParamDefinitionType(audio_element_params_));

  // Read the specific `audio_element_type`'s config.
  switch (audio_element_type_) {
    case kAudioElementChannelBased:
      config_ = ScalableChannelLayoutConfig();
      return ReadAndValidateScalableChannelLayout(
          std::get<ScalableChannelLayoutConfig>(config_), GetNumSubstreams(),
          rb);
    case kAudioElementSceneBased: {
      config_ = AmbisonicsConfig{.ambisonics_config =
                                     *AmbisonicsMonoConfig::Create(1, {0})};
      auto& ambisonics_config = std::get<AmbisonicsConfig>(config_);
      RETURN_IF_NOT_OK(ambisonics_config.ReadAndValidate(rb));
      RETURN_IF_NOT_OK(ValidateEqual<DecodedUleb128>(
          GetNumSubstreams(), ambisonics_config.GetNumSubstreams(),
          "OBU and `AmbisonicsConfig` substream count"));
      return absl::OkStatus();
    }
    case kAudioElementObjectBased: {
      auto objects_config = ObjectsConfig::CreateFromBuffer(rb);
      if (!objects_config.ok()) {
        return objects_config.status();
      }
      config_.emplace<ObjectsConfig>(std::move(*objects_config));
      return absl::OkStatus();
    }
    default: {
      ExtensionConfig extension_config;
      DecodedUleb128 audio_element_config_size;
      RETURN_IF_NOT_OK(rb.ReadULeb128(audio_element_config_size));
      for (DecodedUleb128 i = 0; i < audio_element_config_size; ++i) {
        uint8_t config_bytes;
        RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, config_bytes));
        extension_config.audio_element_config_bytes.push_back(config_bytes);
      }

      return absl::OkStatus();
    }
  }
}

}  // namespace iamf_tools
