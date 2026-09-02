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
#include "iamf/obu/ambisonics_config.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

void LogAmbisonicsMonoConfig(const AmbisonicsMonoConfig& mono_config) {
  ABSL_VLOG(1) << "  ambisonics_mono_config:";
  ABSL_VLOG(1) << "    output_channel_count: "
               << absl::StrCat(mono_config.GetOutputChannelCount());
  ABSL_VLOG(1) << "    substream_count: "
               << absl::StrCat(mono_config.GetSubstreamCount());
  ABSL_VLOG(1) << "    channel_mapping: ["
               << absl::StrJoin(mono_config.GetChannelMappingView(), ", ")
               << "]";
}

void LogAmbisonicsProjectionConfig(const AmbisonicsProjectionConfig& config) {
  ABSL_VLOG(1) << "  ambisonics_projection_config:";
  ABSL_VLOG(1) << "    output_channel_count: "
               << absl::StrCat(config.GetOutputChannelCount());
  ABSL_VLOG(1) << "    substream_count:"
               << absl::StrCat(config.GetSubstreamCount());
  ABSL_VLOG(1) << "    coupled_substream_count:"
               << absl::StrCat(config.GetCoupledSubstreamCount());
  ABSL_VLOG(1) << "    demixing_matrix: [ "
               << absl::StrJoin(config.GetDemixingMatrixView(), ", ") << "]";
}

absl::Status ValidateOutputChannelCount(const uint8_t channel_count) {
  uint8_t next_valid_output_channel_count;
  RETURN_IF_NOT_OK(AmbisonicsConfig::GetNextValidOutputChannelCount(
      channel_count, next_valid_output_channel_count));

  if (next_valid_output_channel_count == channel_count) {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(absl::StrCat(
      "Invalid Ambisonics output channel_count = ", channel_count));
}

// Writes the `AmbisonicsMonoConfig` of an ambisonics mono `AudioElementObu`.
absl::Status WriteAmbisonicsMono(const AmbisonicsMonoConfig& mono_config,
                                 WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(mono_config.GetOutputChannelCount(), 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(mono_config.GetSubstreamCount(), 8));

  return wb.WriteUint8Span(mono_config.GetChannelMappingView());
}

// Writes the `AmbisonicsProjectionConfig` of an ambisonics projection
// `AudioElementObu`.
absl::Status WriteAmbisonicsProjection(
    const AmbisonicsProjectionConfig& projection_config, WriteBitBuffer& wb) {
  // Write the main portion of the `AmbisonicsProjectionConfig`.
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(projection_config.GetOutputChannelCount(), 8));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(projection_config.GetSubstreamCount(), 8));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(projection_config.GetCoupledSubstreamCount(), 8));

  // Loop to write the `demixing_matrix`.
  for (int16_t val : projection_config.GetDemixingMatrixView()) {
    RETURN_IF_NOT_OK(wb.WriteSigned16(val));
  }

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<AmbisonicsMonoConfig> AmbisonicsMonoConfig::Create(
    uint8_t substream_count, absl::Span<const uint8_t> channel_mapping) {
  const size_t output_channel_count = channel_mapping.size();
  MAYBE_RETURN_IF_NOT_OK(ValidateOutputChannelCount(output_channel_count));
  RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
      "channel_mapping", channel_mapping, output_channel_count));
  RETURN_IF_NOT_OK(ValidateInRange(size_t{substream_count},
                                   {size_t{1}, output_channel_count},
                                   "substream_count"));

  // Track the number of unique substream indices in the mapping.
  absl::flat_hash_set<uint8_t> unique_substream_indices;
  for (const auto& substream_index : channel_mapping) {
    if (substream_index == kInactiveAmbisonicsChannelNumber) {
      // OK. This implies the nth ambisonics channel number is dropped (i.e. the
      // user wants mixed-order ambisonics).
      continue;
    }
    if (substream_index >= substream_count) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Mapping out of bounds. When substream_count= ", substream_count,
          " there is no substream_index= ", substream_index, "."));
    }

    unique_substream_indices.insert(substream_index);
  }

  if (unique_substream_indices.size() != substream_count) {
    return absl::InvalidArgumentError(absl::StrCat(
        "A substream is in limbo; it has no associated ACN. ",
        "substream_count= ", substream_count,
        ", unique_substream_indices.size()= ", unique_substream_indices.size(),
        "."));
  }

  return AmbisonicsMonoConfig(substream_count, channel_mapping);
}

