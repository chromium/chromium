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

#include "iamf/cli/demixer.h"

#include <cmath>
#include <cstddef>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using enum ChannelLabel::Label;
using ::absl::MakeConstSpan;

absl::Status S5ToS7Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "S5 to S7";
  if (std::abs(down_mixing_params.beta) < 1e-5) {
    return absl::InvalidArgumentError(
        "beta cannot be near zero in S5ToS7Demixer");
  }

  absl::Span<const InternalSampleType> l5_samples;
  absl::Span<const InternalSampleType> ls5_samples;
  absl::Span<const InternalSampleType> lss7_samples;
  absl::Span<const InternalSampleType> r5_samples;
  absl::Span<const InternalSampleType> rs5_samples;
  absl::Span<const InternalSampleType> rss7_samples;
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kL5, label_to_samples, l5_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kLs5, label_to_samples, ls5_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kLss7, label_to_samples, lss7_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kR5, label_to_samples, r5_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kRs5, label_to_samples, rs5_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kRss7, label_to_samples, rss7_samples));

  absl::StatusOr<size_t> num_ticks =
      GetCommonNumTicks(MakeConstSpan({l5_samples, ls5_samples, lss7_samples,
                                       r5_samples, rs5_samples, rss7_samples}));
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  auto& l7_samples = label_to_samples[kDemixedL7];
  auto& r7_samples = label_to_samples[kDemixedR7];
  auto& lrs7_samples = label_to_samples[kDemixedLrs7];
  auto& rrs7_samples = label_to_samples[kDemixedRrs7];

  l7_samples = {l5_samples.begin(), l5_samples.end()};
  r7_samples = {r5_samples.begin(), r5_samples.end()};

  lrs7_samples.resize(*num_ticks, 0.0);
  rrs7_samples.resize(*num_ticks, 0.0);
  for (size_t i = 0; i < *num_ticks; i++) {
    lrs7_samples[i] =
        (ls5_samples[i] - down_mixing_params.alpha * lss7_samples[i]) /
        down_mixing_params.beta;
    rrs7_samples[i] =
        (rs5_samples[i] - down_mixing_params.alpha * rss7_samples[i]) /
        down_mixing_params.beta;
  }

  return absl::OkStatus();
}

absl::Status S3ToS5Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "S3 to S5";
  if (std::abs(down_mixing_params.delta) < 1e-5) {
    return absl::InvalidArgumentError(
        "delta cannot be near zero in S3ToS5Demixer");
  }

  absl::Span<const InternalSampleType> l3_samples;
  absl::Span<const InternalSampleType> l5_samples;
  absl::Span<const InternalSampleType> r3_samples;
  absl::Span<const InternalSampleType> r5_samples;
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kL3, label_to_samples, l3_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kL5, label_to_samples, l5_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kR3, label_to_samples, r3_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kR5, label_to_samples, r5_samples));

  absl::StatusOr<size_t> num_ticks = GetCommonNumTicks(
      MakeConstSpan({l3_samples, l5_samples, r3_samples, r5_samples}));
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  auto& ls5_samples = label_to_samples[kDemixedLs5];
  auto& rs5_samples = label_to_samples[kDemixedRs5];
  ls5_samples.resize(*num_ticks, 0.0);
  rs5_samples.resize(*num_ticks, 0.0);
  for (size_t i = 0; i < *num_ticks; i++) {
    ls5_samples[i] = (l3_samples[i] - l5_samples[i]) / down_mixing_params.delta;
    rs5_samples[i] = (r3_samples[i] - r5_samples[i]) / down_mixing_params.delta;
  }

  return absl::OkStatus();
}

absl::Status S2ToS3Demixer(const DownMixingParams& /*down_mixing_params*/,
                           LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "S2 to S3";

  absl::Span<const InternalSampleType> l2_samples;
  absl::Span<const InternalSampleType> r2_samples;
  absl::Span<const InternalSampleType> c_samples;
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kL2, label_to_samples, l2_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kR2, label_to_samples, r2_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kCentre, label_to_samples, c_samples));

  absl::StatusOr<size_t> num_ticks =
      GetCommonNumTicks(MakeConstSpan({l2_samples, r2_samples, c_samples}));
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  auto& l3_samples = label_to_samples[kDemixedL3];
  auto& r3_samples = label_to_samples[kDemixedR3];

  l3_samples.resize(*num_ticks, 0.0);
  r3_samples.resize(*num_ticks, 0.0);
  for (size_t i = 0; i < *num_ticks; i++) {
    l3_samples[i] = (l2_samples[i] - 0.707 * c_samples[i]);
    r3_samples[i] = (r2_samples[i] - 0.707 * c_samples[i]);
  }

  return absl::OkStatus();
}

