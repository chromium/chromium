// Copyright 2020 The Crashpad Authors
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

#include "client/crashpad_client.h"

#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <ios>
#include <iterator>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/logging.h"
#include "client/ios_handler/exception_processor.h"
#include "client/ios_handler/in_process_handler.h"
#include "util/ios/raw_logging.h"
#include "util/mach/exc_server_variants.h"
#include "util/mach/exception_ports.h"
#include "util/mach/mach_extensions.h"
#include "util/mach/mach_message.h"
#include "util/mach/mach_message_server.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/posix/signals.h"
#include "util/thread/thread.h"

namespace {

bool IsBeingDebugged() {
  kinfo_proc kern_proc_info;
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t len = sizeof(kern_proc_info);
  if (sysctl(mib, std::size(mib), &kern_proc_info, &len, nullptr, 0) == 0)
    return kern_proc_info.kp_proc.p_flag & P_TRACED;
  return false;
}

}  // namespace

namespace crashpad {

namespace {

// Thread-safe version of `base::apple::ScopedMachReceiveRight` which allocates
// the Mach port upon construction and deallocates it upon destruction.
class ThreadSafeScopedMachPortWithReceiveRight {
 public:
  ThreadSafeScopedMachPortWithReceiveRight()
      : port_(NewMachPort(MACH_PORT_RIGHT_RECEIVE)) {}

  ThreadSafeScopedMachPortWithReceiveRight(
      const ThreadSafeScopedMachPortWithReceiveRight&) = delete;
  ThreadSafeScopedMachPortWithReceiveRight& operator=(
      const ThreadSafeScopedMachPortWithReceiveRight&) = delete;

  ~ThreadSafeScopedMachPortWithReceiveRight() { reset(); }

