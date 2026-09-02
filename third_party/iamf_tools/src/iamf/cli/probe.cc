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
#include "iamf/cli/probe.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/descriptor_obu_parser.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

// ---------- Enum to string helpers ----------

std::string ProfileToString(ProfileVersion profile) {
  switch (profile) {
    case ProfileVersion::kIamfSimpleProfile:
      return "simple";
    case ProfileVersion::kIamfBaseProfile:
      return "base";
    case ProfileVersion::kIamfBaseEnhancedProfile:
      return "base_enhanced";
    case ProfileVersion::kIamfBaseAdvancedProfile:
      return "base_advanced";
    case ProfileVersion::kIamfAdvanced1Profile:
      return "advanced1";
    case ProfileVersion::kIamfAdvanced2Profile:
      return "advanced2";
    case ProfileVersion::kIamfReserved255Profile:
      return "reserved_255";
  }
  return absl::StrCat("unknown(", static_cast<int>(profile), ")");
}

std::string CodecIdToString(CodecConfig::CodecId id) {
  switch (id) {
    case CodecConfig::kCodecIdOpus:
      return "Opus";
    case CodecConfig::kCodecIdFlac:
      return "FLAC";
    case CodecConfig::kCodecIdLpcm:
      return "LPCM";
    case CodecConfig::kCodecIdAacLc:
      return "AAC LC";
  }
  char cc[5] = {
      static_cast<char>((id >> 24) & 0xff),
      static_cast<char>((id >> 16) & 0xff),
      static_cast<char>((id >> 8) & 0xff),
      static_cast<char>(id & 0xff),
      0,
  };
  return absl::StrCat("unknown(", cc, ")");
}

std::string AudioElementTypeToString(AudioElementObu::AudioElementType t) {
  switch (t) {
    case AudioElementObu::kAudioElementChannelBased:
      return "channel_based";
    case AudioElementObu::kAudioElementSceneBased:
      return "scene_based";
    case AudioElementObu::kAudioElementObjectBased:
      return "object_based";
    default:
      return absl::StrCat("reserved(", static_cast<int>(t), ")");
  }
}

std::string LoudspeakerLayoutToString(
    ChannelAudioLayerConfig::LoudspeakerLayout layout) {
  switch (layout) {
    case ChannelAudioLayerConfig::kLayoutMono:
      return "Mono";
    case ChannelAudioLayerConfig::kLayoutStereo:
      return "Stereo";
    case ChannelAudioLayerConfig::kLayout5_1_ch:
      return "5.1";
    case ChannelAudioLayerConfig::kLayout5_1_2_ch:
      return "5.1.2";
    case ChannelAudioLayerConfig::kLayout5_1_4_ch:
      return "5.1.4";
    case ChannelAudioLayerConfig::kLayout7_1_ch:
      return "7.1";
    case ChannelAudioLayerConfig::kLayout7_1_2_ch:
      return "7.1.2";
    case ChannelAudioLayerConfig::kLayout7_1_4_ch:
      return "7.1.4";
    case ChannelAudioLayerConfig::kLayout3_1_2_ch:
      return "3.1.2";
    case ChannelAudioLayerConfig::kLayoutBinaural:
      return "Binaural";
    case ChannelAudioLayerConfig::kLayoutExpanded:
      return "Expanded";
    default:
      return absl::StrCat("reserved(", static_cast<int>(layout), ")");
  }
}

std::string ExpandedLayoutToString(
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout layout) {
  using E = ChannelAudioLayerConfig;
  switch (layout) {
    case E::kExpandedLayoutLFE:
      return "LFE";
    case E::kExpandedLayoutStereoS:
      return "StereoS";
    case E::kExpandedLayoutStereoSS:
      return "StereoSS";
    case E::kExpandedLayoutStereoRS:
      return "StereoRS";
    case E::kExpandedLayoutStereoTF:
      return "StereoTF";
    case E::kExpandedLayoutStereoTB:
      return "StereoTB";
    case E::kExpandedLayoutTop4Ch:
      return "Top4Ch";
    case E::kExpandedLayout3_0_ch:
      return "3.0";
    case E::kExpandedLayout9_1_6_ch:
      return "9.1.6";
    case E::kExpandedLayoutStereoF:
      return "StereoF";
    case E::kExpandedLayoutStereoSi:
      return "StereoSi";
    case E::kExpandedLayoutStereoTpSi:
      return "StereoTpSi";
    case E::kExpandedLayoutTop6Ch:
      return "Top6Ch";
    case E::kExpandedLayout10_2_9_3:
      return "10.2.9.3";
    case E::kExpandedLayoutLfePair:
      return "LfePair";
    case E::kExpandedLayoutBottom3Ch:
      return "Bottom3Ch";
    case E::kExpandedLayout7_1_5_4Ch:
      return "7.1.5.4";
    case E::kExpandedLayoutBottom4Ch:
      return "Bottom4Ch";
    case E::kExpandedLayoutTop1Ch:
      return "Top1Ch";
    case E::kExpandedLayoutTop5Ch:
      return "Top5Ch";
    default:
      return absl::StrCat("reserved(", static_cast<int>(layout), ")");
  }
}

std::string SoundSystemToString(
    LoudspeakersSsConventionLayout::SoundSystem ss) {
  switch (ss) {
    case LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0:
      return "Stereo";
    case LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0:
      return "5.1";
    case LoudspeakersSsConventionLayout::kSoundSystemC_2_5_0:
      return "5.1.2";
    case LoudspeakersSsConventionLayout::kSoundSystemD_4_5_0:
      return "5.1.4";
    case LoudspeakersSsConventionLayout::kSoundSystemE_4_5_1:
      return "5+4+1";
    case LoudspeakersSsConventionLayout::kSoundSystemF_3_7_0:
      return "3+7+0";
    case LoudspeakersSsConventionLayout::kSoundSystemG_4_9_0:
      return "9.1.4";
    case LoudspeakersSsConventionLayout::kSoundSystemH_9_10_3:
      return "22.2";
    case LoudspeakersSsConventionLayout::kSoundSystemI_0_7_0:
      return "7.1";
    case LoudspeakersSsConventionLayout::kSoundSystemJ_4_7_0:
      return "7.1.4";
    case LoudspeakersSsConventionLayout::kSoundSystem10_2_7_0:
      return "7.1.2";
    case LoudspeakersSsConventionLayout::kSoundSystem11_2_3_0:
      return "3.1.2";
    case LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0:
      return "Mono";
    case LoudspeakersSsConventionLayout::kSoundSystem13_6_9_0:
      return "9.1.6";
    case LoudspeakersSsConventionLayout::kSoundSystem14_5_7_4:
      return "7.1.5.4";
    default:
      return absl::StrCat("reserved(", static_cast<int>(ss), ")");
  }
}