absl::Status S1ToS2Demixer(const DownMixingParams& /*down_mixing_params*/,
                           LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "S1 to S2";

  absl::Span<const InternalSampleType> l2_samples;
  absl::Span<const InternalSampleType> mono_samples;
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kL2, label_to_samples, l2_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kMono, label_to_samples, mono_samples));

  absl::StatusOr<size_t> num_ticks =
      GetCommonNumTicks(MakeConstSpan({l2_samples, mono_samples}));
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  auto& r2_samples = label_to_samples[kDemixedR2];
  r2_samples.resize(*num_ticks, 0.0);
  for (size_t i = 0; i < *num_ticks; i++) {
    r2_samples[i] = 2.0 * mono_samples[i] - l2_samples[i];
  }

  return absl::OkStatus();
}

absl::Status T2ToT4Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "T2 to T4";
  if (std::abs(down_mixing_params.gamma) < 1e-5) {
    return absl::InvalidArgumentError(
        "gamma cannot be near zero in T2ToT4Demixer");
  }

  absl::Span<const InternalSampleType> ltf2_samples;
  absl::Span<const InternalSampleType> ltf4_samples;
  absl::Span<const InternalSampleType> rtf2_samples;
  absl::Span<const InternalSampleType> rtf4_samples;
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kLtf2, label_to_samples, ltf2_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kLtf4, label_to_samples, ltf4_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kRtf2, label_to_samples, rtf2_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kRtf4, label_to_samples, rtf4_samples));

  absl::StatusOr<size_t> num_ticks = GetCommonNumTicks(
      MakeConstSpan({ltf2_samples, ltf4_samples, rtf2_samples, rtf4_samples}));
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  auto& ltb4_samples = label_to_samples[kDemixedLtb4];
  auto& rtb4_samples = label_to_samples[kDemixedRtb4];
  ltb4_samples.resize(*num_ticks, 0.0);
  rtb4_samples.resize(*num_ticks, 0.0);
  for (size_t i = 0; i < *num_ticks; i++) {
    ltb4_samples[i] =
        (ltf2_samples[i] - ltf4_samples[i]) / down_mixing_params.gamma;
    rtb4_samples[i] =
        (rtf2_samples[i] - rtf4_samples[i]) / down_mixing_params.gamma;
  }

  return absl::OkStatus();
}

absl::Status Tf2ToT2Demixer(const DownMixingParams& down_mixing_params,
                            LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "TF2 to T2";

  absl::Span<const InternalSampleType> ltf3_samples;
  absl::Span<const InternalSampleType> l3_samples;
  absl::Span<const InternalSampleType> l5_samples;
  absl::Span<const InternalSampleType> rtf3_samples;
  absl::Span<const InternalSampleType> r3_samples;
  absl::Span<const InternalSampleType> r5_samples;
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kLtf3, label_to_samples, ltf3_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kL3, label_to_samples, l3_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kL5, label_to_samples, l5_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kRtf3, label_to_samples, rtf3_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kR3, label_to_samples, r3_samples));
  RETURN_IF_NOT_OK(
      FindSamplesOrDemixedSamples(kR5, label_to_samples, r5_samples));

  absl::StatusOr<size_t> num_ticks =
      GetCommonNumTicks(MakeConstSpan({ltf3_samples, l3_samples, l5_samples,
                                       rtf3_samples, r3_samples, r5_samples}));
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  auto& ltf2_samples = label_to_samples[kDemixedLtf2];
  auto& rtf2_samples = label_to_samples[kDemixedRtf2];
  ltf2_samples.resize(*num_ticks, 0.0);
  rtf2_samples.resize(*num_ticks, 0.0);
  for (size_t i = 0; i < *num_ticks; i++) {
    ltf2_samples[i] = ltf3_samples[i] -
                      down_mixing_params.w * (l3_samples[i] - l5_samples[i]);
    rtf2_samples[i] = rtf3_samples[i] -
                      down_mixing_params.w * (r3_samples[i] - r5_samples[i]);
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
