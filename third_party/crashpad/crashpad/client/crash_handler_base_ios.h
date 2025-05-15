// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASHPAD_CLIENT_CRASH_HANDLER_BASE_IOS_H_
#define CRASHPAD_CLIENT_CRASH_HANDLER_BASE_IOS_H_

#include <stdint.h>
#include <sys/signal.h>

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "client/ios_handler/exception_processor.h"
#include "client/ios_handler/in_process_handler.h"
#include "client/upload_behavior_ios.h"
#include "util/misc/capture_context.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

// Base class shared by the iOS and tvOS CrashHandler implementations.
class CrashHandlerBase : public ObjcExceptionDelegate {
 public:
  CrashHandlerBase(const CrashHandlerBase&) = delete;
  CrashHandlerBase& operator=(const CrashHandlerBase&) = delete;

  bool Initialize(
      const base::FilePath& database,
      const std::string& url,
      const std::map<std::string, std::string>& annotations,
      internal::InProcessHandler::ProcessPendingReportsObservationCallback
          callback);

  void ProcessIntermediateDumps(
      const std::map<std::string, std::string>& annotations,
      const UserStreamDataSources* user_stream_sources);

  void ProcessIntermediateDump(
      const base::FilePath& file,
      const std::map<std::string, std::string>& annotations);

  void DumpWithoutCrash(NativeCPUContext* context, bool process_dump);

  void DumpWithoutCrashAtPath(NativeCPUContext* context,
                              const base::FilePath& path);

  void StartProcessingPendingReports(UploadBehavior upload_behavior);

  void SetExceptionCallbackForTesting(void (*callback)());

 protected:
  CrashHandlerBase();
  virtual ~CrashHandlerBase();

  // Subclasses are expected to install signal handlers and set up Mach ports in
  // this function.
  virtual bool DoInitialize() = 0;

  void HandleAndReraiseSignal(int signo,
                              siginfo_t* siginfo,
                              ucontext_t* context,
                              struct sigaction* old_action);

  internal::InProcessHandler& in_process_handler() {
    return in_process_handler_;
  }

 private:
  // ObjcExceptionDelegate overrides:
  void HandleUncaughtNSException(const uint64_t* frames,
                                 const size_t num_frames) override;

  void HandleUncaughtNSExceptionWithContext(NativeCPUContext* context) override;

  void HandleUncaughtNSExceptionWithContextAtPath(
      NativeCPUContext* context,
      const base::FilePath& path) override;

  bool MoveIntermediateDumpAtPathToPending(const base::FilePath& path) override;

  internal::InProcessHandler in_process_handler_;
  InitializationStateDcheck initialized_;
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_CRASH_HANDLER_BASE_IOS_H_