std::string LayoutTypeToString(Layout::LayoutType t) {
  switch (t) {
    case Layout::kLayoutTypeLoudspeakersSsConvention:
      return "loudspeakers_ss_convention";
    case Layout::kLayoutTypeBinaural:
      return "binaural";
    case Layout::kLayoutTypeReserved0:
      return "reserved_0";
    case Layout::kLayoutTypeReserved1:
      return "reserved_1";
  }
  return absl::StrCat("reserved(", static_cast<int>(t), ")");
}

std::string HeadphonesRenderingModeToString(
    RenderingConfig::HeadphonesRenderingMode m) {
  switch (m) {
    case RenderingConfig::kHeadphonesRenderingModeStereo:
      return "stereo";
    case RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked:
      return "binaural_world_locked";
    case RenderingConfig::kHeadphonesRenderingModeBinauralHeadLocked:
      return "binaural_head_locked";
    case RenderingConfig::kHeadphonesRenderingModeReserved3:
      return "reserved_3";
  }
  return absl::StrCat("reserved(", static_cast<int>(m), ")");
}

std::string BinauralFilterProfileToString(
    RenderingConfig::BinauralFilterProfile p) {
  switch (p) {
    case RenderingConfig::kBinauralFilterProfileAmbient:
      return "ambient";
    case RenderingConfig::kBinauralFilterProfileDirect:
      return "direct";
    case RenderingConfig::kBinauralFilterProfileReverberant:
      return "reverberant";
    case RenderingConfig::kBinauralFilterProfileReserved3:
      return "reserved_3";
  }
  return absl::StrCat("reserved(", static_cast<int>(p), ")");
}

std::string AnchorElementToString(AnchoredLoudnessElement::AnchorElement a) {
  switch (a) {
    case AnchoredLoudnessElement::kAnchorElementUnknown:
      return "unknown";
    case AnchoredLoudnessElement::kAnchorElementDialogue:
      return "dialogue";
    case AnchoredLoudnessElement::kAnchorElementAlbum:
      return "album";
  }
  return absl::StrCat("reserved(", static_cast<int>(a), ")");
}

std::string LpcmSampleFormatToString(
    LpcmDecoderConfig::LpcmFormatFlagsBitmask f) {
  if ((f & LpcmDecoderConfig::kLpcmLittleEndian) != 0) return "little_endian";
  if (f == LpcmDecoderConfig::kLpcmBigEndian) return "big_endian";
  return absl::StrCat("reserved(", static_cast<int>(f), ")");
}

std::string AacSampleFrequencyIndexToString(
    AudioSpecificConfig::SampleFrequencyIndex i) {
  using E = AudioSpecificConfig::SampleFrequencyIndex;
  switch (i) {
    case E::k96000:
      return "96000";
    case E::k88200:
      return "88200";
    case E::k64000:
      return "64000";
    case E::k48000:
      return "48000";
    case E::k44100:
      return "44100";
    case E::k32000:
      return "32000";
    case E::k24000:
      return "24000";
    case E::k22050:
      return "22050";
    case E::k16000:
      return "16000";
    case E::k12000:
      return "12000";
    case E::k11025:
      return "11025";
    case E::k8000:
      return "8000";
    case E::k7350:
      return "7350";
    case E::kReservedA:
      return "reserved_a";
    case E::kReservedB:
      return "reserved_b";
  }
  return absl::StrCat("reserved(", static_cast<int>(i), ")");
}

std::string ParamDefinitionTypeToString(
    ParamDefinition::ParameterDefinitionType t) {
  switch (t) {
    case ParamDefinition::kParameterDefinitionMixGain:
      return "mix_gain";
    case ParamDefinition::kParameterDefinitionDemixing:
      return "demixing";
    case ParamDefinition::kParameterDefinitionReconGain:
      return "recon_gain";
    case ParamDefinition::kParameterDefinitionPolar:
      return "polar";
    case ParamDefinition::kParameterDefinitionCart8:
      return "cart8";
    case ParamDefinition::kParameterDefinitionCart16:
      return "cart16";
    case ParamDefinition::kParameterDefinitionDualPolar:
      return "dual_polar";
    case ParamDefinition::kParameterDefinitionDualCart8:
      return "dual_cart8";
    case ParamDefinition::kParameterDefinitionDualCart16:
      return "dual_cart16";
    default:
      return absl::StrCat("reserved(", static_cast<int>(t), ")");
  }
}

std::string FlacBlockTypeToString(FlacMetaBlockHeader::FlacBlockType t) {
  switch (t) {
    case FlacMetaBlockHeader::kFlacStreamInfo:
      return "StreamInfo";
    case FlacMetaBlockHeader::kFlacPadding:
      return "Padding";
    case FlacMetaBlockHeader::kFlacApplication:
      return "Application";
    case FlacMetaBlockHeader::kFlacSeektable:
      return "Seektable";
    case FlacMetaBlockHeader::kFlacVorbisComment:
      return "VorbisComment";
    case FlacMetaBlockHeader::kFlacCuesheet:
      return "Cuesheet";
    case FlacMetaBlockHeader::kFlacPicture:
      return "Picture";
    case FlacMetaBlockHeader::kFlacInvalid:
      return "Invalid";
    default:
      return absl::StrCat("reserved(", static_cast<int>(t), ")");
  }
}

// Q7.8 fixed-point to float.
float Q7_8ToFloat(int16_t v) { return static_cast<float>(v) / 256.0f; }

// ---------- Codec config builders ----------

OpusDecoderConfigReport BuildOpusReport(const OpusDecoderConfig& c) {
  return {
      .version = c.version_,
      .output_channel_count = c.output_channel_count_,
      .pre_skip = c.pre_skip_,
      .input_sample_rate = c.input_sample_rate_,
      .output_gain_q7_8 = c.output_gain_,
      .output_gain = Q7_8ToFloat(c.output_gain_),
      .mapping_family = c.mapping_family_,
  };
}

LpcmDecoderConfigReport BuildLpcmReport(const LpcmDecoderConfig& c) {
  return {
      .sample_format = LpcmSampleFormatToString(c.sample_format_flags_bitmask_),
      .sample_format_raw = static_cast<uint8_t>(c.sample_format_flags_bitmask_),
      .sample_size = c.sample_size_,
      .sample_rate = c.sample_rate_,
  };
}

AacDecoderConfigReport BuildAacReport(const AacDecoderConfig& c) {
  const auto& asc = c.decoder_specific_info_.audio_specific_config;
  return {
      .object_type_indication = c.object_type_indication_,
      .stream_type = c.stream_type_,
      .upstream = c.upstream_,
      .reserved = c.reserved_,
      .buffer_size_db = c.buffer_size_db_,
      .max_bitrate = c.max_bitrate_,
      .average_bit_rate = c.average_bit_rate_,
      .audio_object_type = asc.audio_object_type_,
      .sample_frequency_index =
          AacSampleFrequencyIndexToString(asc.sample_frequency_index_),
      .sample_frequency_index_raw =
          static_cast<uint8_t>(asc.sample_frequency_index_),
      .channel_configuration = asc.channel_configuration_,
  };
}

