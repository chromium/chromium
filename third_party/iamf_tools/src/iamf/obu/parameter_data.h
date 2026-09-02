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
#ifndef OBU_PARAMETER_DATA_H_
#define OBU_PARAMETER_DATA_H_

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief Abstract class to holding type-specific data of a parameter subblock.
 */
struct ParameterData {
  /*!\brief Constructor.*/
  ParameterData() = default;

  /*!\brief Destructor.*/
  virtual ~ParameterData() = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  virtual absl::Status Write(WriteBitBuffer& wb) const = 0;

  /*!\brief Prints the parameter data.
   */
  virtual void Print() const = 0;

  bool friend operator==(const ParameterData& lhs,
                         const ParameterData& rhs) = default;
};

}  // namespace iamf_tools

#endif  // OBU_PARAMETER_DATA_H_
