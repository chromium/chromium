// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/linux/proc_task_reader.h"

#include <stdio.h>

#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "util/file/directory_reader.h"
#include "util/misc/as_underlying_type.h"

namespace crashpad {

bool ReadThreadIDs(pid_t pid, std::vector<pid_t>* tids) {
  DCHECK(tids->empty());

  char path[32];
  snprintf(path, base::size(path), "/proc/%d/task", pid);
  DirectoryReader reader;
  if (!reader.Open(base::FilePath(path))) {
    return false;
  }

  std::vector<pid_t> local_tids;
  base::FilePath tid_str;
  DirectoryReader::Result result;
  while ((result = reader.NextFile(&tid_str)) ==
         DirectoryReader::Result::kSuccess) {
    pid_t tid;
    if (!base::StringToInt(tid_str.value(), &tid)) {
      LOG(ERROR) << "format error";
      continue;
    }

    local_tids.push_back(tid);
  }
  DCHECK_EQ(AsUnderlyingType(result),
            AsUnderlyingType(DirectoryReader::Result::kNoMoreFiles));
  DCHECK(!local_tids.empty());

  tids->swap(local_tids);
  return true;
}

}  // namespace crashpad
