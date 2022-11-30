// Copyright 2017 The Crashpad Authors
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

#include "util/linux/scoped_ptrace_attach.h"

#include <sys/ptrace.h>
#include <sys/wait.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace crashpad {

bool PtraceAttach(pid_t pid, bool can_log) {
  if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) != 0) {
    PLOG_IF(ERROR, can_log) << "ptrace";
    return false;
  }

  int status;
  if (HANDLE_EINTR(waitpid(pid, &status, __WALL)) < 0) {
    PLOG_IF(ERROR, can_log) << "waitpid";
    return false;
  }
  if (!WIFSTOPPED(status)) {
    LOG_IF(ERROR, can_log) << "process not stopped";
    return false;
  }
  return true;
}

bool PtraceDetach(pid_t pid, bool can_log) {
  if (pid >= 0 && ptrace(PTRACE_DETACH, pid, nullptr, nullptr) != 0) {
    PLOG_IF(ERROR, can_log) << "ptrace";
    return false;
  }
  return true;
}

ScopedPtraceAttach::ScopedPtraceAttach()
    : pid_(-1) {}

ScopedPtraceAttach::~ScopedPtraceAttach() {
  Reset();
}

bool ScopedPtraceAttach::Reset() {
  if (!PtraceDetach(pid_, true)) {
    return false;
  }
  pid_ = -1;
  return true;
}

bool ScopedPtraceAttach::ResetAttach(pid_t pid) {
  Reset();

  if (!PtraceAttach(pid, true)) {
    return false;
  }

  pid_ = pid;
  return true;
}

}  // namespace crashpad
