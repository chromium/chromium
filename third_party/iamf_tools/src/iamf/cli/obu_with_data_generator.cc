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

#include "iamf/cli/obu_with_data_generator.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using absl::InvalidArgumentError;
using absl::StrCat;

using enum ChannelLabel::Label;

namespace {

absl::Status LoudspeakerLayoutToChannels(
    const ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    ChannelNumbers& channels) {
  switch (loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    case kLayoutMono:
      channels = {.surround = 1, .lfe = 0, .height = 0, .bottom = 0};
      break;
    case kLayoutStereo:
      channels = {.surround = 2, .lfe = 0, .height = 0, .bottom = 0};
      break;
    case kLayout5_1_ch:
      channels = {.surround = 5, .lfe = 1, .height = 0, .bottom = 0};
      break;
    case kLayout5_1_2_ch:
      channels = {.surround = 5, .lfe = 1, .height = 2, .bottom = 0};
      break;
    case kLayout5_1_4_ch:
      channels = {.surround = 5, .lfe = 1, .height = 4, .bottom = 0};
      break;
    case kLayout7_1_ch:
      channels = {.surround = 7, .lfe = 1, .height = 0, .bottom = 0};
      break;
    case kLayout7_1_2_ch:
      channels = {.surround = 7, .lfe = 1, .height = 2, .bottom = 0};
      break;
    case kLayout7_1_4_ch:
      channels = {.surround = 7, .lfe = 1, .height = 4, .bottom = 0};
      break;
    case kLayout3_1_2_ch:
      channels = {.surround = 3, .lfe = 1, .height = 2, .bottom = 0};
      break;
    case kLayoutBinaural:
      channels = {.surround = 2, .lfe = 0, .height = 0, .bottom = 0};
      break;
    default:
      return InvalidArgumentError(
          StrCat("Unknown loudspeaker_layout= ", loudspeaker_layout));
  }
  return absl::OkStatus();
}

// For the Base Channel Group (BCG). This is the first layer of a scalable audio
// element.
// https://aomediacodec.github.io/iamf/#scalablechannelaudio-channelgroupformat
absl::Status CollectBaseChannelGroupLabels(
    const ChannelNumbers& layer_channels,
    std::list<ChannelLabel::Label>* coupled_substream_labels,
    std::list<ChannelLabel::Label>* non_coupled_substream_labels) {
  switch (layer_channels.surround) {
    case 1:
      non_coupled_substream_labels->push_back(kMono);
      break;
    case 2:
      coupled_substream_labels->push_back(kL2);
      coupled_substream_labels->push_back(kR2);
      break;
    case 3:
      coupled_substream_labels->push_back(kL3);
      coupled_substream_labels->push_back(kR3);
      non_coupled_substream_labels->push_back(kCentre);
      break;
    case 5:
      coupled_substream_labels->push_back(kL5);
      coupled_substream_labels->push_back(kR5);
      coupled_substream_labels->push_back(kLs5);
      coupled_substream_labels->push_back(kRs5);
      non_coupled_substream_labels->push_back(kCentre);
      break;
    case 7:
      coupled_substream_labels->push_back(kL7);
      coupled_substream_labels->push_back(kR7);
      coupled_substream_labels->push_back(kLss7);
      coupled_substream_labels->push_back(kRss7);
      coupled_substream_labels->push_back(kLrs7);
      coupled_substream_labels->push_back(kRrs7);
      non_coupled_substream_labels->push_back(kCentre);
      break;
    default:
      ABSL_LOG(ERROR) << "Unsupported number of surround channels: "
                      << layer_channels.surround;
      return InvalidArgumentError(
          StrCat("Unsupported number of surround channels: ",
                 layer_channels.surround));
  }
  switch (layer_channels.height) {
    case 0:
      // Not adding anything.
      break;
    case 2:
      if (layer_channels.surround == 3) {
        coupled_substream_labels->push_back(kLtf3);
        coupled_substream_labels->push_back(kRtf3);
      } else {
        coupled_substream_labels->push_back(kLtf2);
        coupled_substream_labels->push_back(kRtf2);
      }
      break;
    case 4:
      coupled_substream_labels->push_back(kLtf4);
      coupled_substream_labels->push_back(kRtf4);
      coupled_substream_labels->push_back(kLtb4);
      coupled_substream_labels->push_back(kRtb4);
      break;
    default:
      ABSL_LOG(ERROR) << "Unsupported number of height channels: "
                      << layer_channels.height;
      return InvalidArgumentError(StrCat(
          "Unsupported number of height channels: ", layer_channels.height));
  }
  switch (layer_channels.lfe) {
    case 0:
      // Not adding anything.
      break;
    case 1:
      non_coupled_substream_labels->push_back(kLFE);
      break;
    default:
      return InvalidArgumentError(
          StrCat("Unsupported number of LFE channels: ", layer_channels.lfe));
  }

  return absl::OkStatus();
}

absl::Status CollectChannelLayersAndLabelsForExpandedLoudspeakerLayout(
    int layer_index,
    std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
        expanded_loudspeaker_layout,
    ChannelNumbers& channel_numbers,
    std::list<ChannelLabel::Label>& coupled_substream_labels,
    std::list<ChannelLabel::Label>& non_coupled_substream_labels) {
  if (layer_index != 0) {
    return absl::InvalidArgumentError(
        "Expanded layout is only permitted when there is a single layer.");
  }
  RETURN_IF_NOT_OK(ValidateHasValue(expanded_loudspeaker_layout,
                                    "Expanded layout is required."));

  switch (*expanded_loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
    case kExpandedLayoutLFE:
      channel_numbers = {.surround = 0, .lfe = 1, .height = 0, .bottom = 0};
      non_coupled_substream_labels = {kLFE};
      break;
    case kExpandedLayoutStereoS:
      channel_numbers = {.surround = 2, .lfe = 0, .height = 0, .bottom = 0};
      coupled_substream_labels = {kLs5, kRs5};
      break;
    case kExpandedLayoutStereoSS:
      channel_numbers = {.surround = 2, .lfe = 0, .height = 0, .bottom = 0};
      coupled_substream_labels = {kLss7, kRss7};
      break;
    case kExpandedLayoutStereoRS:
      channel_numbers = {.surround = 2, .lfe = 0, .height = 0, .bottom = 0};
      coupled_substream_labels = {kLrs7, kRrs7};
      break;
    case kExpandedLayoutStereoTF:
      channel_numbers = {.surround = 0, .lfe = 0, .height = 2, .bottom = 0};
      coupled_substream_labels = {kLtf4, kRtf4};
      break;
    case kExpandedLayoutStereoTB:
      channel_numbers = {.surround = 0, .lfe = 0, .height = 2, .bottom = 0};
      coupled_substream_labels = {kLtb4, kRtb4};
      break;
    case kExpandedLayoutTop4Ch:
      channel_numbers = {.surround = 0, .lfe = 0, .height = 4, .bottom = 0};
      coupled_substream_labels = {kLtf4, kRtf4, kLtb4, kRtb4};
      break;
    case kExpandedLayout3_0_ch:
      channel_numbers = {.surround = 3, .lfe = 0, .height = 0, .bottom = 0};
      coupled_substream_labels = {kL7, kR7};
      non_coupled_substream_labels = {kCentre};
      break;
    case kExpandedLayout9_1_6_ch:
      channel_numbers = {.surround = 9, .lfe = 1, .height = 6, .bottom = 0};
      coupled_substream_labels = {kFLc,   kFRc,   kFL,   kFR,   kSiL,
                                  kSiR,   kBL,    kBR,   kTpFL, kTpFR,
                                  kTpSiL, kTpSiR, kTpBL, kTpBR};
      non_coupled_substream_labels = {kFC, kLFE};
      break;
    case kExpandedLayoutStereoF:
      channel_numbers = {.surround = 2, .lfe = 0, .height = 0, .bottom = 0};
      coupled_substream_labels = {kFL, kFR};
      break;
    case kExpandedLayoutStereoSi:
      channel_numbers = {.surround = 2, .lfe = 0, .height = 0, .bottom = 0};
      coupled_substream_labels = {kSiL, kSiR};
      break;
    case kExpandedLayoutStereoTpSi:
      channel_numbers = {.surround = 0, .lfe = 0, .height = 2, .bottom = 0};
      coupled_substream_labels = {kTpSiL, kTpSiR};
      break;
    case kExpandedLayoutTop6Ch:
      channel_numbers = {.surround = 0, .lfe = 0, .height = 6, .bottom = 0};
      coupled_substream_labels = {kTpFL, kTpFR, kTpSiL, kTpSiR, kTpBL, kTpBR};
      break;
    case kExpandedLayout10_2_9_3:
      channel_numbers = {.surround = 10, .lfe = 2, .height = 9, .bottom = 3};
      coupled_substream_labels = {kFLc,  kFRc,  kFL,   kFR,   kSiL,   kSiR,
                                  kBL,   kBR,   kTpFL, kTpFR, kTpSiL, kTpSiR,
                                  kTpBL, kTpBR, kBtFL, kBtFR};
      non_coupled_substream_labels = {kFC,   kBC,   kTpFC, kTpC,
                                      kTpBC, kBtFC, kLFE,  kLFE2};
      break;
    case kExpandedLayoutLfePair:
      channel_numbers = {.surround = 0, .lfe = 2, .height = 0, .bottom = 0};
      non_coupled_substream_labels = {kLFE, kLFE2};
      break;
    case kExpandedLayoutBottom3Ch:
      channel_numbers = {.surround = 0, .lfe = 0, .height = 0, .bottom = 3};
      coupled_substream_labels = {kBtFL, kBtFR};
      non_coupled_substream_labels = {kBtFC};
      break;
    case kExpandedLayout7_1_5_4Ch:
      channel_numbers = {.surround = 7, .lfe = 1, .height = 5, .bottom = 4};
      coupled_substream_labels = {kL7,   kR7,   kLss7, kRss7, kLrs7,
                                  kRrs7, kLtf4, kRtf4, kLtb4, kRtb4,
                                  kBtFL, kBtFR, kBtBL, kBtBR};
      non_coupled_substream_labels = {kCentre, kTpC, kLFE};
      break;
    case kExpandedLayoutBottom4Ch:
      channel_numbers = {.surround = 0, .lfe = 0, .height = 0, .bottom = 4};
      coupled_substream_labels = {kBtFL, kBtFR, kBtBL, kBtBR};
      break;
    case kExpandedLayoutTop1Ch:
      channel_numbers = {.surround = 0, .lfe = 0, .height = 1, .bottom = 0};
      non_coupled_substream_labels = {kTpC};
      break;
    case kExpandedLayoutTop5Ch:
      channel_numbers = {.surround = 0, .lfe = 0, .height = 5, .bottom = 0};
      coupled_substream_labels = {kLtf4, kRtf4, kLtb4, kRtb4};
      non_coupled_substream_labels = {kTpC};
      break;
    case kExpandedLayoutReserved20:
    case kExpandedLayoutReserved255:
      break;
    default:
      return absl::InvalidArgumentError(
          StrCat("Unsupported expanded loudspeaker layout= ",
                 *expanded_loudspeaker_layout));
  }

  ABSL_LOG(INFO) << "Layer[" << layer_index << "]:";
  LogChannelNumbers("  layer_channels", channel_numbers);

  return absl::OkStatus();
}

// For the Demixed Channel Groups (DCG). This all layers after the first layer
// in a scalable audio element.
// https://aomediacodec.github.io/iamf/#scalablechannelaudio-channelgroupformat
absl::Status CollectDemixedChannelGroupLabels(
    const ChannelNumbers& accumulated_channels,
    const ChannelNumbers& layer_channels,
    std::list<ChannelLabel::Label>* coupled_substream_labels,
    std::list<ChannelLabel::Label>* non_coupled_substream_labels) {
  bool push_l2_in_the_end = false;
  for (int surround = accumulated_channels.surround + 1;
       surround <= layer_channels.surround; surround++) {
    switch (surround) {
      case 2:
        // This is the special case where layer 1 is Mono and layer 2 is
        // Stereo. According to the Spec 3.7.2
        // (https://aomediacodec.github.io/iamf/#syntax-scalable-channel-layout-config):
        // "The Centre (or Front Centre) channel comes first and is followed by
        // the LFE (or LFE1) channel, and then the L channel.". Save pushing
        // kL2 till the end.
        push_l2_in_the_end = true;
        break;
      case 3:
        non_coupled_substream_labels->push_back(kCentre);
        break;
      case 5:
        coupled_substream_labels->push_back(kL5);
        coupled_substream_labels->push_back(kR5);
        break;
      case 7:
        coupled_substream_labels->push_back(kLss7);
        coupled_substream_labels->push_back(kRss7);
        break;
      default:
        if (surround > 7) {
          return InvalidArgumentError(
              StrCat("Unsupported number of surround channels: ", surround));
        }
        break;
    }
  }

  if (layer_channels.height > accumulated_channels.height) {
    if (accumulated_channels.height == 0) {
      if (layer_channels.height == 4) {
        coupled_substream_labels->push_back(kLtf4);
        coupled_substream_labels->push_back(kRtf4);
        coupled_substream_labels->push_back(kLtb4);
        coupled_substream_labels->push_back(kRtb4);
      } else if (layer_channels.height == 2) {
        if (layer_channels.surround == 3) {
          coupled_substream_labels->push_back(kLtf3);
          coupled_substream_labels->push_back(kRtf3);
        } else {
          coupled_substream_labels->push_back(kLtf2);
          coupled_substream_labels->push_back(kRtf2);
        }
      } else {
        return InvalidArgumentError(StrCat(
            "Unsupported number of height channels: ", layer_channels.height));
      }
    } else if (accumulated_channels.height == 2) {
      coupled_substream_labels->push_back(kLtf4);
      coupled_substream_labels->push_back(kRtf4);
    } else {
      return InvalidArgumentError(
          absl::StrCat("Unsupported number of height channels: ",
                       accumulated_channels.height));
    }
  }

  if (layer_channels.lfe > accumulated_channels.lfe) {
    if (layer_channels.lfe == 1) {
      non_coupled_substream_labels->push_back(kLFE);
    } else {
      return InvalidArgumentError(
          StrCat("Unsupported number of LFE channels: ", layer_channels.lfe));
    }
  }

  if (push_l2_in_the_end) {
    non_coupled_substream_labels->push_back(kL2);
  }

  return absl::OkStatus();
}

absl::Status AddSubstreamLabels(
    const std::list<ChannelLabel::Label>& coupled_substream_labels,
    const std::list<ChannelLabel::Label>& non_coupled_substream_labels,
    const std::vector<DecodedUleb128>& substream_ids,
    SubstreamIdLabelsMap& substream_id_to_labels, int& substream_index) {
  ABSL_CHECK_EQ(coupled_substream_labels.size() % 2, size_t{0});
  // Determine how many substream IDs will be used below. This helps prevent
  // indexing `substream_ids` out of bounds.
  const auto substreams_to_add =
      coupled_substream_labels.size() / 2 + non_coupled_substream_labels.size();
  if (substream_index + substreams_to_add > substream_ids.size()) {
    return absl::OutOfRangeError(
        absl::StrCat("Too few substream IDs are present to assign all labels. "
                     "substream_ids.size()= ",
                     substream_ids.size()));
  }

  // First add coupled substream labels, two at a time.
  for (auto iter = coupled_substream_labels.begin();
       iter != coupled_substream_labels.end() &&
       static_cast<size_t>(substream_index) < substream_ids.size();) {
    const auto substream_id = substream_ids[substream_index++];
    auto& labels_for_substream_id = substream_id_to_labels[substream_id];
    const auto first_label = *iter++;
    const auto second_label = *iter++;

    labels_for_substream_id.push_back(first_label);
    labels_for_substream_id.push_back(second_label);
    ABSL_VLOG(1) << "  substream_id_to_labels[" << substream_id
                 << "]: " << first_label << "/" << second_label;
  }

  // Then add non-coupled substream labels.
  for (auto iter = non_coupled_substream_labels.begin();
       iter != non_coupled_substream_labels.end();) {
    const auto substream_id = substream_ids[substream_index++];
    substream_id_to_labels[substream_id].push_back(*iter++);
    ABSL_VLOG(1) << "  substream_id_to_labels[" << substream_id
                 << "]: " << substream_id_to_labels[substream_id].back();
  }
  return absl::OkStatus();
}

absl::Status ValidateSubstreamCounts(
    const std::list<ChannelLabel::Label>& coupled_substream_labels,
    const std::list<ChannelLabel::Label>& non_coupled_substream_labels,
    const ChannelAudioLayerConfig& layer_config) {
  const auto num_required_coupled_channels =
      static_cast<uint32_t>(coupled_substream_labels.size()) / 2;
  const auto num_required_non_coupled_channels =
      static_cast<uint32_t>(non_coupled_substream_labels.size());
  ABSL_VLOG(1) << "num_required_coupled_channels = "
               << num_required_coupled_channels;
  ABSL_VLOG(1) << "num_required_non_coupled_channels= "
               << num_required_non_coupled_channels;

  const auto coupled_substream_count_in_obu =
      static_cast<uint32_t>(layer_config.coupled_substream_count);
  const auto substream_count_in_obu =
      static_cast<uint32_t>(layer_config.substream_count);
  if (coupled_substream_count_in_obu != num_required_coupled_channels) {
    return InvalidArgumentError(StrCat(
        "Coupled substream count different from the required number. In OBU: ",
        coupled_substream_count_in_obu,
        " vs expected: ", num_required_coupled_channels));
  }

  // The sum of coupled and non-coupled channels must be the same as
  // the `substream_count` recorded in the OBU.
  if (substream_count_in_obu !=
      (num_required_non_coupled_channels + num_required_coupled_channels)) {
    return InvalidArgumentError(StrCat(
        "Substream count different from the #non-coupled substreams. In OBU: ",
        substream_count_in_obu, " vs expected: ",
        num_required_non_coupled_channels + num_required_coupled_channels));
  }

  return absl::OkStatus();
}

bool OutputGainApplies(const uint8_t output_gain_flag,
                       ChannelLabel::Label label) {
  switch (label) {
    case kMono:
    case kL2:
    case kL3:
      return output_gain_flag & (1 << 5);
    case kR2:
    case kR3:
      return output_gain_flag & (1 << 4);
    case kLs5:
      return output_gain_flag & (1 << 3);
    case kRs5:
      return output_gain_flag & (1 << 2);
    case kLtf2:
    case kLtf3:
      return output_gain_flag & (1 << 1);
    case kRtf2:
    case kRtf3:
      return output_gain_flag & 1;
    default:
      return false;
  }
}

absl::Status FinalizeAmbisonicsMonoConfig(
    const AudioElementObu& audio_element_obu,
    const AmbisonicsMonoConfig& mono_config,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  // Fill `substream_id_to_labels`. `channel_mapping` encodes the mapping of
  // Ambisonics Channel Number (ACN) to substream index.
  for (size_t ambisonics_channel_number = 0;
       ambisonics_channel_number < mono_config.GetChannelMappingView().size();
       ++ambisonics_channel_number) {
    const uint8_t obu_substream_index =
        mono_config.GetChannelMappingView()[ambisonics_channel_number];
    if (obu_substream_index ==
        AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber) {
      ABSL_VLOG(1) << "Detected mixed-order ambisonics with  A"
                   << ambisonics_channel_number << " dropped.";
      continue;
    }
    const DecodedUleb128 substream_id =
        audio_element_obu.audio_substream_ids_[obu_substream_index];

    // Add the associated ACN to the labels associated with that substream.
    const auto ambisonics_label =
        ChannelLabel::AmbisonicsChannelNumberToLabel(ambisonics_channel_number);
    if (!ambisonics_label.ok()) {
      return ambisonics_label.status();
    }
    substream_id_to_labels[substream_id].push_back(*ambisonics_label);
  }
  return absl::OkStatus();
}

absl::Status FinalizeAmbisonicsProjectionConfig(
    const AudioElementObu& audio_element_obu,
    const AmbisonicsProjectionConfig& projection_config,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  if (audio_element_obu.GetNumSubstreams() !=
      static_cast<uint32_t>(projection_config.GetSubstreamCount())) {
    return InvalidArgumentError(
        StrCat("`num_substreams` different from `substream_count`: (",
               audio_element_obu.GetNumSubstreams(), " vs ",
               projection_config.GetSubstreamCount(), ")"));
  }

  // For projection mode, assume coupled substreams (using 2 channels) come
  // first and are followed by non-coupled substreams (using 1 channel each).
  for (int i = 0; i < static_cast<int>(audio_element_obu.GetNumSubstreams());
       ++i) {
    const std::list<int> ambisonic_channel_numbers =
        i < projection_config.GetCoupledSubstreamCount()
            ? std::list<int>{2 * i, 2 * i + 1}
            : std::list<int>{projection_config.GetCoupledSubstreamCount() + i};
    for (const auto ambisonic_channel_number : ambisonic_channel_numbers) {
      const auto ambisonics_label =
          ChannelLabel::AmbisonicsChannelNumberToLabel(
              ambisonic_channel_number);
      if (!ambisonics_label.ok()) {
        return ambisonics_label.status();
      }
      substream_id_to_labels[audio_element_obu.audio_substream_ids_[i]]
          .push_back(*ambisonics_label);
    }
  }
  return absl::OkStatus();
}

absl::Status CollectChannelLayersAndLabelsForLoudspeakerLayout(
    int layer_index,
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    const ChannelNumbers& accumulated_channels, ChannelNumbers& layer_channels,
    std::list<ChannelLabel::Label>& coupled_substream_labels,
    std::list<ChannelLabel::Label>& non_coupled_substream_labels) {
  // Figure out the `ChannelNumber` representation of ChannelGroup #i, i.e.
  // the additional channels presented in this layer.
  RETURN_IF_NOT_OK(
      LoudspeakerLayoutToChannels(loudspeaker_layout, layer_channels));

  // Channel number in each group can only grow or stay the same.
  if (layer_channels.surround < accumulated_channels.surround ||
      layer_channels.lfe < accumulated_channels.lfe ||
      layer_channels.height < accumulated_channels.height) {
    LogChannelNumbers("From", accumulated_channels);
    LogChannelNumbers("To", layer_channels);
    return InvalidArgumentError(
        StrCat("At least one channel number decreased from "
               "accumulated_channels to layer_channels"));
  }

  ABSL_VLOG(1) << "Layer[" << layer_index << "]:";
  LogChannelNumbers("  layer_channels", layer_channels);
  LogChannelNumbers("  accumulated_channels", accumulated_channels);

  if (layer_index == 0) {
    return CollectBaseChannelGroupLabels(layer_channels,
                                         &coupled_substream_labels,
                                         &non_coupled_substream_labels);
  } else {
    return CollectDemixedChannelGroupLabels(
        accumulated_channels, layer_channels, &coupled_substream_labels,
        &non_coupled_substream_labels);
  }
}

struct LayerSubstreamLabels {
  ChannelNumbers layer_channels = {0, 0, 0, 0};
  std::list<ChannelLabel::Label> coupled_substream_labels;
  std::list<ChannelLabel::Label> non_coupled_substream_labels;
};

absl::StatusOr<std::vector<LayerSubstreamLabels>>
CollectSubstreamLabelsForLayers(const ScalableChannelLayoutConfig& config) {
  ChannelNumbers accumulated_channels = {0, 0, 0, 0};
  std::vector<LayerSubstreamLabels> layer_labels(config.GetNumLayers());
  for (size_t i = 0; i < config.GetNumLayers(); ++i) {
    const auto& layer_config = config.channel_audio_layer_configs[i];
    if (layer_config.loudspeaker_layout ==
        ChannelAudioLayerConfig::kLayoutExpanded) {
      RETURN_IF_NOT_OK(
          CollectChannelLayersAndLabelsForExpandedLoudspeakerLayout(
              i, layer_config.expanded_loudspeaker_layout,
              layer_labels[i].layer_channels,
              layer_labels[i].coupled_substream_labels,
              layer_labels[i].non_coupled_substream_labels));
    } else {
      RETURN_IF_NOT_OK(CollectChannelLayersAndLabelsForLoudspeakerLayout(
          i, layer_config.loudspeaker_layout, accumulated_channels,
          layer_labels[i].layer_channels,
          layer_labels[i].coupled_substream_labels,
          layer_labels[i].non_coupled_substream_labels));
    }
    accumulated_channels = layer_labels[i].layer_channels;
  }
  return layer_labels;
}

}  // namespace