  mach_port_t get() { return port_.load(); }
  void reset() {
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

 private:
  std::atomic<mach_port_t> port_;
};

// A base class for signal handler and Mach exception server.
class CrashHandler : public Thread,
                     public UniversalMachExcServer::Interface,
                     public ObjcExceptionDelegate {
 public:
  CrashHandler(const CrashHandler&) = delete;
  CrashHandler& operator=(const CrashHandler&) = delete;

  static CrashHandler* Get() {
    if (!instance_)
      instance_ = new CrashHandler();
    return instance_;
  }

  static void ResetForTesting() {
    delete instance_;
    instance_ = nullptr;
  }

  bool Initialize(
      const base::FilePath& database,
      const std::string& url,
      const std::map<std::string, std::string>& annotations,
      internal::InProcessHandler::ProcessPendingReportsObservationCallback
          callback) {
    INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
    if (!in_process_handler_.Initialize(database, url, annotations, callback) ||
        !InstallMachExceptionHandler() ||
        // xnu turns hardware faults into Mach exceptions, so the only signal
        // left to register is SIGABRT, which never starts off as a hardware
        // fault. Installing a handler for other signals would lead to
        // recording exceptions twice. As a consequence, Crashpad will not
        // generate intermediate dumps for anything manually calling
        // raise(SIG*). In practice, this doesn’t actually happen for crash
        // signals that originate as hardware faults.
        !Signals::InstallHandler(
            SIGABRT, CatchAndReraiseSignal, 0, &old_action_)) {
      LOG(ERROR) << "Unable to initialize Crashpad.";
      return false;
    }

    // For applications that haven't ignored or set a handler for SIGPIPE:
    // It’s OK for an application to set its own SIGPIPE handler (including
    // SIG_IGN) before initializing Crashpad, because Crashpad will discover the
    // existing handler and not install its own.
    // It’s OK for Crashpad to install its own  SIGPIPE handler and for the
    // application to subsequently install its own (including SIG_IGN)
    // afterwards, because its handler will replace Crashpad’s.
    // This is useful to cover the default situation where nobody installs a
    // SIGPIPE  handler and the disposition is at SIG_DFL, because SIGPIPE is a
    // “kill” signal (bsd/sys/signalvar.h  sigprop). In that case, without
    // Crashpad, SIGPIPE results in a silent and unreported kill (and not even
    // ReportCrash will record it), but developers probably want to be alerted
    // to the conditon.
    struct sigaction sa;
    if (sigaction(SIGPIPE, nullptr, &sa) == 0 && sa.sa_handler == SIG_DFL) {
      Signals::InstallHandler(
          SIGPIPE, CatchAndReraiseSignalDefaultAction, 0, nullptr);
    }

    InstallObjcExceptionPreprocessor(this);
    INITIALIZATION_STATE_SET_VALID(initialized_);
    return true;
  }

  void ProcessIntermediateDumps(
      const std::map<std::string, std::string>& annotations) {
    in_process_handler_.ProcessIntermediateDumps(annotations);
  }

  void ProcessIntermediateDump(
      const base::FilePath& file,
      const std::map<std::string, std::string>& annotations) {
    in_process_handler_.ProcessIntermediateDump(file, annotations);
  }

  void DumpWithoutCrash(NativeCPUContext* context, bool process_dump) {
    INITIALIZATION_STATE_DCHECK_VALID(initialized_);
    base::FilePath path;
    if (!in_process_handler_.DumpExceptionFromSimulatedMachException(
            context, kMachExceptionSimulated, &path)) {
      return;
    }

    if (process_dump) {
      in_process_handler_.ProcessIntermediateDump(path);
    }
  }

  void DumpWithoutCrashAtPath(NativeCPUContext* context,
                              const base::FilePath& path) {
    in_process_handler_.DumpExceptionFromSimulatedMachExceptionAtPath(
        context, kMachExceptionSimulated, path);
  }

  void StartProcessingPendingReports(UploadBehavior upload_behavior) {
    INITIALIZATION_STATE_DCHECK_VALID(initialized_);
    in_process_handler_.StartProcessingPendingReports(upload_behavior);
  }

  void SetMachExceptionCallbackForTesting(void (*callback)()) {
    in_process_handler_.SetMachExceptionCallbackForTesting(callback);
  }

  uint64_t GetThreadIdForTesting() { return Thread::GetThreadIdForTesting(); }

 private:
  CrashHandler() = default;

  ~CrashHandler() {
    UninstallObjcExceptionPreprocessor();
    Signals::InstallDefaultHandler(SIGABRT);
    UninstallMachExceptionHandler();
  }

  bool InstallMachExceptionHandler() {
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

  void UninstallMachExceptionHandler() {
    mach_handler_running_ = false;
    exception_port_.reset();
    Join();
  }

  // Thread:

  void ThreadMain() override {
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

  kern_return_t CatchMachException(exception_behavior_t behavior,
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
                                   bool* destroy_complex_request) override {
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

  void HandleMachException(exception_behavior_t behavior,
                           thread_t thread,
                           exception_type_t exception,
                           const mach_exception_data_type_t* code,
                           mach_msg_type_number_t code_count,
                           thread_state_flavor_t flavor,
                           ConstThreadState old_state,
                           mach_msg_type_number_t old_state_count) {
    in_process_handler_.DumpExceptionFromMachException(behavior,
                                                       thread,
                                                       exception,
                                                       code,
                                                       code_count,
                                                       flavor,
                                                       old_state,
                                                       old_state_count);
  }

  void HandleUncaughtNSException(const uint64_t* frames,
                                 const size_t num_frames) override {
    in_process_handler_.DumpExceptionFromNSExceptionWithFrames(frames,
                                                               num_frames);
    // After uncaught exceptions are reported, the system immediately triggers a
    // call to std::terminate()/abort(). Remove the abort handler so a second
    // dump isn't generated.
    CHECK(Signals::InstallDefaultHandler(SIGABRT));
  }

  void HandleUncaughtNSExceptionWithContext(
      NativeCPUContext* context) override {
    base::FilePath path;
    in_process_handler_.DumpExceptionFromSimulatedMachException(
        context, kMachExceptionFromNSException, &path);

    // After uncaught exceptions are reported, the system immediately triggers a
    // call to std::terminate()/abort(). Remove the abort handler so a second
    // dump isn't generated.
    CHECK(Signals::InstallDefaultHandler(SIGABRT));
  }

  void HandleUncaughtNSExceptionWithContextAtPath(
      NativeCPUContext* context,
      const base::FilePath& path) override {
    in_process_handler_.DumpExceptionFromSimulatedMachExceptionAtPath(
        context, kMachExceptionFromNSException, path);
  }

  bool MoveIntermediateDumpAtPathToPending(
      const base::FilePath& path) override {
    if (in_process_handler_.MoveIntermediateDumpAtPathToPending(path)) {
      // After uncaught exceptions are reported, the system immediately triggers
      // a call to std::terminate()/abort(). Remove the abort handler so a
      // second dump isn't generated.
      CHECK(Signals::InstallDefaultHandler(SIGABRT));
      return true;
    }
    return false;
  }

  // The signal handler installed at OS-level.
  static void CatchAndReraiseSignal(int signo,
                                    siginfo_t* siginfo,
                                    void* context) {
    Get()->HandleAndReraiseSignal(signo,
                                  siginfo,
                                  reinterpret_cast<ucontext_t*>(context),
                                  &(Get()->old_action_));
  }

  static void CatchAndReraiseSignalDefaultAction(int signo,
                                                 siginfo_t* siginfo,
                                                 void* context) {
    Get()->HandleAndReraiseSignal(
        signo, siginfo, reinterpret_cast<ucontext_t*>(context), nullptr);
  }

  void HandleAndReraiseSignal(int signo,
                              siginfo_t* siginfo,
                              ucontext_t* context,
                              struct sigaction* old_action) {
    in_process_handler_.DumpExceptionFromSignal(siginfo, context);

    // Always call system handler.
    Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, old_action);
  }

  ThreadSafeScopedMachPortWithReceiveRight exception_port_;
  ExceptionPorts::ExceptionHandlerVector original_handlers_;
  struct sigaction old_action_ = {};
  internal::InProcessHandler in_process_handler_;
  static CrashHandler* instance_;
  std::atomic<bool> mach_handler_running_ = false;
  InitializationStateDcheck initialized_;
};

CrashHandler* CrashHandler::instance_ = nullptr;

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

// static
bool CrashpadClient::StartCrashpadInProcessHandler(
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    ProcessPendingReportsObservationCallback callback) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  return crash_handler->Initialize(database, url, annotations, callback);
}

// static
void CrashpadClient::ProcessIntermediateDumps(
    const std::map<std::string, std::string>& annotations) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->ProcessIntermediateDumps(annotations);
}

// static
void CrashpadClient::ProcessIntermediateDump(
    const base::FilePath& file,
    const std::map<std::string, std::string>& annotations) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->ProcessIntermediateDump(file, annotations);
}

// static
void CrashpadClient::StartProcessingPendingReports(
    UploadBehavior upload_behavior) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->StartProcessingPendingReports(upload_behavior);
}

// static
void CrashpadClient::DumpWithoutCrash(NativeCPUContext* context) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->DumpWithoutCrash(context, /*process_dump=*/true);
}

// static
void CrashpadClient::DumpWithoutCrashAndDeferProcessing(
    NativeCPUContext* context) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->DumpWithoutCrash(context, /*process_dump=*/false);
}

// static
void CrashpadClient::DumpWithoutCrashAndDeferProcessingAtPath(
    NativeCPUContext* context,
    const base::FilePath path) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->DumpWithoutCrashAtPath(context, path);
}

void CrashpadClient::ResetForTesting() {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->ResetForTesting();
}

void CrashpadClient::SetMachExceptionCallbackForTesting(void (*callback)()) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->SetMachExceptionCallbackForTesting(callback);
}

uint64_t CrashpadClient::GetThreadIdForTesting() {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  return crash_handler->GetThreadIdForTesting();
}

}  // namespace crashpad
