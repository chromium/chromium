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

#include "iamf/cli/ambisonics_mixer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using absl::MakeConstSpan;

constexpr std::array<int16_t, 256> kThirdOrderDemixingMatrixFromLibopus = {
    8192,   8192,   8192,   8192,   8192,   8192,   8192,   8192,   8192,
    8192,   8192,   8192,   8192,   8192,   8192,   8192,   0,      -9779,
    9779,   6263,   8857,   0,      6263,   13829,  9779,   -13829, 0,
    -6263,  0,      -8857,  -6263,  -9779,  -3413,  3413,   3413,   -11359,
    11359,  11359,  -11359, -3413,  3413,   -3413,  -3413,  -11359, 11359,
    11359,  -11359, 3413,   13829,  9779,   -9779,  6263,   0,      8857,
    -6263,  0,      9779,   0,      -13829, 6263,   -8857,  0,      -6263,
    -9779,  0,      -15617, -15617, 6406,   0,      0,      -6406,  0,
    15617,  0,      0,      -6406,  0,      0,      6406,   15617,  0,
    -5003,  5003,   -10664, 15081,  0,      -10664, -7075,  5003,   7075,
    0,      10664,  0,      -15081, 10664,  -5003,  -8176,  -8176,  -8176,
    8208,   8208,   8208,   8208,   -8176,  -8176,  -8176,  -8176,  8208,
    8208,   8208,   8208,   -8176,  -7075,  5003,   -5003,  -10664, 0,
    15081,  10664,  0,      5003,   0,      7075,   -10664, -15081, 0,
    10664,  -5003,  15617,  0,      0,      0,      -6406,  6406,   0,
    -15617, 0,      -15617, 15617,  0,      6406,   -6406,  0,      0,
    0,      -11393, 11393,  2993,   -4233,  0,      2993,   -16112, 11393,
    16112,  0,      -2993,  0,      4233,   -2993,  -11393, 0,      -9974,
    -9974,  -13617, 0,      0,      13617,  0,      9974,   0,      0,
    13617,  0,      0,      -13617, 9974,   0,      5579,   -5579,  10185,
    14403,  0,      10185,  -7890,  -5579,  7890,   0,      -10185, 0,
    -14403, -10185, 5579,   11826,  -11826, -11826, -901,   901,    901,
    -901,   11826,  -11826, 11826,  11826,  -901,   901,    901,    -901,
    -11826, -7890,  -5579,  5579,   10185,  0,      14403,  -10185, 0,
    -5579,  0,      7890,   10185,  -14403, 0,      -10185, 5579,   -9974,
    0,      0,      0,      -13617, 13617,  0,      9974,   0,      9974,
    -9974,  0,      13617,  -13617, 0,      0,      16112,  -11393, 11393,
    -2993,  0,      4233,   2993,   0,      -11393, 0,      -16112, -2993,
    -4233,  0,      2993,   11393};

