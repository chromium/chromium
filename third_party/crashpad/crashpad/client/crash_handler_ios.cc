// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client/crash_handler_ios.h"

#include <sys/sysctl.h>
#include <unistd.h>

#include <iterator>

#include "base/apple/mach_logging.h"
#include "util/ios/raw_logging.h"
#include "util/mach/mach_extensions.h"
#include "util/mach/mach_message.h"
#include "util/mach/mach_message_server.h"
#include "util/posix/signals.h"

namespace {

bool IsBeingDebugged() {
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t len = sizeof(kinfo_proc);
  kinfo_proc kern_proc_info;
  if (sysctl(mib, std::size(mib), &kern_proc_info, &len, nullptr, 0) == 0) {
    return kern_proc_info.kp_proc.p_flag & P_TRACED;
  }
  return false;
}

}  // namespace

namespace crashpad {

CrashHandler::ThreadSafeScopedMachPortWithReceiveRight::
    ThreadSafeScopedMachPortWithReceiveRight()
    : port_(NewMachPort(MACH_PORT_RIGHT_RECEIVE)) {}

CrashHandler::ThreadSafeScopedMachPortWithReceiveRight::
    ~ThreadSafeScopedMachPortWithReceiveRight() {
  reset();
}

mach_port_t CrashHandler::ThreadSafeScopedMachPortWithReceiveRight::get() {
  return port_.load();
}
void CrashHandler::ThreadSafeScopedMachPortWithReceiveRight::reset() {
  mach_port_t old_port = port_.exchange(MACH_PORT_NULL);
  if (old_port == MACH_PORT_NULL) {
    // Already reset, nothing to do.
    return;
  }
  kern_return_t kr = mach_port_mod_refs(
      mach_task_self(), old_port, MACH_PORT_RIGHT_RECEIVE, -1);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "ThreadSafeScopedMachPortWithReceiveRight mach_port_mod_refs";
  kr = mach_port_deallocate(mach_task_self(), old_port);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "ThreadSafeScopedMachPortWithReceiveRight mach_port_deallocate";
}

CrashHandler* CrashHandler::instance_ = nullptr;

CrashHandler::CrashHandler() = default;

CrashHandler::~CrashHandler() {
  UninstallObjcExceptionPreprocessor();
  // Reset the SIGPIPE handler only if the current handler is the
  // one installed by DoInitialize(). In other words, if an
  // application has set its own SIGPIPE handler after initializing
  // Crashpad, there is no need to change the signal handler here.
  struct sigaction sa;
  if (sigaction(SIGPIPE, nullptr, &sa) == 0 &&
      sa.sa_sigaction == CatchAndReraiseSignalDefaultAction) {
    Signals::InstallDefaultHandler(SIGPIPE);
  }
  Signals::InstallDefaultHandler(SIGABRT);
  UninstallMachExceptionHandler();
}

// static
CrashHandler* CrashHandler::Get() {
  if (!instance_)
    instance_ = new CrashHandler();
  return instance_;
}

// static
void CrashHandler::ResetForTesting() {
  delete instance_;
  instance_ = nullptr;
}

bool CrashHandler::DoInitialize() {
  if (!InstallMachExceptionHandler() ||
      // xnu turns hardware faults into Mach exceptions, so the only signal
      // left to register is SIGABRT, which never starts off as a hardware
      // fault. Installing a handler for other signals would lead to
      // recording exceptions twice. As a consequence, Crashpad will not
      // generate intermediate dumps for anything manually calling
      // raise(SIG*). In practice, this doesn’t actually happen for crash
      // signals that originate as hardware faults.
      !Signals::InstallHandler(
          SIGABRT, CatchAndReraiseSignal, 0, &old_action_)) {
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
    Signals::InstallHandler(
        SIGPIPE, CatchAndReraiseSignalDefaultAction, 0, nullptr);
  }

  InstallObjcExceptionPreprocessor(this);
  return true;
}

bool CrashHandler::InstallMachExceptionHandler() {
  mach_port_t exception_port = exception_port_.get();
  if (exception_port == MACH_PORT_NULL) {
    return false;
  }

  kern_return_t kr = mach_port_insert_right(mach_task_self(),
                                            exception_port,
                                            exception_port,
                                            MACH_MSG_TYPE_MAKE_SEND);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_port_insert_right";
    return false;
  }

  // TODO: Use SwapExceptionPort instead and put back EXC_MASK_BREAKPOINT.
  // Until then, remove |EXC_MASK_BREAKPOINT| while attached to a debugger.
  exception_mask_t mask =
      ExcMaskAll() &
      ~(EXC_MASK_EMULATION | EXC_MASK_SOFTWARE | EXC_MASK_RPC_ALERT |
        EXC_MASK_GUARD | (IsBeingDebugged() ? EXC_MASK_BREAKPOINT : 0));

  ExceptionPorts exception_ports(ExceptionPorts::kTargetTypeTask, TASK_NULL);
  if (!exception_ports.GetExceptionPorts(mask, &original_handlers_) ||
      !exception_ports.SetExceptionPort(
          mask,
          exception_port,
          EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES,
          MACHINE_THREAD_STATE)) {
    return false;
  }