absl::StatusOr<AudioElementWithData>
ObuWithDataGenerator::GenerateAudioElementWithData(
    const DescriptorObus::CodecConfigsById& codec_config_obus,
    const AudioElementObu& audio_element_obu) {
  SubstreamIdLabelsMap substream_id_to_labels;
  LabelGainMap label_to_output_gain;
  std::vector<ChannelNumbers> channel_numbers_for_layers;
  if (audio_element_obu.GetAudioElementType() ==
      AudioElementObu::AudioElementType::kAudioElementChannelBased) {
    if (!std::holds_alternative<ScalableChannelLayoutConfig>(
            audio_element_obu.config_)) {
      return absl::InvalidArgumentError(
          "Audio Element OBU signals it holds a scalable channel layout "
          "config, but one is not present.");
    }

    RETURN_IF_NOT_OK(ObuWithDataGenerator::FinalizeScalableChannelLayoutConfig(
        audio_element_obu.audio_substream_ids_,
        std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_),
        substream_id_to_labels, label_to_output_gain,
        channel_numbers_for_layers));
  }
  if (audio_element_obu.GetAudioElementType() ==
      AudioElementObu::AudioElementType::kAudioElementSceneBased) {
    RETURN_IF_NOT_OK(ObuWithDataGenerator::FinalizeAmbisonicsConfig(
        audio_element_obu, substream_id_to_labels));
  }
  auto iter = codec_config_obus.find(audio_element_obu.GetCodecConfigId());
  if (iter == codec_config_obus.end()) {
    return absl::InvalidArgumentError(
        "codec_config_obus does not contain codec_config_id");
  }
  return AudioElementWithData{
      .obu = std::move(audio_element_obu),
      .codec_config = &iter->second,
      .substream_id_to_labels = substream_id_to_labels,
      .label_to_output_gain = label_to_output_gain,
      .channel_numbers_for_layers = channel_numbers_for_layers};
}