absl::StatusOr<AmbisonicsMonoConfig> AmbisonicsMonoConfig::CreateFromBuffer(
    ReadBitBuffer& rb) {
  uint8_t output_channel_count;
  uint8_t substream_count;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, output_channel_count));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, substream_count));
  std::vector<uint8_t> channel_mapping(output_channel_count);
  RETURN_IF_NOT_OK(rb.ReadUint8Span(absl::MakeSpan(channel_mapping)));
  return Create(substream_count, channel_mapping);
}

AmbisonicsMonoConfig::AmbisonicsMonoConfig(
    uint8_t substream_count, absl::Span<const uint8_t> channel_mapping)
    : substream_count_(substream_count),
      channel_mapping_(channel_mapping.begin(), channel_mapping.end()) {}

absl::StatusOr<AmbisonicsProjectionConfig> AmbisonicsProjectionConfig::Create(
    uint8_t output_channel_count, uint8_t substream_count,
    uint8_t coupled_substream_count,
    absl::Span<const int16_t> demixing_matrix) {
  RETURN_IF_NOT_OK(ValidateOutputChannelCount(output_channel_count));
  if (coupled_substream_count > substream_count) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected coupled_substream_count= ", coupled_substream_count,
        " to be less than or equal to substream_count= ", substream_count));
  }

  // Add these as `size_t` to a avoid potential overflow.
  const size_t total_input_channels =
      static_cast<size_t>(substream_count) +
      static_cast<size_t>(coupled_substream_count);
  RETURN_IF_NOT_OK(ValidateInRange(total_input_channels,
                                   {size_t{1}, size_t{output_channel_count}},
                                   "substream_count"));

  const size_t expected_num_elements =
      total_input_channels * output_channel_count;
  RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
      "demixing_matrix", demixing_matrix, expected_num_elements));

  return AmbisonicsProjectionConfig(output_channel_count, substream_count,
                                    coupled_substream_count, demixing_matrix);
}

absl::StatusOr<AmbisonicsProjectionConfig>
AmbisonicsProjectionConfig::CreateFromBuffer(ReadBitBuffer& rb) {
  uint8_t output_channel_count;
  uint8_t substream_count;
  uint8_t coupled_substream_count;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, output_channel_count));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, substream_count));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, coupled_substream_count));

  RETURN_IF_NOT_OK(ValidateOutputChannelCount(output_channel_count));
  if (coupled_substream_count > substream_count) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected coupled_substream_count= ", coupled_substream_count,
        " to be less than or equal to substream_count= ", substream_count));
  }
  const size_t total_input_channels =
      static_cast<size_t>(substream_count) + coupled_substream_count;
  RETURN_IF_NOT_OK(ValidateInRange(total_input_channels,
                                   {size_t{1}, size_t{output_channel_count}},
                                   "substream_count"));

  const size_t demixing_matrix_size =
      total_input_channels * output_channel_count;
  std::vector<int16_t> demixing_matrix;
  demixing_matrix.reserve(demixing_matrix_size);
  for (size_t i = 0; i < demixing_matrix_size; ++i) {
    int16_t demixing_matrix_value;
    RETURN_IF_NOT_OK(rb.ReadSigned16(demixing_matrix_value));
    demixing_matrix.push_back(demixing_matrix_value);
  }
  return Create(output_channel_count, substream_count, coupled_substream_count,
                demixing_matrix);
}

AmbisonicsProjectionConfig::AmbisonicsProjectionConfig(
    uint8_t output_channel_count, uint8_t substream_count,
    uint8_t coupled_substream_count, absl::Span<const int16_t> demixing_matrix)
    : output_channel_count_(output_channel_count),
      substream_count_(substream_count),
      coupled_substream_count_(coupled_substream_count),
      demixing_matrix_(demixing_matrix.begin(), demixing_matrix.end()) {}

