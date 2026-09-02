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

#include "iamf/cli/renderer/audio_element_renderer_ambisonics_to_channel.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/renderer/loudspeakers_renderer.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
namespace iamf_tools {

namespace {

double Q15ToSignedDouble(const int16_t input) {
  return static_cast<double>(input) / 32768.0;
}

/*!\brief Multiplies the demixing matrix and gains matrix.
 *
 * Conceptually, Ambisonics projection is performed by several sequential matrix
 * multiplications:
 *
 *  - M: Number of input channels (`num_substreams` + `num_coupled_substreams`).
 *  - N: Number of intermediate projected channels (`output_channel_count`).
 *  - K: Number of output speaker channels.
 *  - X (M x 1): Input audio vector (per tick).
 *  - D (N x M): Substreams to Ambisonics `demixing_matrix`.
 *  - G (K x N): Ambisonics to speaker `gains` matrix.
 *
 *   A) Project the input audio vector to the Ambisonics domain:
 *      Y = D * X  ->  Dimensions: (N x 1) = (N x M) * (M x 1)
 *   B) Map the Ambisonics domain to the speaker outputs:
 *      Z = G * Y  ->  Dimensions: (K x 1) = (K x N) * (N x 1)
 *
 *   This has a dynamic computation cost of (N * M + K * N) operations per tick.
 *
 * Matrix multiplication is associative, therefore we can pre-compute C at
 * initialization.
 *
 *   C = G * D  ->  Dimensions: (K x M) = (K x N) * (N x M)
 *
 * Then, we this can be used as a single matrix multiplication at runtime:
 *
 *   Z = C * X  ->  Dimensions: (K x 1) = (K x M) * (M x 1)
 * * This reduces the dynamic computation cost to just (K * M) operations per
 * tick (e.g., from 675 down to 50 operations per tick for 4OA to Stereo, saving
 * ~93%).
 *
 * Note on Memory Layout:
 *   To match the runtime loop layout, the input `gains` matrix is stored as the
 *   transpose G^T (N x K), and this function computes and returns the transpose
 *   C^T (M x K):
 *      C^T = D^T * G^T  ->  (M x N) * (N x K) = (M x K)
 *
 *
 * \param projection_config Config for the Ambisonics projection. Contains D (N
 *        x M).
 * \param gains Precomputed gains matrix G (N x K).
 * \return The combined gains matrix C (M x K) on success, or a specific status
 *         on failure.
 */
absl::StatusOr<std::vector<std::vector<double>>> GetCombinedGains(
    const AmbisonicsProjectionConfig& projection_config,
    const std::vector<std::vector<double>>& gains) {
  const size_t num_input_channels =
      projection_config.GetSubstreamCount() +
      projection_config.GetCoupledSubstreamCount();
  const auto num_projected_channels = projection_config.GetOutputChannelCount();
  absl::Span<const int16_t> demixing_matrix_q15 =
      projection_config.GetDemixingMatrixView();

  // Verify the matrix is the correct size.
  RETURN_IF_NOT_OK(
      ValidateContainerSizeEqual("gains", gains, num_projected_channels));
  // For safety, verify that all rows have the same size.
  const size_t num_output_channels = gains.empty() ? 0 : gains[0].size();
  for (const auto& gain_row : gains) {
    RETURN_IF_NOT_OK(
        ValidateContainerSizeEqual("gain_row", gain_row, num_output_channels));
  }

  // Pre-allocate the output matrix.
  std::vector<std::vector<double>> combined_gains(
      num_input_channels, std::vector<double>(num_output_channels, 0.0));
  for (size_t i = 0; i < num_input_channels; ++i) {
    for (size_t k = 0; k < num_output_channels; ++k) {
      for (size_t j = 0; j < num_projected_channels; ++j) {
        const size_t index = i * num_projected_channels + j;
        // `AmbisonicsProjectionConfig` guarantees this is valid, but for safety
        // we check.
        ABSL_CHECK_LT(index, demixing_matrix_q15.size());
        combined_gains[i][k] +=
            Q15ToSignedDouble(demixing_matrix_q15[index]) * gains[j][k];
      }
    }
  }
  return combined_gains;
}

}  // namespace

std::unique_ptr<AudioElementRendererAmbisonicsToChannel>
AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
    const AmbisonicsConfig& ambisonics_config,
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const Layout& playback_layout, size_t num_samples_per_frame,
    const TrimmingSettings trimming_settings) {
  // Exclude unsupported modes first, and deal with only mono or projection
  // in the rest of the code.
  const auto mode = ambisonics_config.GetAmbisonicsMode();
  if (mode != AmbisonicsConfig::kAmbisonicsModeMono &&
      mode != AmbisonicsConfig::kAmbisonicsModeProjection) {
    ABSL_LOG(ERROR) << "Unsupported ambisonics mode. mode= " << mode;
    return nullptr;
  }

  std::vector<ChannelLabel::Label> channel_labels;
  if (const auto status =
          GetChannelLabelsForAmbisonics(ambisonics_config, audio_substream_ids,
                                        substream_id_to_labels, channel_labels);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }

  const auto& output_key = LookupOutputKeyFromPlaybackLayout(playback_layout);
  if (!output_key.ok()) {
    ABSL_LOG(ERROR) << output_key.status();
    return nullptr;
  }

  int ambisonics_order = 0;
  // Input key for ambisonics is "A{ambisonics_order}".
  if (const auto status = GetAmbisonicsOrder(
          ambisonics_config.GetOutputChannelCount(), ambisonics_order);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }
  const std::string input_key = absl::StrCat("A", ambisonics_order);
  const auto& gains = LookupPrecomputedGains(input_key, *output_key);
  if (!gains.ok()) {
    ABSL_LOG(ERROR) << gains.status();
    return nullptr;
  }
  const std::string gains_map_key =
      absl::StrCat("A", ambisonics_order, "->", *output_key);

  int32_t num_output_channels = 0;
  if (!MixPresentationObu::GetNumChannelsFromLayout(playback_layout,
                                                    num_output_channels)
           .ok()) {
    return nullptr;
  }

  if (mode == AmbisonicsConfig::kAmbisonicsModeMono) {
    std::vector<ChannelLabel::Label> active_channel_labels;
    std::vector<std::vector<double>> combined_gains;
    active_channel_labels.reserve(channel_labels.size());
    combined_gains.reserve(channel_labels.size());
    if (gains->size() != channel_labels.size()) {
      ABSL_LOG(ERROR) << "Gains size does not match channel labels size.";
      return nullptr;
    }

    for (size_t i = 0; i < channel_labels.size(); ++i) {
      if (channel_labels[i] != ChannelLabel::kOmitted) {
        active_channel_labels.push_back(channel_labels[i]);
        combined_gains.push_back((*gains)[i]);
      }
    }
    // `AmbisonicsMonoConfig` guarantees there are active channels, but for
    // safety we check.
    ABSL_CHECK(!active_channel_labels.empty());

    return absl::WrapUnique(new AudioElementRendererAmbisonicsToChannel(
        static_cast<size_t>(num_output_channels), num_samples_per_frame,
        active_channel_labels, combined_gains, trimming_settings));
  }

  // The top of the function guarantees this is now projection mode.
  const auto* projection_config = std::get_if<AmbisonicsProjectionConfig>(
      &ambisonics_config.ambisonics_config);
  if (projection_config == nullptr) {
    ABSL_LOG(ERROR) << "Failed to get projection config.";
    return nullptr;
  }

  auto combined_gains = GetCombinedGains(*projection_config, *gains);
  if (!combined_gains.ok()) {
    ABSL_LOG(ERROR) << combined_gains.status();
    return nullptr;
  }

  return absl::WrapUnique(new AudioElementRendererAmbisonicsToChannel(
      static_cast<size_t>(num_output_channels), num_samples_per_frame,
      channel_labels, *combined_gains, trimming_settings));
}

absl::Status AudioElementRendererAmbisonicsToChannel::RenderSamples(
    absl::Span<const absl::Span<const InternalSampleType>> samples_to_render) {
  // Render the samples.
  return RenderChannelLayoutToLoudspeakers(samples_to_render, gains_,
                                           rendered_samples_);
}

}  // namespace iamf_tools