FlacDecoderConfigReport BuildFlacReport(const FlacDecoderConfig& c) {
  FlacDecoderConfigReport r;
  r.metadata_blocks.reserve(c.metadata_blocks_.size());
  for (const auto& block : c.metadata_blocks_) {
    FlacMetaBlockReport mr;
    mr.block_type = FlacBlockTypeToString(block.header.block_type);
    mr.block_type_raw = static_cast<uint8_t>(block.header.block_type);
    if (const auto* info =
            std::get_if<FlacMetaBlockStreamInfo>(&block.payload)) {
      mr.is_stream_info = true;
      mr.minimum_block_size = info->minimum_block_size;
      mr.maximum_block_size = info->maximum_block_size;
      mr.minimum_frame_size = info->minimum_frame_size;
      mr.maximum_frame_size = info->maximum_frame_size;
      mr.sample_rate = info->sample_rate;
      mr.number_of_channels = info->number_of_channels;
      mr.bits_per_sample = info->bits_per_sample;
      mr.total_samples_in_stream = info->total_samples_in_stream;
    }
    r.metadata_blocks.push_back(std::move(mr));
  }
  return r;
}

CodecConfigReport BuildCodecConfigReport(uint32_t id,
                                         const CodecConfigObu& obu) {
  const CodecConfig& cc = obu.GetCodecConfig();
  CodecConfigReport r;
  r.id = id;
  r.codec_id = CodecIdToString(cc.codec_id);
  r.codec_id_raw = static_cast<uint32_t>(cc.codec_id);
  r.num_samples_per_frame = obu.GetNumSamplesPerFrame();
  r.audio_roll_distance = cc.audio_roll_distance;
  r.output_sample_rate = obu.GetOutputSampleRate();
  r.input_sample_rate = obu.GetInputSampleRate();
  r.bit_depth = static_cast<int>(obu.GetBitDepthToMeasureLoudness());

  if (const auto* opus = std::get_if<OpusDecoderConfig>(&cc.decoder_config)) {
    r.opus = BuildOpusReport(*opus);
  } else if (const auto* lpcm =
                 std::get_if<LpcmDecoderConfig>(&cc.decoder_config)) {
    r.lpcm = BuildLpcmReport(*lpcm);
  } else if (const auto* aac =
                 std::get_if<AacDecoderConfig>(&cc.decoder_config)) {
    r.aac = BuildAacReport(*aac);
  } else if (const auto* flac =
                 std::get_if<FlacDecoderConfig>(&cc.decoder_config)) {
    r.flac = BuildFlacReport(*flac);
  }
  return r;
}

// ---------- Audio element builders ----------

ChannelAudioLayerReport BuildLayerReport(const ChannelAudioLayerConfig& layer) {
  ChannelAudioLayerReport r;
  r.loudspeaker_layout = LoudspeakerLayoutToString(layer.loudspeaker_layout);
  r.loudspeaker_layout_raw = static_cast<uint8_t>(layer.loudspeaker_layout);
  if (layer.expanded_loudspeaker_layout.has_value()) {
    r.expanded_loudspeaker_layout =
        ExpandedLayoutToString(*layer.expanded_loudspeaker_layout);
    r.expanded_loudspeaker_layout_raw =
        static_cast<uint8_t>(*layer.expanded_loudspeaker_layout);
  }
  r.output_gain_is_present_flag = layer.output_gain_is_present_flag;
  r.recon_gain_is_present_flag = layer.recon_gain_is_present_flag;
  r.substream_count = layer.substream_count;
  r.coupled_substream_count = layer.coupled_substream_count;
  r.output_gain_flag = layer.output_gain_flag;
  r.output_gain_q7_8 = layer.output_gain;
  r.output_gain = Q7_8ToFloat(layer.output_gain);
  return r;
}

AmbisonicsReport BuildAmbisonicsReport(const AmbisonicsConfig& ambi) {
  AmbisonicsReport r;
  const auto mode = ambi.GetAmbisonicsMode();
  switch (mode) {
    case AmbisonicsConfig::kAmbisonicsModeMono:
      r.mode = "mono";
      break;
    case AmbisonicsConfig::kAmbisonicsModeProjection:
      r.mode = "projection";
      break;
    default:
      r.mode = absl::StrCat("reserved(", static_cast<int>(mode), ")");
  }
  r.mode_raw = static_cast<uint8_t>(mode);
  int output_channels = 0;
  if (const auto* mono =
          std::get_if<AmbisonicsMonoConfig>(&ambi.ambisonics_config)) {
    AmbisonicsMonoReport m;
    m.output_channel_count = mono->GetOutputChannelCount();
    m.substream_count = mono->GetSubstreamCount();
    const auto channel_mapping = mono->GetChannelMappingView();
    m.channel_mapping.assign(channel_mapping.begin(), channel_mapping.end());
    output_channels = m.output_channel_count;
    r.mono = std::move(m);
  } else if (const auto* proj = std::get_if<AmbisonicsProjectionConfig>(
                 &ambi.ambisonics_config)) {
    AmbisonicsProjectionReport p;
    p.output_channel_count = proj->GetOutputChannelCount();
    p.substream_count = proj->GetSubstreamCount();
    p.coupled_substream_count = proj->GetCoupledSubstreamCount();
    const auto demixing_matrix = proj->GetDemixingMatrixView();
    p.demixing_matrix.assign(demixing_matrix.begin(), demixing_matrix.end());
    output_channels = p.output_channel_count;
    r.projection = std::move(p);
  }
  if (output_channels > 0) {
    int order = 0;
    while ((order + 1) * (order + 1) < output_channels) ++order;
    if ((order + 1) * (order + 1) == output_channels) {
      r.order = order;
    }
  }
  return r;
}

AudioElementParamReport BuildParamReport(const AudioElementParam& p) {
  AudioElementParamReport r;
  r.param_type = ParamDefinitionTypeToString(p.GetType());
  r.param_type_raw = static_cast<uint32_t>(p.GetType());
  std::visit(
      [&](const auto& def) {
        r.parameter_id = def.GetParameterId();
        r.parameter_rate = def.GetParameterRate();
        r.param_definition_mode =
            static_cast<uint8_t>(def.GetParamDefinitionMode());
        r.duration = def.GetDuration();
        r.constant_subblock_duration = def.GetConstantSubblockDuration();
      },
      p.param_definition);
  return r;
}

