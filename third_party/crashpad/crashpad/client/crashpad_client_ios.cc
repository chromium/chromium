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
#include "util/ios/exception_processor.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/mach/exc_server_variants.h"
#include "util/mach/exception_ports.h"
#include "util/mach/mach_extensions.h"
#include "util/mach/mach_message.h"
#include "util/mach/mach_message_server.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/posix/signals.h"
#include "util/thread/thread.h"

namespace crashpad {

namespace {

// A base class for signal handler and Mach exception server.
class CrashHandler : public Thread, public UniversalMachExcServer::Interface {
 public:
  static CrashHandler* Get() {
    static CrashHandler* instance = new CrashHandler();
    return instance;
  }

  void Initialize() {
    INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
    InstallMachExceptionHandler();
    CHECK(Signals::InstallHandler(SIGABRT, CatchSignal, 0, &old_action_));
    INITIALIZATION_STATE_SET_VALID(initialized_);
  }

  void ProcessIntermediateDumps(
      const std::map<std::string, std::string>& annotations = {}) {}

  void ProcessIntermediateDump(
      const base::FilePath& file,
      const std::map<std::string, std::string>& annotations = {}) {}

  void DumpWithoutCrash(NativeCPUContext* context) {
    INITIALIZATION_STATE_DCHECK_VALID(initialized_);
    mach_exception_data_type_t code[2] = {};
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

 private:
  CrashHandler() = default;

  void InstallMachExceptionHandler() {
    exception_port_.reset(NewMachPort(MACH_PORT_RIGHT_RECEIVE));
    CHECK(exception_port_.is_valid());

    kern_return_t kr = mach_port_insert_right(mach_task_self(),
                                              exception_port_.get(),
                                              exception_port_.get(),
                                              MACH_MSG_TYPE_MAKE_SEND);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_insert_right";

    // TODO: Use SwapExceptionPort instead and put back EXC_MASK_BREAKPOINT.
    const exception_mask_t mask =
        ExcMaskAll() &
        ~(EXC_MASK_EMULATION | EXC_MASK_SOFTWARE | EXC_MASK_BREAKPOINT |
          EXC_MASK_RPC_ALERT | EXC_MASK_GUARD);
    ExceptionPorts exception_ports(ExceptionPorts::kTargetTypeTask, TASK_NULL);
    exception_ports.GetExceptionPorts(mask, &original_handlers_);
    exception_ports.SetExceptionPort(
        mask,
        exception_port_.get(),
        EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES,
        MACHINE_THREAD_STATE);

    Start();
  }

  // Thread:

  void ThreadMain() override {
    UniversalMachExcServer universal_mach_exc_server(this);
    while (true) {
      mach_msg_return_t mr =
          MachMessageServer::Run(&universal_mach_exc_server,
                                 exception_port_.get(),
                                 MACH_MSG_OPTION_NONE,
                                 MachMessageServer::kPersistent,
                                 MachMessageServer::kReceiveLargeIgnore,
                                 kMachMessageTimeoutWaitIndefinitely);
      MACH_CHECK(mr == MACH_SEND_INVALID_DEST, mr) << "MachMessageServer::Run";
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
    // TODO(justincohen): This is incomplete.
  }

  // The signal handler installed at OS-level.
  static void CatchSignal(int signo, siginfo_t* siginfo, void* context) {
    Get()->HandleAndReraiseSignal(
        signo, siginfo, reinterpret_cast<ucontext_t*>(context));
  }

  void HandleAndReraiseSignal(int signo,
                              siginfo_t* siginfo,
                              ucontext_t* context) {
    // TODO(justincohen): This is incomplete.

    // Always call system handler.
    Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, &old_action_);
  }

  base::mac::ScopedMachReceiveRight exception_port_;
  ExceptionPorts::ExceptionHandlerVector original_handlers_;
  struct sigaction old_action_ = {};
  internal::IOSSystemDataCollector system_data_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(CrashHandler);
};

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

// static
void CrashpadClient::StartCrashpadInProcessHandler(
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations) {
  InstallObjcExceptionPreprocessor();

  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->Initialize();
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
  // TODO(justincohen): Start the CrashReportUploadThread.
}

// static
void CrashpadClient::DumpWithoutCrash(NativeCPUContext* context) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->DumpWithoutCrash(context);
  // TODO(justincohen): Change this to only process the dump from above, not all
  // intermediate dump files.
  crash_handler->ProcessIntermediateDumps();
}

// static
void CrashpadClient::DumpWithoutCrashAndDeferProcessing(
    NativeCPUContext* context) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  crash_handler->DumpWithoutCrash(context);
}

// static
void CrashpadClient::DumpWithoutCrashAndDeferProcessingAtPath(
    NativeCPUContext* context,
    const base::FilePath path) {
  CrashHandler* crash_handler = CrashHandler::Get();
  DCHECK(crash_handler);
  // TODO(justincohen): Change to DumpWithoutCrashAtPath(context, path).
  crash_handler->DumpWithoutCrash(context);
}

}  // namespace crashpad