constexpr std::array<int16_t, 256> kThirdOrderMixingMatrixFromLibopus = {
    8208,   0,      -881,   14369,  0,      0,      -8192,  -4163,  13218,
    0,      0,      0,      11095,  -8836,  -6218,  14833,  8208,   -10161,
    881,    10161,  -13218, -2944,  -8192,  2944,   0,      -10488, -6218,
    6248,   -11095, -6248,  0,      -10488, 8208,   10161,  881,    -10161,
    -13218, 2944,   -8192,  -2944,  0,      10488,  -6218,  -6248,  -11095,
    6248,   0,      10488,  8176,   5566,   -11552, 5566,   9681,   -11205,
    8192,   -11205, 0,      4920,   -15158, 9756,   -3334,  9756,   0,
    -4920,  8176,   7871,   11552,  0,      0,      15846,  8192,   0,
    -9681,  -6958,  0,      13797,  3334,   0,      -15158, 0,      8176,
    0,      11552,  7871,   0,      0,      8192,   15846,  9681,   0,
    0,      0,      3334,   13797,  15158,  6958,   8176,   5566,   -11552,
    -5566,  -9681,  -11205, 8192,   11205,  0,      4920,   15158,  9756,
    -3334,  -9756,  0,      4920,   8208,   14369,  -881,   0,      0,
    -4163,  -8192,  0,      -13218, -14833, 0,      -8836,  11095,  0,
    6218,   0,      8208,   10161,  881,    10161,  13218,  2944,   -8192,
    2944,   0,      10488,  6218,   -6248,  -11095, -6248,  0,      -10488,
    8208,   -14369, -881,   0,      0,      4163,   -8192,  0,      -13218,
    14833,  0,      8836,   11095,  0,      6218,   0,      8208,   0,
    -881,   -14369, 0,      0,      -8192,  4163,   13218,  0,      0,
    0,      11095,  8836,   -6218,  -14833, 8176,   -5566,  -11552, 5566,
    -9681,  11205,  8192,   -11205, 0,      -4920,  15158,  -9756,  -3334,
    9756,   0,      -4920,  8176,   0,      11552,  -7871,  0,      0,
    8192,   -15846, 9681,   0,      0,      0,      3334,   -13797, 15158,
    -6958,  8176,   -7871,  11552,  0,      0,      -15846, 8192,   0,
    -9681,  6958,   0,      -13797, 3334,   0,      -15158, 0,      8176,
    -5566,  -11552, -5566,  9681,   11205,  8192,   11205,  0,      -4920,
    -15158, -9756,  -3334,  -9756,  0,      4920,   8208,   -10161, 881,
    -10161, 13218,  -2944,  -8192,  -2944,  0,      -10488, 6218,   6248,
    -11095, 6248,   0,      10488,
};

AmbisonicsConfig CreateMonoConfig(AmbisonicsMixer::Preset preset) {
  uint8_t substream_count;
  switch (preset) {
    using enum AmbisonicsMixer::Preset;
    case kBestPracticeForOrder0:
      substream_count = 1;
      break;
    case kBestPracticeForOrder1:
      substream_count = 4;
      break;
    case kBestPracticeForOrder2:
      substream_count = 9;
      break;
    case kBestPracticeForOrder3:
      substream_count = 16;
      break;
  }

  constexpr std::array<uint8_t, 16> kMonoAmbisonicsChannelMapping = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  auto mono_config = AmbisonicsMonoConfig::Create(
      substream_count,
      MakeConstSpan(kMonoAmbisonicsChannelMapping).first(substream_count));
  // All options are hard-coded to be valid.
  ABSL_CHECK(mono_config.ok());
  return AmbisonicsConfig{.ambisonics_config = *mono_config};
}

AmbisonicsConfig CreateProjectConfigForThirdOrder() {
  auto projection_config = AmbisonicsProjectionConfig::Create(
      /*output_channel_count=*/16, /*substream_count=*/8,
      /*coupled_substream_count=*/8,
      MakeConstSpan(kThirdOrderDemixingMatrixFromLibopus));
  ABSL_CHECK_OK(projection_config);

  return AmbisonicsConfig{.ambisonics_config = *projection_config};
}

struct AmbisonicsConfigAndMixingMatrix {
  AmbisonicsConfig config;
  std::optional<absl::Span<const int16_t>> mixing_matrix;
};

AmbisonicsConfigAndMixingMatrix MakeAmbisonicsConfig(
    CodecConfig::CodecId codec_id, AmbisonicsMixer::Preset preset) {
  if (codec_id == CodecConfig::CodecId::kCodecIdOpus &&
      preset == AmbisonicsMixer::Preset::kBestPracticeForOrder3) {
    return {.config = CreateProjectConfigForThirdOrder(),
            .mixing_matrix = kThirdOrderMixingMatrixFromLibopus};
  }

  return {.config = CreateMonoConfig(preset), .mixing_matrix = std::nullopt};
}

double Q15ToSignedDouble(const int16_t input) {
  return static_cast<double>(input) / 32768.0;
}

