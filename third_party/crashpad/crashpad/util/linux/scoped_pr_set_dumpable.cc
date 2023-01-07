// Copyright 2018 The Crashpad Authors
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

#include "util/linux/scoped_pr_set_dumpable.h"

#include <sys/prctl.h>

#include "base/logging.h"

namespace crashpad {

ScopedPrSetDumpable::ScopedPrSetDumpable(bool may_log) : may_log_(may_log) {
  int result = prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
  PLOG_IF(ERROR, result < 0 && may_log_) << "prctl";
  was_dumpable_ = result > 0;

  if (!was_dumpable_) {
    result = prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
    PLOG_IF(ERROR, result != 0 && may_log_) << "prctl";
  }
}

ScopedPrSetDumpable::~ScopedPrSetDumpable() {
  if (!was_dumpable_) {
    int result = prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
    PLOG_IF(ERROR, result != 0 && may_log_) << "prctl";
  }
}

}  // namespace crashpad