absl::Status AmbisonicsConfig::GetNextValidOutputChannelCount(
    uint8_t requested_output_channel_count,
    uint8_t& next_valid_output_channel_count) {
  // Valid values are `(1+n)^2`, for integer `n` in the range [0, 14].
  static constexpr auto kValidAmbisonicChannelCounts = []() -> auto {
    std::array<uint8_t, 15> channel_count_i;
    for (size_t i = 0; i < channel_count_i.size(); ++i) {
      channel_count_i[i] = (i + 1) * (i + 1);
    }
    return channel_count_i;
  }();

  // Lookup the next higher or equal valid channel count.
  auto valid_channel_count_iter = std::lower_bound(
      kValidAmbisonicChannelCounts.begin(), kValidAmbisonicChannelCounts.end(),
      requested_output_channel_count);
  if (valid_channel_count_iter != kValidAmbisonicChannelCounts.end()) {
    next_valid_output_channel_count = *valid_channel_count_iter;
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(absl::StrCat(
      "Output channel count is too large. requested_output_channel_count= ",
      requested_output_channel_count,
      ". Max=", kValidAmbisonicChannelCounts.back(), "."));
}

absl::Status AmbisonicsConfig::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(GetAmbisonicsMode())));

  switch (GetAmbisonicsMode()) {
    using enum AmbisonicsConfig::AmbisonicsMode;
    case kAmbisonicsModeMono:
      return WriteAmbisonicsMono(
          std::get<AmbisonicsMonoConfig>(ambisonics_config), wb);
    case kAmbisonicsModeProjection:
      return WriteAmbisonicsProjection(
          std::get<AmbisonicsProjectionConfig>(ambisonics_config), wb);
    default:
      return absl::OkStatus();
  }
}

absl::Status AmbisonicsConfig::ReadAndValidate(ReadBitBuffer& rb) {
  DecodedUleb128 ambisonics_mode_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(ambisonics_mode_uleb));
  switch (static_cast<AmbisonicsMode>(ambisonics_mode_uleb)) {
    using enum AmbisonicsConfig::AmbisonicsMode;
    case kAmbisonicsModeMono: {
      auto mono_config = AmbisonicsMonoConfig::CreateFromBuffer(rb);
      if (!mono_config.ok()) {
        return mono_config.status();
      }
      ambisonics_config = *mono_config;
      return absl::OkStatus();
    }
    case kAmbisonicsModeProjection: {
      auto projection_config = AmbisonicsProjectionConfig::CreateFromBuffer(rb);
      if (!projection_config.ok()) {
        return projection_config.status();
      }
      ambisonics_config = *projection_config;
      return absl::OkStatus();
    }
    default:
      return absl::OkStatus();
  }
}

void AmbisonicsConfig::Print() const {
  ABSL_VLOG(1) << "  ambisonics_config:";
  ABSL_VLOG(1) << "    ambisonics_mode= " << absl::StrCat(GetAmbisonicsMode());
  if (GetAmbisonicsMode() == AmbisonicsConfig::kAmbisonicsModeMono) {
    LogAmbisonicsMonoConfig(std::get<AmbisonicsMonoConfig>(ambisonics_config));
  } else if (GetAmbisonicsMode() ==
             AmbisonicsConfig::kAmbisonicsModeProjection) {
    LogAmbisonicsProjectionConfig(
        std::get<AmbisonicsProjectionConfig>(ambisonics_config));
  }
}

uint8_t AmbisonicsConfig::GetOutputChannelCount() const {
  return std::visit(
      [](const auto& config) { return config.GetOutputChannelCount(); },
      ambisonics_config);
}

uint8_t AmbisonicsConfig::GetNumSubstreams() const {
  return std::visit(
      [](const auto& config) { return config.GetSubstreamCount(); },
      ambisonics_config);
}

AmbisonicsConfig::AmbisonicsMode AmbisonicsConfig::GetAmbisonicsMode() const {
  return std::visit(
      [](const auto& config) {
        using Type = std::decay_t<decltype(config)>;
        if constexpr (std::is_same_v<Type, AmbisonicsMonoConfig>) {
          return kAmbisonicsModeMono;
        } else if constexpr (std::is_same_v<Type, AmbisonicsProjectionConfig>) {
          return kAmbisonicsModeProjection;
        }
      },
      ambisonics_config);
}

std::optional<absl::Span<const int16_t>> AmbisonicsConfig::GetDemixingMatrix()
    const {
  if (std::holds_alternative<AmbisonicsProjectionConfig>(ambisonics_config)) {
    return std::get<AmbisonicsProjectionConfig>(ambisonics_config)
        .GetDemixingMatrixView();
  }
  return std::nullopt;
}

}  // namespace iamf_tools
