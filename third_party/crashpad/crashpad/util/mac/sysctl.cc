// Copyright 2020 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/mac/sysctl.h"

#include <errno.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "base/check_op.h"
#include "base/logging.h"

namespace crashpad {

std::string ReadStringSysctlByName(const char* name, bool may_log_enoent) {
  size_t buf_len;
  if (sysctlbyname(name, nullptr, &buf_len, nullptr, 0) != 0) {
    PLOG_IF(WARNING, may_log_enoent || errno != ENOENT)
        << "sysctlbyname (size) " << name;
    return std::string();
  }

  DCHECK_GE(buf_len, 1u);

  std::string value(buf_len - 1, '\0');
  if (sysctlbyname(name, &value[0], &buf_len, nullptr, 0) != 0) {
    PLOG(WARNING) << "sysctlbyname " << name;
    return std::string();
  }

  DCHECK_EQ(value[buf_len - 1], '\0');

  return value;
}

}  // namespace crashpad
