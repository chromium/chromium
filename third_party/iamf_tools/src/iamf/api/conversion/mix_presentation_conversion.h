/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef API_CONVERSION_MIX_PRESENTATION_CONVERSION_H_
#define API_CONVERSION_MIX_PRESENTATION_CONVERSION_H_

#include <optional>

#include "absl/status/statusor.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

/*!\brief Converts the API-requested OutputLayout to an internal IAMF Layout. */
std::optional<Layout> ApiToInternalType(
    std::optional<api::OutputLayout> api_output_layout);

/*!\brief Converts the internal IAMF Layout to the API OutputLayout. */
absl::StatusOr<api::OutputLayout> InternalToApiType(Layout internal_layout);

}  // namespace iamf_tools

#endif  // API_CONVERSION_MIX_PRESENTATION_CONVERSION_H_