absl::StatusOr<AudioFrameWithData>
ObuWithDataGenerator::GenerateAudioFrameWithData(
    const AudioElementWithData& audio_element_with_data,
    const AudioFrameObu& audio_frame_obu,
    GlobalTimingModule& global_timing_module,
    ParametersManager& parameters_manager) {
  const auto audio_substream_id = audio_frame_obu.GetSubstreamId();
  const auto audio_element_id = audio_element_with_data.obu.GetAudioElementId();

  // Make sure we have the correct audio element.
  if (!audio_element_with_data.substream_id_to_labels.contains(
          audio_substream_id)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Audio element with ID= ", audio_element_id,
        " does not contain a substream with ID= ", audio_substream_id));
  }

  const uint32_t duration =
      audio_element_with_data.codec_config->GetNumSamplesPerFrame();

  // Get the timestamps and demixing and recon-gain parameters to fill in
  // `AudioFrameWithData`.
  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;
  RETURN_IF_NOT_OK(global_timing_module.GetNextAudioFrameTimestamps(
      audio_substream_id, duration, start_timestamp, end_timestamp));
  DownMixingParams down_mixing_params;
  RETURN_IF_NOT_OK(parameters_manager.GetDownMixingParameters(
      audio_element_id, down_mixing_params));
  ReconGainInfoParameterData recon_gain_info_parameter_data;
  RETURN_IF_NOT_OK(parameters_manager.GetReconGainInfoParameterData(
      audio_element_id,
      audio_element_with_data.channel_numbers_for_layers.size(),
      recon_gain_info_parameter_data));

  return AudioFrameWithData{
      .obu = std::move(audio_frame_obu),
      .start_timestamp = start_timestamp,
      .end_timestamp = end_timestamp,
      .encoded_samples = std::nullopt,  // The encoded samples cannot be
                                        // derived from the bitstream.
      .down_mixing_params = down_mixing_params,
      .recon_gain_info_parameter_data = recon_gain_info_parameter_data,
      .audio_element_with_data = &audio_element_with_data};
}

