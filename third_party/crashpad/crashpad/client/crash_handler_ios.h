// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASHPAD_CLIENT_CRASH_HANDLER_IOS_H_
#define CRASHPAD_CLIENT_CRASH_HANDLER_IOS_H_

#include <atomic>

#include "client/crash_handler_base_ios.h"
#include "util/mach/exc_server_variants.h"
#include "util/mach/exception_ports.h"
#include "util/thread/thread.h"

namespace crashpad {

// A base class for signal handler and Mach exception server.
class CrashHandler : public Thread,
                     public UniversalMachExcServer::Interface,
                     public CrashHandlerBase {
 public:
  CrashHandler(const CrashHandler&) = delete;
  CrashHandler& operator=(const CrashHandler&) = delete;

  static CrashHandler* Get();

  static void ResetForTesting();

 private:
  // Thread-safe version of `base::apple::ScopedMachReceiveRight` which
  // allocates the Mach port upon construction and deallocates it upon
  // destruction.
  class ThreadSafeScopedMachPortWithReceiveRight {
   public:
    ThreadSafeScopedMachPortWithReceiveRight();

    ThreadSafeScopedMachPortWithReceiveRight(
        const ThreadSafeScopedMachPortWithReceiveRight&) = delete;
    ThreadSafeScopedMachPortWithReceiveRight& operator=(
        const ThreadSafeScopedMachPortWithReceiveRight&) = delete;

    ~ThreadSafeScopedMachPortWithReceiveRight();

    mach_port_t get();
    void reset();

   private:
    std::atomic<mach_port_t> port_;
  };

  CrashHandler();

  ~CrashHandler() override;

  bool DoInitialize() override;

  bool InstallMachExceptionHandler();

  void UninstallMachExceptionHandler();

  // Thread:

  void ThreadMain() override;

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
                                   bool* destroy_complex_request) override;

  void HandleMachException(exception_behavior_t behavior,
                           thread_t thread,
                           exception_type_t exception,
                           const mach_exception_data_type_t* code,
                           mach_msg_type_number_t code_count,
                           thread_state_flavor_t flavor,
                           ConstThreadState old_state,
                           mach_msg_type_number_t old_state_count);

  // The signal handler installed at OS-level.
  static void CatchAndReraiseSignal(int signo,
                                    siginfo_t* siginfo,
                                    void* context);

  static void CatchAndReraiseSignalDefaultAction(int signo,
                                                 siginfo_t* siginfo,
                                                 void* context);

  ThreadSafeScopedMachPortWithReceiveRight exception_port_;
  ExceptionPorts::ExceptionHandlerVector original_handlers_;
  struct sigaction old_action_ = {};
  static CrashHandler* instance_;
  std::atomic<bool> mach_handler_running_ = false;
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_CRASH_HANDLER_IOS_H_
