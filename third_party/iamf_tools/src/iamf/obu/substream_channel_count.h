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

#ifndef OBU_SUBSTREAM_CHANNEL_COUNT_H_
#define OBU_SUBSTREAM_CHANNEL_COUNT_H_

#include <cstddef>

#include "absl/status/statusor.h"

namespace iamf_tools {

/*!\brief Class to represent a sanitized number of channels in a substream.
 *
 * This class represents the number of channels in a substream. While IAMF
 * can support many channels, it is exclusively made with elementary substreams
 * that are either singular or coupled.
 */
class SubstreamChannelCount {
 public:
  /*!\brief Makes a singular substream channel count.
   *
   * Convenience function to create a singular substream channel count (i.e. 1).
   *
   * \return `SubstreamChannelCount` with 1 channel.
   */
  static SubstreamChannelCount MakeSingular();

  /*!\brief Makes a coupled substream channel count.
   *
   * Convenience function to create a coupled substream channel count (i.e. 2).
   *
   * \return `SubstreamChannelCount` with 2 channels.
   */
  static SubstreamChannelCount MakeCoupled();

  /*!\brief Creates a substream channel count.
   *
   * Function to create a substream channel count from the number of channels.
   *
   * \param num_channels Number of channels for this substream.
   * \return `SubstreamChannelCount` if the number of channels is valid (1 or
   *         2), otherwise an error.
   */
  static absl::StatusOr<SubstreamChannelCount> Create(int num_channels);

  /*!\brief Returns the number of channels in this substream.
   *
   * \return Number of channels in this substream.
   */
  size_t num_channels() const;

 private:
  /*!\brief Private constructor.
   *
   * \param num_channels Number of channels for this substream.
   */
  SubstreamChannelCount(size_t num_channels);

  size_t num_channels_;  // Fixed to 1 or 2.
};

}  // namespace iamf_tools

#endif  // OBU_SUBSTREAM_CHANNEL_COUNT_H_