  mach_handler_running_ = true;
  Start();
  return true;
}

void CrashHandler::UninstallMachExceptionHandler() {
  mach_handler_running_ = false;
  exception_port_.reset();
  Join();
}

// Thread:

void CrashHandler::ThreadMain() {
  UniversalMachExcServer universal_mach_exc_server(this);
  while (mach_handler_running_) {
    mach_msg_return_t mr =
        MachMessageServer::Run(&universal_mach_exc_server,
                               exception_port_.get(),
                               MACH_MSG_OPTION_NONE,
                               MachMessageServer::kPersistent,
                               MachMessageServer::kReceiveLargeIgnore,
                               kMachMessageTimeoutWaitIndefinitely);
    MACH_CHECK(
        mach_handler_running_
            ? mr == MACH_SEND_INVALID_DEST  // This shouldn't happen for
                                            // exception messages that come
                                            // from the kernel itself, but if
                                            // something else in-process sends
                                            // exception messages and breaks,
                                            // handle that case.
            : (mr == MACH_RCV_PORT_CHANGED ||  // Port was closed while the
                                               // thread was listening.
               mr == MACH_RCV_INVALID_NAME),  // Port was closed before the
                                              // thread started listening.
        mr)
        << "MachMessageServer::Run";
  }
}

// UniversalMachExcServer::Interface:

kern_return_t CrashHandler::CatchMachException(
    exception_behavior_t behavior,
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    ConstThreadState old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    const mach_msg_trailer_t* trailer,
    bool* destroy_complex_request) {
  *destroy_complex_request = true;

  // TODO(justincohen): Forward exceptions to original_handlers_ with
  // UniversalExceptionRaise.

  // iOS shouldn't have any child processes, but just in case, those will
  // inherit the task exception ports, and this process isn’t prepared to
  // handle them
  if (task != mach_task_self()) {
    CRASHPAD_RAW_LOG("MachException task != mach_task_self()");
    return KERN_FAILURE;
  }

  HandleMachException(behavior,
                      thread,
                      exception,
                      code,
                      code_count,
                      *flavor,
                      old_state,
                      old_state_count);

  // Respond with KERN_FAILURE so the system will continue to handle this
  // exception. xnu will turn this Mach exception into a signal and take the
  // default action to terminate the process. However, if sigprocmask is
  // called before this Mach exception returns (such as by another thread
  // calling abort, see: Libc-1506.40.4/stdlib/FreeBSD/abort.c), the Mach
  // exception will be converted into a signal but delivery will be blocked.
  // Since concurrent exceptions lead to the losing thread sleeping
  // indefinitely, if the abort thread never returns, the thread that
  // triggered this Mach exception will repeatedly trap and the process will
  // never terminate. If the abort thread didn’t have a user-space signal
  // handler that slept forever, the abort would terminate the process even if
  // all other signals had been blocked. Instead, unblock all signals
  // corresponding to all Mach exceptions Crashpad is registered for before
  // returning KERN_FAILURE. There is still racy behavior possible with this
  // call to sigprocmask, but the repeated calls to CatchMachException here
  // will eventually lead to termination.
  sigset_t unblock_set;
  sigemptyset(&unblock_set);
  sigaddset(&unblock_set, SIGILL);  // EXC_BAD_INSTRUCTION
  sigaddset(&unblock_set, SIGTRAP);  // EXC_BREAKPOINT
  sigaddset(&unblock_set, SIGFPE);  // EXC_ARITHMETIC
  sigaddset(&unblock_set, SIGBUS);  // EXC_BAD_ACCESS
  sigaddset(&unblock_set, SIGSEGV);  // EXC_BAD_ACCESS
  if (sigprocmask(SIG_UNBLOCK, &unblock_set, nullptr) != 0) {
    CRASHPAD_RAW_LOG("sigprocmask");
  }
  return KERN_FAILURE;
}

void CrashHandler::HandleMachException(exception_behavior_t behavior,
                                       thread_t thread,
                                       exception_type_t exception,
                                       const mach_exception_data_type_t* code,
                                       mach_msg_type_number_t code_count,
                                       thread_state_flavor_t flavor,
                                       ConstThreadState old_state,
                                       mach_msg_type_number_t old_state_count) {
  in_process_handler().DumpExceptionFromMachException(behavior,
                                                      thread,
                                                      exception,
                                                      code,
                                                      code_count,
                                                      flavor,
                                                      old_state,
                                                      old_state_count);
}

// static
void CrashHandler::CatchAndReraiseSignal(int signo,
                                         siginfo_t* siginfo,
                                         void* context) {
  Get()->HandleAndReraiseSignal(signo,
                                siginfo,
                                reinterpret_cast<ucontext_t*>(context),
                                &(Get()->old_action_));
}

// static
void CrashHandler::CatchAndReraiseSignalDefaultAction(int signo,
                                                      siginfo_t* siginfo,
                                                      void* context) {
  Get()->HandleAndReraiseSignal(
      signo, siginfo, reinterpret_cast<ucontext_t*>(context), nullptr);
}

}  // namespace crashpad