absl::StatusOr<ParameterBlockWithData>
ObuWithDataGenerator::GenerateParameterBlockWithData(
    InternalTimestamp input_start_timestamp,
    GlobalTimingModule& global_timing_module,
    std::unique_ptr<ParameterBlockObu> parameter_block_obu) {
  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;
  RETURN_IF_NOT_OK(global_timing_module.GetNextParameterBlockTimestamps(
      parameter_block_obu->parameter_id_, input_start_timestamp,
      parameter_block_obu->GetDuration(), start_timestamp, end_timestamp));
  return ParameterBlockWithData{.obu = std::move(parameter_block_obu),
                                .start_timestamp = start_timestamp,
                                .end_timestamp = end_timestamp};
}

absl::Status ObuWithDataGenerator::FillSubstreamCounts(
    ScalableChannelLayoutConfig& config) {
  ABSL_ASSIGN_OR_RETURN(const std::vector<LayerSubstreamLabels> layer_labels,
                        CollectSubstreamLabelsForLayers(config));
  ABSL_CHECK_EQ(layer_labels.size(), config.GetNumLayers());
  for (size_t i = 0; i < config.GetNumLayers(); ++i) {
    auto& layer_config = config.channel_audio_layer_configs[i];
    RETURN_IF_NOT_OK(StaticCastIfInRange<size_t, uint8_t>(
        "coupled_substream_count",
        layer_labels[i].coupled_substream_labels.size() / 2,
        layer_config.coupled_substream_count));
    RETURN_IF_NOT_OK(StaticCastIfInRange<size_t, uint8_t>(
        "substream_count",
        layer_labels[i].non_coupled_substream_labels.size() +
            layer_config.coupled_substream_count,
        layer_config.substream_count));
  }
  return absl::OkStatus();
}

