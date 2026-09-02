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
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"

#include <ostream>
#include <string>

namespace iamf_tools {
namespace api {

IamfStatus::IamfStatus(const std::string& error_message)
    : success(false), error_message(error_message) {}

IamfStatus IamfStatus::OkStatus() { return IamfStatus(); }

IamfStatus IamfStatus::ErrorStatus(const std::string& error_message) {
  return IamfStatus(error_message);
}

std::ostream& operator<<(std::ostream& os, const IamfStatus& status) {
  if (status.ok()) {
    os << "Success\n";
  } else {
    os << "Failure: " << status.error_message << "\n";
  }
  return os;
}

}  // namespace api
}  // namespace iamf_tools
