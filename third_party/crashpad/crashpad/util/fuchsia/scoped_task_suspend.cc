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

#include "util/fuchsia/scoped_task_suspend.h"

#include <lib/zx/time.h>
#include <zircon/errors.h>
#include <zircon/status.h>
#include <zircon/syscalls/object.h>

#include <vector>

#include "base/check_op.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "util/fuchsia/koid_utilities.h"

namespace crashpad {

ScopedTaskSuspend::ScopedTaskSuspend(const zx::process& process) {
  DCHECK_NE(process.get(), zx::process::self()->get());

  const zx_status_t suspend_status = process.suspend(&suspend_token_);
  if (suspend_status != ZX_OK) {
    ZX_LOG(ERROR, suspend_status) << "zx_task_suspend";
    return;
  }

  // suspend() is asynchronous so we now check that each thread is indeed
  // suspended, up to some deadline.
  for (const auto& thread : GetThreadHandles(process)) {
    // We omit the crashed thread (blocked in an exception) as it is technically
    // not suspended, cf. ZX-3772.
    zx_info_thread_t info;
    if (thread.get_info(
            ZX_INFO_THREAD, &info, sizeof(info), nullptr, nullptr) == ZX_OK) {
      if (info.state == ZX_THREAD_STATE_BLOCKED_EXCEPTION) {
        continue;
      }
    }

    zx_signals_t observed = 0u;
    const zx_status_t wait_status = thread.wait_one(
        ZX_THREAD_SUSPENDED, zx::deadline_after(zx::msec(50)), &observed);
    if (wait_status != ZX_OK) {
      zx_info_thread_t info = {};
      zx_status_t info_status = thread.get_info(
          ZX_INFO_THREAD, &info, sizeof(info), nullptr, nullptr);
      ZX_LOG(ERROR, wait_status) << "thread failed to suspend";
      LOG(ERROR) << "Thread info status " << info_status;
      if (info_status == ZX_OK) {
        LOG(ERROR) << "Thread state " << info.state;
      }
    }
  }
}

}  // namespace crashpad
