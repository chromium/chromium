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
#ifndef CLI_RENDERER_RENDERER_UTILS_H_
#define CLI_RENDERER_RENDERER_UTILS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Gets a key associated with the playback layout.
 *
 * The output key is the layout name of sound systems described in [ITU2051-3],
 * e.g. "0+2+0", "4+7+0".
 *
 * \param output_layout Layout to get key from.
 * \return Key associated with the layout. Or a specific status on failure.
 */
absl::StatusOr<std::string> LookupOutputKeyFromPlaybackLayout(
    const Layout& output_layout);

/*!\brief Gets the ambisonics order from the total count of ambisonics channels.
 *
 * \param channel_count Number of channels.
 * \param order Output ambisonics order.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status GetAmbisonicsOrder(const uint8_t channel_count, int& order);

/*!\brief Gets channel labels from an ambisonics-based config.
 *
 * \param ambisonics_config Config for the ambisonics layout.
 * \param audio_substream_ids Audio susbtream IDs.
 * \param substream_id_to_labels Mapping of substream IDs to labels.
 * \param channel_labels Output vector of channel labels.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status GetChannelLabelsForAmbisonics(
    const AmbisonicsConfig& ambisonics_config,
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    std::vector<ChannelLabel::Label>& channel_labels);

/*!\brief Projects samples using the demixing matrix.
 *
 * \param input_samples Input samples arranged in (channel, time).
 * \param demixing_matrix Demixing matrix to project the input samples. The
 *        shape is exptected to be (# input channels) x (# output channels),
 *        stored in a 1D array in column-major order.
 * \param projected_samples Output projected samples.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status ProjectSamplesToRender(
    absl::Span<const absl::Span<const InternalSampleType>> input_samples,
    absl::Span<const int16_t> demixing_matrix,
    std::vector<std::vector<InternalSampleType>>& projected_samples);

}  // namespace iamf_tools
#endif  // CLI_RENDERER_RENDERER_UTILS_H_
