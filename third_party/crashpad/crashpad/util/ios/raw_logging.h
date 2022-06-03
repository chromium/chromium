// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_IOS_EXCEPTION_LOGGING_H_
#define CRASHPAD_UTIL_IOS_EXCEPTION_LOGGING_H_

namespace crashpad {
namespace internal {

//! \brief Log \a message to stderr in a way that is safe to run during an
//!     in-process crash.  Also prints the given file, line number and an
//!     optional error code.
//!
//! Note: RUNS-DURING-CRASH.
void RawLog(const char* file, int line, const char* message, int error);

}  // namespace internal
}  // namespace crashpad

#define CRASHPAD_RAW_LOG(message) \
  ::crashpad::internal::RawLog(__FILE__, __LINE__, message, 0)

#define CRASHPAD_RAW_LOG_ERROR(error, message) \
  ::crashpad::internal::RawLog(__FILE__, __LINE__, message, error)

#endif  // CRASHPAD_UTIL_IOS_EXCEPTION_LOGGING_H_
