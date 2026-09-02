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
#ifndef OBU_DUAL_CART8_PARAMETER_DATA_H_
#define OBU_DUAL_CART8_PARAMETER_DATA_H_

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct DualCart8ParameterData : public ParameterData {
  DualCart8ParameterData() = default;

  /*!\brief Overridden destructor.
   */
  ~DualCart8ParameterData() override = default;

  bool friend operator==(const DualCart8ParameterData& lhs,
                         const DualCart8ParameterData& rhs) = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the DualCart8 parameter data.
   */
  void Print() const override;
};
}  // namespace iamf_tools

#endif  // OBU_DUAL_CART8_PARAMETER_DATA_H_