AudioElementReport BuildAudioElementReport(const AudioElementObu& obu) {
  AudioElementReport r;
  r.id = obu.GetAudioElementId();
  r.type = AudioElementTypeToString(obu.GetAudioElementType());
  r.type_raw = static_cast<uint8_t>(obu.GetAudioElementType());
  r.codec_config_id = obu.GetCodecConfigId();
  r.num_substreams = obu.GetNumSubstreams();
  r.reserved = 0;  // AudioElementObu keeps `reserved_` private.

  r.substream_ids.reserve(obu.audio_substream_ids_.size());
  for (auto id : obu.audio_substream_ids_) {
    r.substream_ids.push_back(static_cast<uint32_t>(id));
  }

  r.params.reserve(obu.audio_element_params_.size());
  for (const auto& p : obu.audio_element_params_) {
    r.params.push_back(BuildParamReport(p));
  }

  if (obu.GetAudioElementType() == AudioElementObu::kAudioElementChannelBased) {
    if (const auto* scalable =
            std::get_if<ScalableChannelLayoutConfig>(&obu.config_)) {
      r.scalable_layers.reserve(scalable->channel_audio_layer_configs.size());
      // The top-most layer's channel count is the sum across layers: each
      // coupled substream carries two channels, each non-coupled one.
      uint32_t num_channels = 0;
      for (const auto& layer : scalable->channel_audio_layer_configs) {
        r.scalable_layers.push_back(BuildLayerReport(layer));
        num_channels += 2u * layer.coupled_substream_count +
                        (layer.substream_count - layer.coupled_substream_count);
      }
      if (!scalable->channel_audio_layer_configs.empty()) {
        const auto& top = scalable->channel_audio_layer_configs.back();
        r.channel_layout = LoudspeakerLayoutToString(top.loudspeaker_layout);
        r.channel_layout_raw = static_cast<uint8_t>(top.loudspeaker_layout);
        if (top.expanded_loudspeaker_layout.has_value()) {
          r.expanded_channel_layout_raw =
              static_cast<uint8_t>(*top.expanded_loudspeaker_layout);
        }
        r.num_channels = num_channels;
      }
    }
  } else if (obu.GetAudioElementType() ==
             AudioElementObu::kAudioElementSceneBased) {
    if (const auto* ambi = std::get_if<AmbisonicsConfig>(&obu.config_)) {
      auto report = BuildAmbisonicsReport(*ambi);
      if (report.mono.has_value()) {
        r.ambisonics_channels = report.mono->output_channel_count;
      } else if (report.projection.has_value()) {
        r.ambisonics_channels = report.projection->output_channel_count;
      }
      if (r.ambisonics_channels.has_value()) {
        r.num_channels = static_cast<uint32_t>(*r.ambisonics_channels);
      }
      r.ambisonics_order = report.order;
      r.ambisonics = std::move(report);
    }
  }

  return r;
}

// ---------- Mix presentation builders ----------

ParamDefinitionReport BuildParamDefReport(const ParamDefinition& d) {
  return {
      .parameter_id = d.GetParameterId(),
      .parameter_rate = d.GetParameterRate(),
      .param_definition_mode = static_cast<uint8_t>(d.GetParamDefinitionMode()),
      .duration = d.GetDuration(),
      .constant_subblock_duration = d.GetConstantSubblockDuration(),
  };
}

MixGainParamDefinitionReport BuildMixGainReport(
    const MixGainParamDefinition& d) {
  MixGainParamDefinitionReport r;
  r.param_definition = BuildParamDefReport(d);
  r.default_mix_gain_q7_8 = d.default_mix_gain_.GetQ7_8();
  r.default_mix_gain = d.default_mix_gain_.GetFloatingPoint();
  return r;
}

RenderingConfigReport BuildRenderingConfigReport(const RenderingConfig& rc) {
  return {
      .headphones_rendering_mode =
          HeadphonesRenderingModeToString(rc.headphones_rendering_mode),
      .headphones_rendering_mode_raw =
          static_cast<uint8_t>(rc.headphones_rendering_mode),
      .binaural_filter_profile =
          BinauralFilterProfileToString(rc.binaural_filter_profile),
      .binaural_filter_profile_raw =
          static_cast<uint8_t>(rc.binaural_filter_profile),
  };
}

SubMixAudioElementReport BuildSubMixAudioElementReport(
    const SubMixAudioElement& a) {
  SubMixAudioElementReport r;
  r.audio_element_id = a.audio_element_id;
  r.localized_element_annotations = a.localized_element_annotations;
  r.rendering_config = BuildRenderingConfigReport(a.rendering_config);
  r.element_mix_gain = BuildMixGainReport(a.element_mix_gain);
  return r;
}

LayoutReport BuildLayoutReport(const Layout& layout) {
  LayoutReport r;
  r.layout_type = LayoutTypeToString(layout.layout_type);
  r.layout_type_raw = static_cast<uint8_t>(layout.layout_type);
  if (layout.layout_type == Layout::kLayoutTypeLoudspeakersSsConvention) {
    if (const auto* ss = std::get_if<LoudspeakersSsConventionLayout>(
            &layout.specific_layout)) {
      r.sound_system = SoundSystemToString(ss->sound_system);
      r.sound_system_raw = static_cast<uint8_t>(ss->sound_system);
    }
  }
  return r;
}

LoudnessInfoReport BuildLoudnessReport(const LoudnessInfo& loud) {
  LoudnessInfoReport r;
  r.info_type = loud.info_type;
  r.true_peak_present = (loud.info_type & LoudnessInfo::kTruePeak) != 0;
  r.anchored_loudness_present =
      (loud.info_type & LoudnessInfo::kAnchoredLoudness) != 0;
  r.has_layout_extension =
      (loud.info_type & LoudnessInfo::kAnyLayoutExtension) != 0;
  r.integrated_loudness = Q7_8ToFloat(loud.integrated_loudness);
  r.digital_peak = Q7_8ToFloat(loud.digital_peak);
  if (r.true_peak_present) {
    r.true_peak = Q7_8ToFloat(loud.true_peak);
  }
  if (r.anchored_loudness_present) {
    r.anchored_loudness_elements.reserve(
        loud.anchored_loudness.anchor_elements.size());
    for (const auto& e : loud.anchored_loudness.anchor_elements) {
      r.anchored_loudness_elements.push_back({
          .anchor_element = AnchorElementToString(e.anchor_element),
          .anchor_element_raw = static_cast<uint8_t>(e.anchor_element),
          .anchored_loudness_q7_8 = e.anchored_loudness,
          .anchored_loudness = Q7_8ToFloat(e.anchored_loudness),
      });
    }
  }
  return r;
}

MixPresentationLayoutReport BuildMpLayoutReport(
    const MixPresentationLayout& mpl) {
  return {
      .loudness_layout = BuildLayoutReport(mpl.loudness_layout),
      .loudness = BuildLoudnessReport(mpl.loudness),
  };
}

SubMixReport BuildSubMixReport(const MixPresentationSubMix& sm) {
  SubMixReport r;
  r.audio_elements.reserve(sm.audio_elements.size());
  for (const auto& a : sm.audio_elements) {
    r.audio_elements.push_back(BuildSubMixAudioElementReport(a));
  }
  r.output_mix_gain = BuildMixGainReport(sm.output_mix_gain);
  r.layouts.reserve(sm.layouts.size());
  for (const auto& l : sm.layouts) {
    r.layouts.push_back(BuildMpLayoutReport(l));
  }
  return r;
}

