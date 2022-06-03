// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "test/mac/mach_errors.h"

#include "base/strings/stringprintf.h"

namespace {

std::string FormatBase(const std::string& base) {
  if (base.empty()) {
    return std::string();
  }

  return base::StringPrintf("%s: ", base.c_str());
}

std::string FormatMachErrorNumber(mach_error_t mach_err) {
  // For the os/kern subsystem, give the error number in decimal as in
  // <mach/kern_return.h>. Otherwise, give it in hexadecimal to make it easier
  // to visualize the various bits. See <mach/error.h>.
  if (mach_err >= 0 && mach_err < KERN_RETURN_MAX) {
    return base::StringPrintf(" (%d)", mach_err);
  }
  return base::StringPrintf(" (0x%08x)", mach_err);
}

}  // namespace

namespace crashpad {
namespace test {

std::string MachErrorMessage(mach_error_t mach_err, const std::string& base) {
  return base::StringPrintf("%s%s%s",
                            FormatBase(base).c_str(),
                            mach_error_string(mach_err),
                            FormatMachErrorNumber(mach_err).c_str());
}

}  // namespace test
}  // namespace crashpad
