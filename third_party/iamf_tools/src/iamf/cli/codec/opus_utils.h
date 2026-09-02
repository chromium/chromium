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
#ifndef CLI_CODEC_OPUS_UTILS_H_
#define CLI_CODEC_OPUS_UTILS_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace iamf_tools {

/*!\brief Converts a `libopus` error code to an `absl::Status`.
 *
 * \param opus_error_code Error code from `libopus`.
 * \param message Message to include in the returned `absl::Status`.
 * \return `absl::Status` corresponding to input arguments.
 */
absl::Status OpusErrorCodeToAbslStatus(int opus_error_code,
                                       absl::string_view message);

}  // namespace iamf_tools

#endif  // CLI_CODEC_OPUS_UTILS_H_