absl::Status ObuWithDataGenerator::FinalizeScalableChannelLayoutConfig(
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const ScalableChannelLayoutConfig& config,
    SubstreamIdLabelsMap& substream_id_to_labels,
    LabelGainMap& label_to_output_gain,
    std::vector<ChannelNumbers>& channel_numbers_for_layers) {
  RETURN_IF_NOT_OK(ValidateUnique(audio_substream_ids.begin(),
                                  audio_substream_ids.end(),
                                  "audio_substream_ids"));
  ABSL_ASSIGN_OR_RETURN(const std::vector<LayerSubstreamLabels> layer_labels,
                        CollectSubstreamLabelsForLayers(config));

  int substream_index = 0;
  channel_numbers_for_layers.reserve(config.GetNumLayers());
  for (size_t i = 0; i < config.GetNumLayers(); ++i) {
    const int previous_layer_substream_index = substream_index;
    const auto& layer_config = config.channel_audio_layer_configs[i];
    channel_numbers_for_layers.push_back(layer_labels[i].layer_channels);

    RETURN_IF_NOT_OK(AddSubstreamLabels(
        layer_labels[i].coupled_substream_labels,
        layer_labels[i].non_coupled_substream_labels, audio_substream_ids,
        substream_id_to_labels, substream_index));
    RETURN_IF_NOT_OK(ValidateSubstreamCounts(
        layer_labels[i].coupled_substream_labels,
        layer_labels[i].non_coupled_substream_labels, layer_config));

    // Handle output gains.
    if (layer_config.output_gain_is_present_flag) {
      // Loop through all substream IDs added in this layer.
      for (int j = previous_layer_substream_index; j < substream_index; j++) {
        const auto substream_id = audio_substream_ids[j];

        ABSL_LOG(INFO) << "Output gain for substream ID: " << substream_id
                       << ":";
        for (const auto& label : substream_id_to_labels.at(substream_id)) {
          if (OutputGainApplies(layer_config.output_gain_flag, label)) {
            label_to_output_gain[label] = Q7_8ToFloat(layer_config.output_gain);
            ABSL_LOG(INFO) << "  " << label
                           << ": Q7.8= " << layer_config.output_gain
                           << "; dB= " << label_to_output_gain[label];
          } else {
            ABSL_LOG(INFO) << "  " << label << ": (not found)";
          }
        }
      }
    }
  }

  // Validate that all substreams were assigned at least one label.
  RETURN_IF_NOT_OK(ValidateEqual(
      audio_substream_ids.size(), substream_id_to_labels.size(),
      "audio_substream_ids.size() vs. substream_id_to_labels.size()"));

  return absl::OkStatus();
}