MixPresentationReport BuildMixPresentationReport(
    const MixPresentationObu& obu) {
  MixPresentationReport r;
  r.id = obu.GetMixPresentationId();

  const auto langs = obu.GetAnnotationsLanguage();
  const auto labels = obu.GetLocalizedPresentationAnnotations();
  if (langs.size() != labels.size()) {
    ABSL_LOG(WARNING) << "MixPresentation " << r.id
                      << ": annotation language/label size mismatch ("
                      << langs.size() << " vs " << labels.size()
                      << "); reporting only the overlapping prefix.";
  }
  r.count_label = static_cast<uint32_t>(std::min(langs.size(), labels.size()));
  r.annotations.reserve(r.count_label);
  for (size_t i = 0; i < r.count_label; ++i) {
    r.annotations.emplace_back(langs[i], labels[i]);
  }

  r.num_sub_mixes = static_cast<uint32_t>(std::min<size_t>(
      obu.sub_mixes_.size(), std::numeric_limits<uint32_t>::max()));
  r.sub_mixes.reserve(obu.sub_mixes_.size());
  for (const auto& sm : obu.sub_mixes_) {
    r.sub_mixes.push_back(BuildSubMixReport(sm));
  }

  if (obu.mix_presentation_tags_.has_value()) {
    r.tags.reserve(obu.mix_presentation_tags_->tags.size());
    for (const auto& t : obu.mix_presentation_tags_->tags) {
      r.tags.push_back({t.tag_name, t.tag_value});
    }
  }

  // Backwards-compatible summary fields from first sub-mix / first layout.
  if (!r.sub_mixes.empty()) {
    const auto& first = r.sub_mixes.front();
    r.layouts.reserve(first.layouts.size());
    for (const auto& mpl : first.layouts) {
      r.layouts.push_back(mpl.loudness_layout.sound_system.value_or(
          mpl.loudness_layout.layout_type));
    }
    if (!first.layouts.empty()) {
      const auto& loud = first.layouts.front().loudness;
      r.integrated_loudness_lkfs = loud.integrated_loudness;
      r.digital_peak_dbfs = loud.digital_peak;
      r.true_peak_dbfs = loud.true_peak;
    }
  }
  return r;
}

// ---------- Temporal unit scan helpers ----------

std::string DMixPModeToString(DemixingInfoParameterData::DMixPMode m) {
  switch (m) {
    case DemixingInfoParameterData::kDMixPMode1:
      return "mode1";
    case DemixingInfoParameterData::kDMixPMode2:
      return "mode2";
    case DemixingInfoParameterData::kDMixPMode3:
      return "mode3";
    case DemixingInfoParameterData::kDMixPMode1_n:
      return "mode1_n";
    case DemixingInfoParameterData::kDMixPMode2_n:
      return "mode2_n";
    case DemixingInfoParameterData::kDMixPMode3_n:
      return "mode3_n";
    case DemixingInfoParameterData::kDMixPModeReserved1:
      return "reserved_1";
    case DemixingInfoParameterData::kDMixPModeReserved2:
      return "reserved_2";
  }
  return absl::StrCat("reserved(", static_cast<int>(m), ")");
}

std::string AnimationTypeToString(AnimationType t) {
  switch (t) {
    case AnimationType::kStep:
      return "step";
    case AnimationType::kLinear:
      return "linear";
    case AnimationType::kBezier:
      return "bezier";
    case AnimationType::kInterLinear:
      return "inter_linear";
    case AnimationType::kInterBezier:
      return "inter_bezier";
  }
  return absl::StrCat("reserved(", static_cast<int>(t), ")");
}

MixGainAnimationReport BuildMixGainAnimationReport(
    const MixGainParameterData& data) {
  MixGainAnimationReport r;
  r.animation_type = AnimationTypeToString(data.GetAnimationType());
  r.animation_type_raw = static_cast<uint32_t>(data.GetAnimationType());
  const auto set_start = [&r](int16_t v) {
    r.start_point_value_q7_8 = v;
    r.start_point_value = Q7_8ToFloat(v);
  };
  const auto set_end = [&r](int16_t v) {
    r.end_point_value_q7_8 = v;
    r.end_point_value = Q7_8ToFloat(v);
  };
  const auto& anim = data.param_data;
  if (anim.animation_type() == AnimationType::kStep) {
    set_start(*anim.start_point_value());
  } else if (anim.animation_type() == AnimationType::kLinear) {
    set_start(*anim.start_point_value());
    set_end(*anim.end_point_value());
  } else if (anim.animation_type() == AnimationType::kBezier) {
    set_start(*anim.start_point_value());
    set_end(*anim.end_point_value());
    r.control_point_value_q7_8 = *anim.control_point_value();
    r.control_point_value = Q7_8ToFloat(*anim.control_point_value());
    r.control_point_relative_time = *anim.control_point_relative_time();
  }
  return r;
}

ReconGainInfoReport BuildReconGainReport(const ReconGainInfoParameterData& d) {
  ReconGainInfoReport r;
  // `layer_index` is a uint8_t; beyond 255 layers we would silently wrap.
  // The IAMF spec caps scalable channel layers well below this, so clamp
  // defensively rather than risk reporting a bogus index.
  const size_t n = std::min<size_t>(d.recon_gain_elements.size(), 256);
  r.layers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const auto& element = d.recon_gain_elements[i];
    if (!element.has_value()) continue;
    ReconGainForLayerReport layer;
    layer.layer_index = static_cast<uint8_t>(i);
    layer.recon_gain_flag = element->recon_gain_flag;
    // Emit only the bytes for channels whose flag bit is set.
    for (int bit = 0; bit < 12; ++bit) {
      if ((element->recon_gain_flag >> bit) & 1u) {
        layer.recon_gain.push_back(element->recon_gain[bit]);
      }
    }
    r.layers.push_back(std::move(layer));
  }
  return r;
}

ParameterBlockReport BuildParameterBlockReport(const ParameterBlockObu& obu,
                                               absl::string_view param_type,
                                               uint32_t param_type_raw,
                                               uint64_t start_timestamp,
                                               uint64_t end_timestamp) {
  ParameterBlockReport r;
  r.parameter_id = obu.parameter_id_;
  r.param_type = std::string(param_type);
  r.param_type_raw = param_type_raw;
  r.start_timestamp = start_timestamp;
  r.end_timestamp = end_timestamp;
  r.duration = obu.GetDuration();
  r.constant_subblock_duration = obu.GetConstantSubblockDuration();
  r.subblocks.reserve(obu.subblocks_.size());
  for (size_t i = 0; i < obu.subblocks_.size(); ++i) {
    ParameterSubblockReport sub;
    const auto duration_or = obu.GetSubblockDuration(static_cast<int>(i));
    if (duration_or.ok()) sub.subblock_duration = *duration_or;
    const auto& subblock = obu.subblocks_[i];
    if (subblock == nullptr) {
      r.subblocks.push_back(std::move(sub));
      continue;
    }
    const ParameterData* data = subblock.get();
    if (const auto* mg = dynamic_cast<const MixGainParameterData*>(data)) {
      sub.mix_gain = BuildMixGainAnimationReport(*mg);
    } else if (const auto* dx =
                   dynamic_cast<const DemixingInfoParameterData*>(data)) {
      DemixingInfoReport dr;
      dr.dmixp_mode = DMixPModeToString(dx->dmixp_mode);
      dr.dmixp_mode_raw = static_cast<uint8_t>(dx->dmixp_mode);
      dr.reserved = dx->reserved;
      sub.demixing = std::move(dr);
    } else if (const auto* rg =
                   dynamic_cast<const ReconGainInfoParameterData*>(data)) {
      sub.recon_gain = BuildReconGainReport(*rg);
    }
    r.subblocks.push_back(std::move(sub));
  }
  return r;
}

