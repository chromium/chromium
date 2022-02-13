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

#include "client/crashpad_client.h"

#include <unistd.h>

#include <ios>

#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "client/ios_handler/exception_processor.h"
#include "client/ios_handler/in_process_handler.h"
#include "util/ios/ios_system_data_collector.h"
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
  if (sysctl(mib, base::size(mib), &kern_proc_info, &len, nullptr, 0) == 0)
    return kern_proc_info.kp_proc.p_flag & P_TRACED;
  return false;
}

}  // namespace

namespace crashpad {

namespace {

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

  bool Initialize(const base::FilePath& database,
                  const std::string& url,
                  const std::map<std::string, std::string>& annotations) {
    INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
    if (!in_process_handler_.Initialize(
            database, url, annotations, system_data_) ||
        !InstallMachExceptionHandler() ||
        // xnu turns hardware faults into Mach exceptions, so the only signal
        // left to register is SIGABRT, which never starts off as a hardware
        // fault. Installing a handler for other signals would lead to
        // recording exceptions twice. As a consequence, Crashpad will not
        // generate intermediate dumps for anything manually calling
        // raise(SIG*). In practice, this doesn’t actually happen for crash
        // signals that originate as hardware faults.
        !Signals::InstallHandler(SIGABRT, CatchSignal, 0, &old_action_)) {
      LOG(ERROR) << "Unable to initialize Crashpad.";
      return false;
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

  void DumpWithContext(NativeCPUContext* context) {
    const mach_exception_data_type_t code[2] = {};
    static constexpr int kSimulatedException = -1;
    HandleMachException(MACH_EXCEPTION_CODES,
                        mach_thread_self(),
                        kSimulatedException,
                        code,
                        base::size(code),
                        MACHINE_THREAD_STATE,
                        reinterpret_cast<ConstThreadState>(context),
                        MACHINE_THREAD_STATE_COUNT);
  }

  void DumpWithoutCrash(NativeCPUContext* context, bool process_dump) {
    INITIALIZATION_STATE_DCHECK_VALID(initialized_);
    base::FilePath path;
    {
      // Ensure ScopedAlternateWriter's destructor is invoked before processing
      // the dump, or else any crashes handled during dump processing cannot be
      // written.
      internal::InProcessHandler::ScopedAlternateWriter scoper(
          &in_process_handler_);
      if (!scoper.Open()) {
        LOG(ERROR) << "Could not open writer, ignoring dump request.";
        return;
      }
      DumpWithContext(context);
      path = scoper.path();
    }
    if (process_dump) {
      in_process_handler_.ProcessIntermediateDump(path);
    }
  }

  void DumpWithoutCrashAtPath(NativeCPUContext* context,
                              const base::FilePath& path) {
    internal::InProcessHandler::ScopedAlternateWriter scoper(
        &in_process_handler_);
    if (scoper.OpenAtPath(path))
      DumpWithContext(context);
  }

  void StartProcessingPendingReports() {
    INITIALIZATION_STATE_DCHECK_VALID(initialized_);
    in_process_handler_.StartProcessingPendingReports();
  }

 private:
  CrashHandler() = default;

  ~CrashHandler() {
    UninstallObjcExceptionPreprocessor();
    Signals::InstallDefaultHandler(SIGABRT);
    UninstallMachExceptionHandler();
  }

  bool InstallMachExceptionHandler() {
    exception_port_.reset(NewMachPort(MACH_PORT_RIGHT_RECEIVE));
    if (!exception_port_.is_valid()) {
      return false;
    }

    kern_return_t kr = mach_port_insert_right(mach_task_self(),
                                              exception_port_.get(),
                                              exception_port_.get(),
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
            exception_port_.get(),
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
      MACH_CHECK(mr == (mach_handler_running_ ? MACH_SEND_INVALID_DEST
                                              : MACH_RCV_PORT_CHANGED),
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
      LOG(WARNING) << "task 0x" << std::hex << task << " != 0x"
                   << mach_task_self();
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
    // exception as a crash.
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
    in_process_handler_.DumpExceptionFromMachException(system_data_,
                                                       behavior,
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
    in_process_handler_.DumpExceptionFromNSExceptionFrames(
        system_data_, frames, num_frames);
    // After uncaught exceptions are reported, the system immediately triggers a
    // call to std::terminate()/abort(). Remove the abort handler so a second
    // dump isn't generated.
    CHECK(Signals::InstallDefaultHandler(SIGABRT));
  }

  void HandleUncaughtNSExceptionWithContext(
      NativeCPUContext* context) override {
    const mach_exception_data_type_t code[2] = {0, 0};
    in_process_handler_.DumpExceptionFromMachException(
        system_data_,
        MACH_EXCEPTION_CODES,
        mach_thread_self(),
        kMachExceptionFromNSException,
        code,
        base::size(code),
        MACHINE_THREAD_STATE,
        reinterpret_cast<ConstThreadState>(context),
        MACHINE_THREAD_STATE_COUNT);

    // After uncaught exceptions are reported, the system immediately triggers a
    // call to std::terminate()/abort(). Remove the abort handler so a second
    // dump isn't generated.
    CHECK(Signals::InstallDefaultHandler(SIGABRT));
  }

  // The signal handler installed at OS-level.
  static void CatchSignal(int signo, siginfo_t* siginfo, void* context) {
    Get()->HandleAndReraiseSignal(
        signo, siginfo, reinterpret_cast<ucontext_t*>(context));
  }

  void HandleAndReraiseSignal(int signo,
                              siginfo_t* siginfo,
                              ucontext_t* context) {
    in_process_handler_.DumpExceptionFromSignal(system_data_, siginfo, context);

    // Always call system handler.
    Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, &old_action_);
  }

  base::mac::ScopedMachReceiveRight exception_port_;
  ExceptionPorts::ExceptionHandlerVector original_handlers_;
  struct sigaction old_action_ = {};
  internal::InProcessHandler in_process_handler_;
  internal::IOSSystemDataCollector system_data_;
  static CrashHandler* instance_;
  bool mach_handler_running_ = false;
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
    const std::map<std::string, std::string>& annotations) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  return crash_handler->Initialize(database, url, annotations);
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
void CrashpadClient::StartProcessingPendingReports() {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->StartProcessingPendingReports();
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

}  // namespace crashpad