// TODO(b/340540080): Add tests for this function and remove fragility, for
// example, null pointers, get<> that can fail, etc.
absl::Status ObuWithDataGenerator::FinalizeAmbisonicsConfig(
    const AudioElementObu& audio_element_obu,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  if (audio_element_obu.GetAudioElementType() !=
      AudioElementObu::AudioElementType::kAudioElementSceneBased) {
    return InvalidArgumentError(
        "Cannot finalize AmbisonicsMonoConfig for a non-scene-based Audio "
        "Element OBU.");
  }
  const auto& ambisonics_config =
      std::get<AmbisonicsConfig>(audio_element_obu.config_);
  switch (ambisonics_config.GetAmbisonicsMode()) {
    case AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeMono:
      return FinalizeAmbisonicsMonoConfig(
          audio_element_obu,
          std::get<AmbisonicsMonoConfig>(ambisonics_config.ambisonics_config),
          substream_id_to_labels);
    case AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeProjection:
      return FinalizeAmbisonicsProjectionConfig(
          audio_element_obu,
          std::get<AmbisonicsProjectionConfig>(
              ambisonics_config.ambisonics_config),
          substream_id_to_labels);
    default:
      return absl::UnimplementedError(
          StrCat("Unimplemented Ambisonics mode: ",
                 ambisonics_config.GetAmbisonicsMode()));
  }
}