// Walks temporal units from the buffer's current position and fills `out`.
// The `read_bit_buffer` should be positioned immediately after the last
// descriptor OBU. Does not return an error; temporal-unit parsing is best-
// effort, and malformed OBUs are skipped in the spirit of the spec. Sets
// `out.stopped_reason` to indicate why the walk ended.
void ScanTemporalUnits(
    const DescriptorObus& descriptors,
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    const ProbeOptions& options, ReadBitBuffer& read_bit_buffer,
    TemporalUnitScanReport& out) {
  const bool collect_details = options.scan_mode == ScanMode::kScanFull;
  // Per-substream frame size + codec-config id, so we can tally samples and
  // label substreams in the report.
  absl::flat_hash_map<DecodedUleb128, uint32_t> substream_to_frame_size;
  absl::flat_hash_map<DecodedUleb128, uint32_t> substream_to_codec_config_id;
  for (const auto& [audio_element_id, element_with_data] :
       descriptors.audio_elements) {
    for (const auto substream_id : element_with_data.obu.audio_substream_ids_) {
      if (element_with_data.codec_config != nullptr) {
        substream_to_frame_size[substream_id] =
            element_with_data.codec_config->GetNumSamplesPerFrame();
        substream_to_codec_config_id[substream_id] =
            element_with_data.obu.GetCodecConfigId();
      }
    }
  }

  // Parameter-id -> type, so we can label blocks in the report.
  absl::flat_hash_map<DecodedUleb128, ParamDefinition::ParameterDefinitionType>
      parameter_id_to_type;
  for (const auto& [parameter_id, variant] : param_definition_variants) {
    // Copy the structured binding into a plain local before capturing it in the
    // lambda: capturing a structured binding directly is only well-formed in
    // C++20 and is rejected by some toolchains (e.g. Apple Clang in
    // Xcode 15.4).
    const DecodedUleb128 id = parameter_id;
    std::visit(
        [&](const auto& def) { parameter_id_to_type[id] = def.GetType(); },
        variant);
  }

  // Track per-substream totals + per-parameter timestamps.
  absl::flat_hash_map<DecodedUleb128, AudioFrameSummaryReport> per_substream;
  absl::flat_hash_map<DecodedUleb128, uint64_t> per_parameter_timestamp;

  const int64_t scan_start_bits = read_bit_buffer.Tell();

  // Per-TU index state. A TU begins at the byte offset of its first OBU and
  // ends when either (a) a temporal delimiter OBU marks a boundary, or (b) an
  // audio frame arrives for a substream that has already appeared in the
  // current TU (each substream emits exactly one frame per TU in IAMF). The
  // cumulative sum of finalised `num_samples` feeds each successive entry's
  // `start_timestamp`.
  struct PendingTu {
    int64_t start_byte_offset = -1;
    uint64_t num_samples = 0;
    uint32_t trim_at_start = 0;
    uint32_t trim_at_end = 0;
    absl::flat_hash_set<DecodedUleb128> substreams_seen;
  };
  PendingTu pending_tu;
  uint64_t cumulative_timestamp = 0;
  uint32_t finalized_tu_count = 0;
  auto note_tu_start = [&](size_t byte_offset) {
    if (pending_tu.start_byte_offset < 0) {
      pending_tu.start_byte_offset = static_cast<int64_t>(byte_offset);
    }
  };
  auto finalize_tu = [&]() {
    if (pending_tu.start_byte_offset < 0) return;
    // Skip pending state with no audio frames; that's parameter-block-only
    // scaffolding (seen only in test-shaped inputs), not a real TU. Keeps
    // `temporal_units.size()` consistent with `temporal_unit_count`.
    if (pending_tu.substreams_seen.empty()) {
      pending_tu = PendingTu{};
      return;
    }
    ++finalized_tu_count;
    if (collect_details) {
      TemporalUnitEntry entry;
      entry.start_timestamp = cumulative_timestamp;
      entry.num_samples = pending_tu.num_samples;
      entry.byte_offset_from_scan_start =
          static_cast<size_t>(pending_tu.start_byte_offset);
      entry.samples_to_trim_at_start = pending_tu.trim_at_start;
      entry.samples_to_trim_at_end = pending_tu.trim_at_end;
      out.temporal_units.push_back(entry);
    }
    cumulative_timestamp += pending_tu.num_samples;
    pending_tu = PendingTu{};
  };

  // Each iteration must consume bytes, so the loop is naturally bounded by
  // the input size; `max_scan_obu_iterations` additionally caps latency on
  // adversarial inputs of tiny reserved OBUs.
  uint64_t iterations = 0;
  for (;;) {
    if (++iterations > options.max_scan_obu_iterations) {
      out.stopped_reason = "scan_budget_exceeded";
      break;
    }
    if (options.should_continue != nullptr &&
        !options.should_continue(ScanProgress{
            .bytes_consumed = static_cast<size_t>(
                (read_bit_buffer.Tell() - scan_start_bits) / 8),
            .temporal_unit_count = finalized_tu_count,
        })) {
      out.stopped_reason = "cancelled";
      break;
    }
    const int64_t position_before_header = read_bit_buffer.Tell();
    const int64_t bytes_available_before_header =
        read_bit_buffer.NumBytesAvailable();
    auto header_metadata =
        ObuHeader::PeekObuTypeAndTotalObuSize(read_bit_buffer);
    if (!header_metadata.ok()) {
      if (header_metadata.status().code() ==
          absl::StatusCode::kResourceExhausted) {
        out.stopped_reason =
            bytes_available_before_header == 0 ? "eof" : "truncated";
      } else {
        out.stopped_reason = "malformed";
      }
      break;
    }
    if (read_bit_buffer.NumBytesAvailable() < header_metadata->total_obu_size) {
      out.stopped_reason = "truncated";
      break;
    }

    ObuHeader header;
    int64_t payload_size = 0;
    const auto header_status =
        header.ReadAndValidate(read_bit_buffer, payload_size);
    if (!header_status.ok()) {
      // Malformed header; skip this OBU and continue.
      if (!read_bit_buffer.Seek(position_before_header).ok() ||
          !read_bit_buffer.IgnoreBytes(header_metadata->total_obu_size).ok()) {
        out.stopped_reason = "malformed";
        break;
      }
      continue;
    }

    bool stop_walk = false;
    bool handled = false;
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
      case kObuIaAudioFrameId17: {
        const size_t frame_start_offset =
            static_cast<size_t>((position_before_header - scan_start_bits) / 8);
        // Duration accounting needs only the substream id (implicit in the
        // OBU type, or one leading ULEB128 for `kObuIaAudioFrame`), so the
        // codec payload is skipped without parsing or copying it.
        DecodedUleb128 substream_id = 0;
        int64_t bytes_to_skip = payload_size;
        if (header.obu_type == kObuIaAudioFrame) {
          int8_t encoded_uleb128_size = 0;
          if (!read_bit_buffer.ReadULeb128(substream_id, encoded_uleb128_size)
                   .ok() ||
              payload_size < encoded_uleb128_size) {
            ++out.audio_frame_parse_errors;
            break;
          }
          bytes_to_skip -= encoded_uleb128_size;
        } else {
          substream_id = header.obu_type - kObuIaAudioFrameId0;
        }
        if (!read_bit_buffer.IgnoreBytes(bytes_to_skip).ok()) {
          ++out.audio_frame_parse_errors;
          break;
        }
        // TU boundary: repeated substream within the pending TU signals that
        // a new TU has begun. Finalise the pending TU before attributing this
        // frame to the new one.
        if (pending_tu.substreams_seen.contains(substream_id)) {
          finalize_tu();
        }
        note_tu_start(frame_start_offset);
        // First audio frame in a new TU supplies the canonical trim values.
        // Subsequent frames in the same TU carry identical values in
        // conformant streams; we trust the first-frame reading.
        if (pending_tu.substreams_seen.empty()) {
          pending_tu.trim_at_start =
              static_cast<uint32_t>(header.num_samples_to_trim_at_start);
          pending_tu.trim_at_end =
              static_cast<uint32_t>(header.num_samples_to_trim_at_end);
        }
        pending_tu.substreams_seen.insert(substream_id);
        uint32_t frame_size = 0;
        if (auto it = substream_to_frame_size.find(substream_id);
            it != substream_to_frame_size.end()) {
          frame_size = it->second;
        }
        pending_tu.num_samples =
            std::max(pending_tu.num_samples, uint64_t{frame_size});
        auto& summary = per_substream[substream_id];
        summary.substream_id = substream_id;
        if (auto it = substream_to_codec_config_id.find(substream_id);
            it != substream_to_codec_config_id.end()) {
          summary.codec_config_id = it->second;
        }
        ++summary.frame_count;
        summary.total_samples += frame_size;
        ++out.audio_frame_count;
        handled = true;
        break;
      }
      case kObuIaParameterBlock: {
        const size_t block_start_offset =
            static_cast<size_t>((position_before_header - scan_start_bits) / 8);
        auto peek_id = ParameterBlockObu::PeekParameterId(read_bit_buffer);
        if (!peek_id.ok()) {
          ++out.parameter_block_parse_errors;
          break;
        }
        note_tu_start(block_start_offset);
        auto def_it = param_definition_variants.find(*peek_id);
        if (def_it == param_definition_variants.end()) {
          // Descriptors didn't declare this parameter id; we can't parse the
          // body, so count it as a skipped block.
          ++out.parameter_block_parse_errors;
          break;
        }
        const ParamDefinition* param_definition = std::visit(
            [](const auto& def) -> const ParamDefinition* { return &def; },
            def_it->second);
        if (param_definition == nullptr) {
          ++out.parameter_block_parse_errors;
          break;
        }
        if (!collect_details) {
          // Block contents are only surfaced in the detailed report; skip
          // the body unparsed (the peek above left the buffer untouched).
          if (!read_bit_buffer.IgnoreBytes(payload_size).ok()) {
            ++out.parameter_block_parse_errors;
            break;
          }
          ++out.parameter_block_count;
          handled = true;
          break;
        }
        auto block_or = ParameterBlockObu::CreateFromBuffer(
            header, payload_size, *param_definition, read_bit_buffer);
        if (!block_or.ok()) {
          ++out.parameter_block_parse_errors;
          break;
        }
        auto type_it = parameter_id_to_type.find(*peek_id);
        const bool type_known = type_it != parameter_id_to_type.end();
        const std::string param_type =
            type_known ? ParamDefinitionTypeToString(type_it->second)
                       : "unknown";
        const uint32_t param_type_raw =
            type_known ? static_cast<uint32_t>(type_it->second) : 0;
        const uint64_t start_ts = per_parameter_timestamp[*peek_id];
        const uint64_t end_ts = start_ts + (*block_or)->GetDuration();
        per_parameter_timestamp[*peek_id] = end_ts;
        out.parameter_blocks.push_back(BuildParameterBlockReport(
            **block_or, param_type, param_type_raw, start_ts, end_ts));
        ++out.parameter_block_count;
        handled = true;
        break;
      }
      case kObuIaTemporalDelimiter: {
        const size_t td_start_offset =
            static_cast<size_t>((position_before_header - scan_start_bits) / 8);
        // A temporal delimiter marks the start of a new TU (IAMF 6.1.7):
        // finalise whatever pending frames preceded it, then begin the next
        // TU at the TD's byte offset.
        finalize_tu();
        note_tu_start(td_start_offset);
        ++out.temporal_delimiter_count;
        // Header parsed and the OBU size fit in the buffer, so a skip failure
        // here would indicate a bit-buffer inconsistency rather than missing
        // bytes.
        if (!read_bit_buffer.IgnoreBytes(payload_size).ok()) {
          out.stopped_reason = "malformed";
          stop_walk = true;
        }
        handled = true;
        break;
      }
      case kObuIaSequenceHeader:
        if (!header.obu_redundant_copy) {
          // Rewind so the caller sees the start of the next IA sequence.
          (void)read_bit_buffer.Seek(position_before_header);
          out.stopped_reason = "next_ia_sequence";
          stop_walk = true;
          handled = true;
          break;
        }
        [[fallthrough]];
      case kObuIaCodecConfig:
      case kObuIaAudioElement:
      case kObuIaMixPresentation:
        // Skip redundant copies.
        (void)read_bit_buffer.IgnoreBytes(payload_size);
        handled = true;
        break;
      default:
        // Reserved OBU; skip.
        (void)read_bit_buffer.IgnoreBytes(payload_size);
        handled = true;
        break;
    }

    if (!handled) {
      // A parse failed inside one of the cases; fall back to the skip pattern.
      // The header peeked cleanly and total_obu_size bytes were confirmed
      // available above, so a seek/skip failure here is a buffer-level
      // inconsistency rather than truncation.
      if (!read_bit_buffer.Seek(position_before_header).ok() ||
          !read_bit_buffer.IgnoreBytes(header_metadata->total_obu_size).ok()) {
        out.stopped_reason = "malformed";
        break;
      }
    }

    if (stop_walk) break;

    // Defensive: every iteration must consume bytes. Catches buffer-state
    // bugs that would otherwise loop forever.
    if (read_bit_buffer.Tell() <= position_before_header) {
      out.stopped_reason = "malformed";
      break;
    }
  }

  // Flush the last in-flight TU, if any. The scan loop only finalises on
  // boundaries it observes in-stream; the final TU has no successor to trigger
  // that path.
  finalize_tu();

  // In IAMF each temporal unit emits exactly one audio frame per substream, so
  // the max frame count across substreams equals the TU count.
  uint32_t max_frames = 0;
  for (const auto& [_, summary] : per_substream) {
    max_frames = std::max(max_frames, summary.frame_count);
  }
  out.temporal_unit_count = std::max(max_frames, out.temporal_delimiter_count);

  // Flatten per_substream map, sorted by substream id for stable output.
  std::vector<DecodedUleb128> substream_ids;
  substream_ids.reserve(per_substream.size());
  for (const auto& [id, _] : per_substream) substream_ids.push_back(id);
  std::sort(substream_ids.begin(), substream_ids.end());
  out.audio_frames_by_substream.reserve(substream_ids.size());
  uint64_t max_samples = 0;
  for (auto id : substream_ids) {
    out.audio_frames_by_substream.push_back(per_substream[id]);
    max_samples = std::max(max_samples, per_substream[id].total_samples);
  }
  if (max_samples > 0) {
    out.total_samples = max_samples;
    // Pick an output sample rate from the first codec config.
    if (!descriptors.codec_config_obus.empty()) {
      const auto& [unused_id, codec_cfg] =
          *descriptors.codec_config_obus.begin();
      const uint32_t rate = codec_cfg.GetOutputSampleRate();
      if (rate > 0) {
        out.output_sample_rate = rate;
        out.duration_seconds =
            static_cast<double>(max_samples) / static_cast<double>(rate);
      }
    }
  }

  out.bytes_consumed =
      static_cast<size_t>((read_bit_buffer.Tell() - scan_start_bits) / 8);
  if (out.stopped_reason.empty()) out.stopped_reason = "eof";
}

