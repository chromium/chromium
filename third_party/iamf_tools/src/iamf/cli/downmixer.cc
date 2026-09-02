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

#include "iamf/cli/downmixer.h"

#include <cstddef>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/obu/demixing_info_parameter_data.h"

namespace iamf_tools {

using enum ChannelLabel::Label;

absl::Status S7ToS5DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "S7 to S5";
  absl::StatusOr<size_t> num_ticks = CheckPresenceAndGetCommonNumTicks(
      label_to_samples, {kL7, kR7, kLss7, kLrs7, kRss7, kRrs7});
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  const auto& l7_samples = label_to_samples[kL7];
  const auto& lss7_samples = label_to_samples[kLss7];
  const auto& lrs7_samples = label_to_samples[kLrs7];
  const auto& r7_samples = label_to_samples[kR7];
  const auto& rss7_samples = label_to_samples[kRss7];
  const auto& rrs7_samples = label_to_samples[kRrs7];

  auto& l5_samples = label_to_samples[kL5];
  auto& r5_samples = label_to_samples[kR5];
  auto& ls5_samples = label_to_samples[kLs5];
  auto& rs5_samples = label_to_samples[kRs5];

  l5_samples = l7_samples;
  r5_samples = r7_samples;

  ls5_samples.resize(*num_ticks);
  rs5_samples.resize(*num_ticks);
  for (size_t i = 0; i < ls5_samples.size(); i++) {
    ls5_samples[i] = down_mixing_params.alpha * lss7_samples[i] +
                     down_mixing_params.beta * lrs7_samples[i];
    rs5_samples[i] = down_mixing_params.alpha * rss7_samples[i] +
                     down_mixing_params.beta * rrs7_samples[i];
  }

  return absl::OkStatus();
}

absl::Status S5ToS3DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "S5 to S3";

  absl::StatusOr<size_t> num_ticks = CheckPresenceAndGetCommonNumTicks(
      label_to_samples, {kL5, kR5, kLs5, kRs5});
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  const auto& l5_samples = label_to_samples[kL5];
  const auto& ls5_samples = label_to_samples[kLs5];
  const auto& r5_samples = label_to_samples[kR5];
  const auto& rs5_samples = label_to_samples[kRs5];

  auto& l3_samples = label_to_samples[kL3];
  auto& r3_samples = label_to_samples[kR3];
  l3_samples.resize(*num_ticks);
  r3_samples.resize(*num_ticks);
  for (size_t i = 0; i < l3_samples.size(); i++) {
    l3_samples[i] = l5_samples[i] + down_mixing_params.delta * ls5_samples[i];
    r3_samples[i] = r5_samples[i] + down_mixing_params.delta * rs5_samples[i];
  }

  return absl::OkStatus();
}

absl::Status S3ToS2DownMixer(const DownMixingParams& /*down_mixing_params*/,
                             LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "S3 to S2";

  absl::StatusOr<size_t> num_ticks =
      CheckPresenceAndGetCommonNumTicks(label_to_samples, {kL3, kR3, kCentre});
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  const auto& l3_samples = label_to_samples[kL3];
  const auto& r3_samples = label_to_samples[kR3];
  const auto& c_samples = label_to_samples[kCentre];

  auto& l2_samples = label_to_samples[kL2];
  auto& r2_samples = label_to_samples[kR2];
  l2_samples.resize(*num_ticks);
  r2_samples.resize(*num_ticks);
  for (size_t i = 0; i < l2_samples.size(); i++) {
    l2_samples[i] = l3_samples[i] + 0.707 * c_samples[i];
    r2_samples[i] = r3_samples[i] + 0.707 * c_samples[i];
  }

  return absl::OkStatus();
}

absl::Status S2ToS1DownMixer(const DownMixingParams& /*down_mixing_params*/,
                             LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "S2 to S1";

  absl::StatusOr<size_t> num_ticks =
      CheckPresenceAndGetCommonNumTicks(label_to_samples, {kL2, kR2});
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  const auto& l2_samples = label_to_samples[kL2];
  const auto& r2_samples = label_to_samples[kR2];

  auto& mono_samples = label_to_samples[kMono];
  mono_samples.resize(*num_ticks);
  for (size_t i = 0; i < mono_samples.size(); i++) {
    mono_samples[i] = 0.5 * (l2_samples[i] + r2_samples[i]);
  }

  return absl::OkStatus();
}

absl::Status T4ToT2DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "T4 to T2";

  absl::StatusOr<size_t> num_ticks = CheckPresenceAndGetCommonNumTicks(
      label_to_samples, {kLtf4, kRtf4, kLtb4, kRtb4});
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  const auto& ltf4_samples = label_to_samples[kLtf4];
  const auto& ltb4_samples = label_to_samples[kLtb4];
  const auto& rtf4_samples = label_to_samples[kRtf4];
  const auto& rtb4_samples = label_to_samples[kRtb4];

  auto& ltf2_samples = label_to_samples[kLtf2];
  auto& rtf2_samples = label_to_samples[kRtf2];
  ltf2_samples.resize(*num_ticks);
  rtf2_samples.resize(*num_ticks);
  for (size_t i = 0; i < ltf2_samples.size(); i++) {
    ltf2_samples[i] =
        ltf4_samples[i] + down_mixing_params.gamma * ltb4_samples[i];
    rtf2_samples[i] =
        rtf4_samples[i] + down_mixing_params.gamma * rtb4_samples[i];
  }

  return absl::OkStatus();
}

absl::Status T2ToTf2DownMixer(const DownMixingParams& down_mixing_params,
                              LabelSamplesMap& label_to_samples) {
  ABSL_VLOG(1) << "T2 to TF2";

  absl::StatusOr<size_t> num_ticks = CheckPresenceAndGetCommonNumTicks(
      label_to_samples, {kLtf2, kLs5, kRtf2, kRs5});
  if (!num_ticks.ok()) {
    return num_ticks.status();
  }

  const auto& ltf2_samples = label_to_samples[kLtf2];
  const auto& ls5_samples = label_to_samples[kLs5];
  const auto& rtf2_samples = label_to_samples[kRtf2];
  const auto& rs5_samples = label_to_samples[kRs5];

  auto& ltf3_samples = label_to_samples[kLtf3];
  auto& rtf3_samples = label_to_samples[kRtf3];
  ltf3_samples.resize(*num_ticks);
  rtf3_samples.resize(*num_ticks);
  for (size_t i = 0; i < ltf2_samples.size(); i++) {
    ltf3_samples[i] = ltf2_samples[i] + down_mixing_params.w *
                                            down_mixing_params.delta *
                                            ls5_samples[i];
    rtf3_samples[i] = rtf2_samples[i] + down_mixing_params.w *
                                            down_mixing_params.delta *
                                            rs5_samples[i];
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