// This function assumes it is called from
// `SampleProcessorBase::PushFrameDerived()`, which validates that the input
// spans are well-formed and consistent.
void ApplyMixingMatrix(
    absl::Span<const double> mixing_matrix,
    absl::Span<const absl::Span<const InternalSampleType>> channel_time_samples,
    std::vector<std::vector<InternalSampleType>>& output_channel_time_samples) {
  const size_t num_channels = channel_time_samples.size();
  // All mixing matrices are statically configured to agree with the number of
  // channels.
  ABSL_CHECK_EQ(mixing_matrix.size(), num_channels * num_channels);

  const auto num_ticks = channel_time_samples[0].size();
  // Initialize output samples to 0.
  for (auto& output_channel : output_channel_time_samples) {
    output_channel.assign(num_ticks, 0.0);
  }

  // Apply mixing matrix (column-major)
  for (size_t out_channel = 0; out_channel < num_channels; ++out_channel) {
    for (size_t in_channel = 0; in_channel < num_channels; ++in_channel) {
      const double mixing_value =
          mixing_matrix[in_channel * num_channels + out_channel];
      for (size_t t = 0; t < num_ticks; ++t) {
        output_channel_time_samples[out_channel][t] +=
            mixing_value * channel_time_samples[in_channel][t];
      }
    }
  }
}

}  // namespace

AmbisonicsMixer AmbisonicsMixer::MakeFromPreset(
    CodecConfig::CodecId codec_id, Preset preset,
    size_t max_input_samples_per_frame) {
  auto config = MakeAmbisonicsConfig(codec_id, preset);
  return AmbisonicsMixer(config.config, config.mixing_matrix,
                         max_input_samples_per_frame);
}

AmbisonicsMixer AmbisonicsMixer::MakeFromAmbisonicsConfig(
    const AmbisonicsConfig& config, size_t max_input_samples_per_frame) {
  return AmbisonicsMixer(config, std::nullopt, max_input_samples_per_frame);
}

AmbisonicsConfig AmbisonicsMixer::GetAmbisonicsConfig() const {
  return ambisonics_config_;
}

std::vector<ChannelLabel::Label> AmbisonicsMixer::GetInputLabels() const {
  std::vector<ChannelLabel::Label> ordered_labels(GetNumChannels());
  for (size_t i = 0; i < ordered_labels.size(); ++i) {
    const auto label = ChannelLabel::AmbisonicsChannelNumberToLabel(i);
    ABSL_CHECK_OK(label);
    ordered_labels[i] = *label;
  }
  return ordered_labels;
}

absl::Status AmbisonicsMixer::PushFrameDerived(
    absl::Span<const absl::Span<const InternalSampleType>>
        channel_time_samples) {
  if (mixing_matrix_.empty()) {
    // For mono mixing and user-supplied configurations, we just copy the input
    // samples to the output.
    for (size_t c = 0; c < num_channels_; ++c) {
      output_channel_time_samples_[c].assign(channel_time_samples[c].begin(),
                                             channel_time_samples[c].end());
    }
  } else {
    // For some special cases (e.g. 3OA Opus), we apply a mixing matrix.
    ApplyMixingMatrix(mixing_matrix_, channel_time_samples,
                      output_channel_time_samples_);
  }
  return absl::OkStatus();
}

AmbisonicsMixer::AmbisonicsMixer(
    AmbisonicsConfig config,
    std::optional<absl::Span<const int16_t>> mixing_matrix,
    size_t max_input_samples_per_frame)
    : SampleProcessorBase(max_input_samples_per_frame,
                          config.GetOutputChannelCount(),
                          max_input_samples_per_frame),
      ambisonics_config_(config) {
  if (mixing_matrix.has_value()) {
    // Pre-compute the mixing matrix in floating point, for simpler calculations
    // when it is used.
    mixing_matrix_.reserve(mixing_matrix->size());
    for (int16_t val : *mixing_matrix) {
      mixing_matrix_.push_back(Q15ToSignedDouble(val));
    }
  }
}

}  // namespace iamf_tools