// Shared body of `Probe` and `ProbeFile`: parses descriptors (and optionally
// scans temporal units) from an already-positioned buffer.
absl::StatusOr<ProbeReport> ProbeFromBuffer(ReadBitBuffer& read_bit_buffer,
                                            ProbeOptions options) {
  bool insufficient_data = false;
  auto descriptors_or = DescriptorObuParser::ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/false, read_bit_buffer, insufficient_data);
  if (!descriptors_or.ok()) {
    if (insufficient_data) {
      // Distinct code so incremental callers can read more bytes and retry
      // without string-matching the message.
      return absl::ResourceExhaustedError(
          absl::StrCat("Insufficient data to parse descriptor OBUs: ",
                       descriptors_or.status().message()));
    }
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse descriptor OBUs: ",
                     descriptors_or.status().message()));
  }
  const DescriptorObus& descriptors = *descriptors_or;

  ProbeReport report;
  report.descriptor_bytes_consumed =
      static_cast<size_t>(read_bit_buffer.Tell() / 8);

  report.primary_profile =
      ProfileToString(descriptors.ia_sequence_header.GetPrimaryProfile());
  report.primary_profile_raw =
      static_cast<uint8_t>(descriptors.ia_sequence_header.GetPrimaryProfile());
  report.additional_profile =
      ProfileToString(descriptors.ia_sequence_header.GetAdditionalProfile());
  report.additional_profile_raw = static_cast<uint8_t>(
      descriptors.ia_sequence_header.GetAdditionalProfile());

  report.codec_configs.reserve(descriptors.codec_config_obus.size());
  for (const auto& [id, obu] : descriptors.codec_config_obus) {
    report.codec_configs.push_back(BuildCodecConfigReport(id, obu));
  }

  report.audio_elements.reserve(descriptors.audio_elements.size());
  for (const auto& [id, element_with_data] : descriptors.audio_elements) {
    report.audio_elements.push_back(
        BuildAudioElementReport(element_with_data.obu));
  }

  report.mix_presentations.reserve(descriptors.mix_presentation_obus.size());
  for (const auto& mp : descriptors.mix_presentation_obus) {
    report.mix_presentations.push_back(BuildMixPresentationReport(mp));
  }

  // FLAC STREAMINFO can carry the stream's total sample count, yielding a
  // duration estimate without walking any temporal units.
  for (const auto& cc : report.codec_configs) {
    if (!cc.flac.has_value()) continue;
    for (const auto& block : cc.flac->metadata_blocks) {
      if (!block.is_stream_info || block.total_samples_in_stream == 0) {
        continue;
      }
      report.descriptor_total_samples = block.total_samples_in_stream;
      const uint32_t rate =
          block.sample_rate > 0 ? block.sample_rate : cc.output_sample_rate;
      if (rate > 0) {
        report.descriptor_duration_seconds =
            static_cast<double>(block.total_samples_in_stream) /
            static_cast<double>(rate);
      }
      break;
    }
    if (report.descriptor_total_samples.has_value()) break;
  }

  if (options.scan_mode != ScanMode::kDescriptorsOnly) {
    absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
        param_definition_variants;
    const auto collect_status = CollectAndValidateParamDefinitions(
        descriptors.audio_elements, descriptors.mix_presentation_obus,
        param_definition_variants);
    if (!collect_status.ok()) {
      ABSL_LOG(WARNING) << "Parameter definitions validation failed; "
                           "continuing with best-effort scan: "
                        << collect_status;
    }
    TemporalUnitScanReport scan;
    ScanTemporalUnits(descriptors, param_definition_variants, options,
                      read_bit_buffer, scan);
    report.temporal_unit_scan = std::move(scan);
  }

  return report;
}

}  // namespace

