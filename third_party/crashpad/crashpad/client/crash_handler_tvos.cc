// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client/crash_handler_tvos.h"

#include <sys/signal.h>

#include "util/posix/signals.h"
#include "util/thread/thread.h"

namespace crashpad {

CrashHandler* CrashHandler::instance_ = nullptr;

CrashHandler::CrashHandler() = default;

CrashHandler::~CrashHandler() {
  UninstallObjcExceptionPreprocessor();
  for (int signo = 1; signo < NSIG; ++signo) {
    if (!Signals::IsCrashSignal(signo)) {
      Signals::RestoreOrResetHandler(signo,
                                     old_actions_.ActionForSignal(signo));
    } else if (signo == SIGPIPE) {
      // Reset the SIGPIPE handler only if the current handler is the one
      // installed by DoInitialize(). In other words, if an application has set
      // its own SIGPIPE handler after initializing Crashpad, there is no need
      // to change the signal handler here.
      struct sigaction sa;
      if (sigaction(SIGPIPE, nullptr, &sa) == 0 &&
          sa.sa_sigaction == CatchAndReraiseSignal) {
        Signals::InstallDefaultHandler(SIGPIPE);
      }
    }
  }
}

// static
CrashHandler* CrashHandler::Get() {
  if (!instance_) {
    instance_ = new CrashHandler;
  }
  return instance_;
}

// static
void CrashHandler::ResetForTesting() {
  delete instance_;
  instance_ = nullptr;
}

uint64_t CrashHandler::GetThreadIdForTesting() {
  return Thread::GetThreadIdForTesting();
}

bool CrashHandler::DoInitialize() {
  if (!Signals::InstallCrashHandlers(CatchAndReraiseSignal,
                                     /*flags=*/0,
                                     &old_actions_)) {
    return false;
  }

  // For applications that haven't ignored or set a handler for SIGPIPE:
  // It’s OK for an application to set its own SIGPIPE handler (including
  // SIG_IGN) before initializing Crashpad, because Crashpad will discover the
  // existing handler and not install its own.
  // It’s OK for Crashpad to install its own SIGPIPE handler and for the
  // application to subsequently install its own (including SIG_IGN)
  // afterwards, because its handler will replace Crashpad’s.
  // This is useful to cover the default situation where nobody installs a
  // SIGPIPE handler and the disposition is at SIG_DFL, because SIGPIPE is a
  // “kill” signal (bsd/sys/signalvar.h sigprop). In that case, without
  // Crashpad, SIGPIPE results in a silent and unreported kill (and not even
  // ReportCrash will record it), but developers probably want to be alerted to
  // the condition.
  struct sigaction sa;
  if (sigaction(SIGPIPE, nullptr, &sa) == 0 && sa.sa_handler == SIG_DFL) {
    Signals::InstallHandler(SIGPIPE, CatchAndReraiseSignal, 0, nullptr);
  }

  InstallObjcExceptionPreprocessor(this);
  return true;
}

// static
void CrashHandler::CatchAndReraiseSignal(int signo,
                                         siginfo_t* siginfo,
                                         void* context) {
  CrashHandler* self = Get();
  self->HandleAndReraiseSignal(signo,
                               siginfo,
                               reinterpret_cast<ucontext_t*>(context),
                               self->old_actions_.ActionForSignal(signo));
}

}  // namespace crashpad
