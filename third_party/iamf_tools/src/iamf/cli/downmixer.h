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

#ifndef CLI_DOWNMIXER_H_
#define CLI_DOWNMIXER_H_

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/obu/demixing_info_parameter_data.h"

namespace iamf_tools {

/*!\brief Function signature for down-mixing.
 *
 * Down-mixers mix from audio channels to the channels which are stored in
 * substreams. Later a demixer reconstructs the original audio channels.
 *
 * When IAMF represents channel-based audio with multiple layers, the substreams
 * represent down-mixed channels.
 *
 * When IAMF represents project-based ambisonics, the substreams represent mixed
 * channels for coding efficiency.
 *
 * \param down_mixing_params Down-mixing parameters.
 * \param label_to_samples Input/output map of channel labels to samples. The
 *        down-mixed channels are inserted into the map.
 * \return `OkStatus()` on success, or a specific status on failure.
 */
using DownMixer =
    absl::AnyInvocable<absl::Status(const DownMixingParams&, LabelSamplesMap&)>;

/*!\brief Down-mixes 7 surround channels (S7) to 5 surround channels (S5).
 *
 * Implements the surround layer down-mixing from a 7.x.y layout (L7, R7, Lss7,
 * Rss7, Lrs7, Rrs7) to a 5.x.y layout (L5, R5, Ls5, Rs5) according to the
 * IAMF spec (https://aomediacodec.github.io/iamf/#processing-downmixmatrix).
 *
 * \param down_mixing_params Down-mixing parameters (alpha, beta).
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `absl::OkStatus()` on success, or a specific status on failure.
 */
absl::Status S7ToS5DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap& label_to_samples);

/*!\brief Down-mixes 5 surround channels (S5) to 3 surround channels (S3).
 *
 * Implements the surround layer down-mixing from a 5.x.y layout (L5, R5, Ls5,
 * Rs5) to a 3.x.y layout (L3, R3) according to the IAMF spec.
 *
 * \param down_mixing_params Down-mixing parameters (delta).
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `absl::OkStatus()` on success, or a specific status on failure.
 */
absl::Status S5ToS3DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap& label_to_samples);

/*!\brief Down-mixes 3 surround channels (S3) to stereo channels (S2).
 *
 * Implements the surround layer down-mixing from a 3.x.y layout (L3, R3,
 * Centre) to a 2.x.y stereo layout (L2, R2) according to the IAMF spec.
 *
 * \param down_mixing_params Unused down-mixing parameters.
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `absl::OkStatus()` on success, or a specific status on failure.
 */
absl::Status S3ToS2DownMixer(const DownMixingParams& /*down_mixing_params*/,
                             LabelSamplesMap& label_to_samples);

/*!\brief Down-mixes stereo channels (S2) to mono (S1).
 *
 * Implements the surround layer down-mixing from a 2.x.y stereo layout (L2,
 * R2) to a 1.x.y mono layout (Mono) according to the IAMF spec.
 *
 * \param down_mixing_params Unused down-mixing parameters.
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `absl::OkStatus()` on success, or a specific status on failure.
 */
absl::Status S2ToS1DownMixer(const DownMixingParams& /*down_mixing_params*/,
                             LabelSamplesMap& label_to_samples);

/*!\brief Down-mixes 4 top/height channels (T4) to 2 top/height channels (T2).
 *
 * Implements the height layer down-mixing from a x.x.4 layout (Ltf4, Rtf4,
 * Ltb4, Rtb4) to a x.x.2 layout (Ltf2, Rtf2) according to the IAMF spec.
 *
 * \param down_mixing_params Down-mixing parameters (gamma).
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `absl::OkStatus()` on success, or a specific status on failure.
 */
absl::Status T4ToT2DownMixer(const DownMixingParams& down_mixing_params,
                             LabelSamplesMap& label_to_samples);

/*!\brief Down-mixes 2 top/height channels (T2) to top front channels (TF2/TF3).
 *
 * Implements the height layer down-mixing from a x.x.2 layout (Ltf2, Rtf2) to
 * a top front layout (Ltf3, Rtf3) according to the IAMF spec.
 *
 * \param down_mixing_params Down-mixing parameters (delta, w).
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `absl::OkStatus()` on success, or a specific status on failure.
 */
absl::Status T2ToTf2DownMixer(const DownMixingParams& down_mixing_params,
                              LabelSamplesMap& label_to_samples);

}  // namespace iamf_tools

#endif  // CLI_DOWNMIXER_H_
