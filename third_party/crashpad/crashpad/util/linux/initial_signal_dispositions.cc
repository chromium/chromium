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

#include "util/linux/initial_signal_dispositions.h"

#include <android/api-level.h>
#include <signal.h>

#include "base/logging.h"
#include "build/build_config.h"

namespace crashpad {

#if __ANDROID_API__ <= 23

namespace {
bool LoggingSignal(int signum, sighandler_t handler) {
  sighandler_t previous = signal(signum, handler);
  PLOG_IF(ERROR, previous == SIG_ERR) << "signal " << signum;
  return previous != SIG_ERR;
}
}  // namespace

#endif  // __ANDROID_API__ <= 23

bool InitializeSignalDispositions() {
#if __ANDROID_API__ <= 23
  const int api_level = android_get_device_api_level();
  if (api_level < 0) {
    LOG(WARNING) << "bad api level";
    return false;
  }

  // Bionic installs signal handlers which request crash dumps from Android's
  // debuggerd, but there are errors in how signals which aren't automatically
  // re-raised are handled on Marshmallow (API 23).
  //
  // Before requesting a dump, Bionic acquires a lock to communicate with
  // debuggerd and expecting imminent death, never releases it.
  //
  // While handling the dump request, debuggerd allows the dying process to
  // continue before ptrace-detaching it. So, when Bionic manually re-raises a
  // signal, it is intercepted by debuggerd and the dying process is allowed to
  // live.
  //
  // Bionic restores SIG_DFL for the signal it's just handled, but if a
  // different crash signal is later recieved, Bionic attempts to reacquire the
  // lock to communicate with debuggerd and blocks forever.
  //
  // Disable Bionic's signal handlers for these signals on Marshmallow.
  bool success = true;
  if (api_level == 23) {
    success = LoggingSignal(SIGABRT, SIG_DFL);
    success = LoggingSignal(SIGFPE, SIG_DFL) && success;
    success = LoggingSignal(SIGPIPE, SIG_DFL) && success;
#if defined(SIGSTKFLT)
    success = LoggingSignal(SIGSTKFLT, SIG_DFL) && success;
#endif
    success = LoggingSignal(SIGTRAP, SIG_DFL) && success;
  }

  return success;
#else
  return true;
#endif  // __ANDROID_API__ <= 23
}

}  // namespace crashpad
