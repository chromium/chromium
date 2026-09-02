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

#ifndef CLI_DEMIXER_H_
#define CLI_DEMIXER_H_

#include <functional>

#include "absl/status/status.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/obu/demixing_info_parameter_data.h"

namespace iamf_tools {

/*!\brief Function signature for demixing.
 *
 * Demixers reconstruct audio channels from substreams which contain mixed audio
 * channels.
 *
 * When IAMF represents channel-based audio with multiple layers, the substreams
 * contain down-mixed channels.
 *
 * When IAMF represents project-based ambisonics, the substreams are mixed for
 * coding efficiency. The demixer helps reconstruct the [ITU-2076-2] ambisonic
 * channels.
 *
 * \param down_mixing_params Down-mixing parameters.
 * \param label_to_samples Input/output map of channel labels to samples. The
 *        demixed channels are inserted into the map.
 * \return `OkStatus()` on success, or a specific status on failure.
 */
using Demixer =
    std::function<absl::Status(const DownMixingParams&, LabelSamplesMap&)>;

/*!\brief Reconstructs 7 surround channels (S7) from 5 surround channels (S5).
 *
 * Implements the surround layer demixing (reconstruction) from a 5.x.y layout
 * (L5, R5, Ls5, Rs5) and side substreams to a 7.x.y layout (L7, R7, Lrs7,
 * Rrs7) according to the IAMF spec
 * (https://aomediacodec.github.io/iamf/#processing-downmixmatrix).
 *
 * \param down_mixing_params Down-mixing parameters (alpha, beta).
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `OkStatus()` on success, or a specific status on failure.
 */
absl::Status S5ToS7Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples);

/*!\brief Reconstructs 5 surround channels (S5) from 3 surround channels (S3).
 *
 * Implements the surround layer demixing (reconstruction) from a 3.x.y layout
 * (L3, R3) and surround substreams to a 5.x.y layout (Ls5, Rs5) according to
 * the IAMF spec.
 *
 * \param down_mixing_params Down-mixing parameters (delta).
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `OkStatus()` on success, or a specific status on failure.
 */
absl::Status S3ToS5Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples);

/*!\brief Reconstructs 3 surround channels (S3) from stereo channels (S2).
 *
 * Implements the surround layer demixing (reconstruction) from a 2.x.y stereo
 * layout (L2, R2) and centre substream to a 3.x.y layout (L3, R3) according to
 * the IAMF spec.
 *
 * \param down_mixing_params Unused down-mixing parameters.
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `OkStatus()` on success, or a specific status on failure.
 */
absl::Status S2ToS3Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples);

/*!\brief Reconstructs stereo channels (S2) from mono (S1).
 *
 * Implements the surround layer demixing (reconstruction) from a 1.x.y mono
 * layout (Mono) and left substream (L2) to a stereo layout (R2) according to
 * the IAMF spec.
 *
 * \param down_mixing_params Unused down-mixing parameters.
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `OkStatus()` on success, or a specific status on failure.
 */
absl::Status S1ToS2Demixer(const DownMixingParams& /*down_mixing_params*/,
                           LabelSamplesMap& label_to_samples);

/*!\brief Reconstructs 4 top/height channels (T4) from 2 top channels (T2).
 *
 * Implements the height layer demixing (reconstruction) from a x.x.2 layout
 * (Ltf2, Rtf2) and front height substreams (Ltf4, Rtf4) to back height
 * channels (Ltb4, Rtb4) according to the IAMF spec.
 *
 * \param down_mixing_params Down-mixing parameters (gamma).
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `OkStatus()` on success, or a specific status on failure.
 */
absl::Status T2ToT4Demixer(const DownMixingParams& down_mixing_params,
                           LabelSamplesMap& label_to_samples);

/*!\brief Reconstructs 2 top/height channels (T2) from top front (TF2/TF3).
 *
 * Implements the height layer demixing (reconstruction) from a top front
 * layout (Ltf3, Rtf3) and surround channels to a x.x.2 height layout (Ltf2,
 * Rtf2) according to the IAMF spec.
 *
 * \param down_mixing_params Down-mixing parameters (w).
 * \param label_to_samples Input/output map of channel labels to samples.
 * \return `OkStatus()` on success, or a specific status on failure.
 */
absl::Status Tf2ToT2Demixer(const DownMixingParams& down_mixing_params,
                            LabelSamplesMap& label_to_samples);

}  // namespace iamf_tools

#endif  // CLI_DEMIXER_H_
