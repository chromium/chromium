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
#ifndef INCLUDE_IAMF_TOOLS_IAMF_TOOLS_ENCODER_API_TYPES_H_
#define INCLUDE_IAMF_TOOLS_IAMF_TOOLS_ENCODER_API_TYPES_H_

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"

namespace iamf_tools {
namespace api {

// Audio channels for this audio element, mapped by a serialized
// `ChannelLabelMessage` protocol buffer.
using IamfAudioElementData =
    absl::flat_hash_map<std::string, absl::Span<const double>>;

struct IamfTemporalUnitData {
  // Mapping of parameter block IDs to serialized `ParameterBlockObuMetadata`
  // protocol buffer starting in this temporal unit.
  absl::flat_hash_map<uint32_t, std::string> parameter_block_id_to_metadata;

  // All audio elements for this temporal unit.
  absl::flat_hash_map<uint32_t, IamfAudioElementData> audio_element_id_to_data;
};

}  // namespace api
}  // namespace iamf_tools

#endif  // INCLUDE_IAMF_TOOLS_IAMF_TOOLS_ENCODER_API_TYPES_H_