absl::StatusOr<ProbeReport> Probe(absl::Span<const uint8_t> data,
                                  ProbeOptions options) {
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  if (read_bit_buffer == nullptr) {
    return absl::InternalError("Failed to create read bit buffer");
  }
  return ProbeFromBuffer(*read_bit_buffer, options);
}

absl::StatusOr<ProbeReport> ProbeFile(const std::string& path,
                                      ProbeOptions options) {
  // The window only bounds single-shot loads; multi-byte reads refill it on
  // demand, so the size is a memory/I/O trade-off, not a limit on OBU or
  // descriptor sizes.
  constexpr int64_t kFileWindowBytes = 64 * 1024;
  auto read_bit_buffer =
      FileBasedReadBitBuffer::CreateFromFilePath(kFileWindowBytes, path);
  if (read_bit_buffer == nullptr) {
    return absl::NotFoundError(
        absl::StrCat("Failed to open file for probing: ", path));
  }
  auto report = ProbeFromBuffer(*read_bit_buffer, options);
  if (absl::IsResourceExhausted(report.status())) {
    // The whole file is visible, so "more bytes needed" cannot be satisfied
    // by retrying: surface it as invalid (truncated) input instead.
    return absl::InvalidArgumentError(report.status().message());
  }
  return report;
}

}  // namespace iamf_tools
