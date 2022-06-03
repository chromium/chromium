// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MALDOCA_BASE_LOGGING_H_
#define MALDOCA_BASE_LOGGING_H_

#if defined(MALDOCA_IN_CHROMIUM)
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"

#elif defined(MALDOCA_CHROME)
#include "mini_chromium/base/logging.h"

#endif

#if defined(MALDOCA_CHROME) || defined(MALDOCA_IN_CHROMIUM)

namespace maldoca {

inline bool InitLogging() {
  ::logging::LoggingSettings setting;
#if defined(_WIN32)
  // Required to make "ASSERT_DEATH" statements in unit tests on Windows work.
  setting.logging_dest = logging::LOG_TO_STDERR;
#endif
  return ::logging::InitLogging(setting);
}
}  // namespace maldoca

#else  // defined(MALDOCA_CHROME) || defined(MALDOCA_IN_CHROMIUM)

#include "zetasql/base/logging.h"

#define MALDOCA_DEBUG_MODE ZETASQL_DEBUG_MODE
namespace maldoca {
#define MALDOCA_CHECK ZETASQL_CHECK
using ::zetasql_base::get_log_directory;
using ::zetasql_base::get_vlog_level;
using ::zetasql_base::InitLogging;
}  // namespace maldoca

#endif  // MALDOCA_CHROME
#endif  // MALDOCA_BASE_LOGGING_H_
