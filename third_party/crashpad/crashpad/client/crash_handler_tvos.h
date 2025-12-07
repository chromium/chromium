// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASHPAD_CLIENT_CRASH_HANDLER_TVOS_H_
#define CRASHPAD_CLIENT_CRASH_HANDLER_TVOS_H_

#include "client/crash_handler_base_ios.h"
#include "util/posix/signals.h"

namespace crashpad {

// A crash handler based on POSIX signals.
// The APIs to handle Mach exceptions are not available to third-party
// applications on tvOS.
class CrashHandler final : public CrashHandlerBase {
 public:
  CrashHandler(const CrashHandler&) = delete;
  CrashHandler& operator=(const CrashHandler&) = delete;

  static CrashHandler* Get();

  static void ResetForTesting();

  uint64_t GetThreadIdForTesting();

 private:
  CrashHandler();
  ~CrashHandler() override;

  bool DoInitialize() override;

  // The signal handler installed at OS-level.
  static void CatchAndReraiseSignal(int signo,
                                    siginfo_t* siginfo,
                                    void* context);

  Signals::OldActions old_actions_ = {};
  static CrashHandler* instance_;
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_CRASH_HANDLER_TVOS_H_