absl::Status ObuWithDataGenerator::FinalizeObjectsConfig(
    const AudioElementObu& audio_element_obu,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  if (audio_element_obu.GetAudioElementType() !=
      AudioElementObu::AudioElementType::kAudioElementObjectBased) {
    return InvalidArgumentError(
        "Cannot finalize ObjectsConfig for a non-object-based Audio "
        "Element OBU.");
  }
  auto objects_config = std::get_if<ObjectsConfig>(&audio_element_obu.config_);
  if (objects_config == nullptr) {
    return InvalidArgumentError(
        "Audio Element OBU for objects has no objects config.");
  }
  if (audio_element_obu.audio_substream_ids_.size() != 1) {
    return InvalidArgumentError(
        "Audio Element OBU for objects has incorrect number of substreams.");
  }
  if (objects_config->GetNumObjects() == 1) {
    substream_id_to_labels[audio_element_obu.audio_substream_ids_[0]] = {
        ChannelLabel::kObjectChannel0};
  } else if (objects_config->GetNumObjects() == 2) {
    substream_id_to_labels[audio_element_obu.audio_substream_ids_[0]] = {
        ChannelLabel::kObjectChannel0, ChannelLabel::kObjectChannel1};
  } else {
    return InvalidArgumentError(
        "Audio Element OBU for objects has incorrect number of objects.");
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
