// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client/crash_handler_base_ios.h"

#include "base/logging.h"
#include "util/posix/signals.h"

namespace crashpad {

CrashHandlerBase::CrashHandlerBase() = default;
CrashHandlerBase::~CrashHandlerBase() = default;

bool CrashHandlerBase::Initialize(
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    internal::InProcessHandler::ProcessPendingReportsObservationCallback
        callback) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  if (!in_process_handler().Initialize(database, url, annotations, callback)) {
    LOG(ERROR) << "Unable to initialize Crashpad.";
    return false;
  }
  if (!DoInitialize()) {
    return false;
  }
  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void CrashHandlerBase::ProcessIntermediateDumps(
    const std::map<std::string, std::string>& annotations,
    const UserStreamDataSources* user_stream_sources) {
  in_process_handler_.ProcessIntermediateDumps(annotations,
                                               user_stream_sources);
}

void CrashHandlerBase::ProcessIntermediateDump(
    const base::FilePath& file,
    const std::map<std::string, std::string>& annotations) {
  in_process_handler_.ProcessIntermediateDump(file, annotations);
}

void CrashHandlerBase::DumpWithoutCrash(NativeCPUContext* context,
                                        bool process_dump) {
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

void CrashHandlerBase::DumpWithoutCrashAtPath(NativeCPUContext* context,
                                              const base::FilePath& path) {
  in_process_handler_.DumpExceptionFromSimulatedMachExceptionAtPath(
      context, kMachExceptionSimulated, path);
}

void CrashHandlerBase::StartProcessingPendingReports(
    UploadBehavior upload_behavior) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  in_process_handler_.StartProcessingPendingReports(upload_behavior);
}

void CrashHandlerBase::SetExceptionCallbackForTesting(void (*callback)()) {
  in_process_handler_.SetExceptionCallbackForTesting(callback);
}

void CrashHandlerBase::HandleAndReraiseSignal(int signo,
                                              siginfo_t* siginfo,
                                              ucontext_t* context,
                                              struct sigaction* old_action) {
  in_process_handler_.DumpExceptionFromSignal(siginfo, context);

  // Always call system handler.
  Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, old_action);
}

void CrashHandlerBase::HandleUncaughtNSException(const uint64_t* frames,
                                                 const size_t num_frames) {
  in_process_handler_.DumpExceptionFromNSExceptionWithFrames(frames,
                                                             num_frames);
  // After uncaught exceptions are reported, the system immediately triggers a
  // call to std::terminate()/abort(). Remove the abort handler so a second
  // dump isn't generated.
  CHECK(Signals::InstallDefaultHandler(SIGABRT));
}

void CrashHandlerBase::HandleUncaughtNSExceptionWithContext(
    NativeCPUContext* context) {
  base::FilePath path;
  in_process_handler_.DumpExceptionFromSimulatedMachException(
      context, kMachExceptionFromNSException, &path);

  // After uncaught exceptions are reported, the system immediately triggers a
  // call to std::terminate()/abort(). Remove the abort handler so a second
  // dump isn't generated.
  CHECK(Signals::InstallDefaultHandler(SIGABRT));
}

void CrashHandlerBase::HandleUncaughtNSExceptionWithContextAtPath(
    NativeCPUContext* context,
    const base::FilePath& path) {
  in_process_handler_.DumpExceptionFromSimulatedMachExceptionAtPath(
      context, kMachExceptionFromNSException, path);
}

bool CrashHandlerBase::MoveIntermediateDumpAtPathToPending(
    const base::FilePath& path) {
  if (in_process_handler_.MoveIntermediateDumpAtPathToPending(path)) {
    // After uncaught exceptions are reported, the system immediately triggers
    // a call to std::terminate()/abort(). Remove the abort handler so a
    // second dump isn't generated.
    CHECK(Signals::InstallDefaultHandler(SIGABRT));
    return true;
  }
  return false;
}

}  // namespace crashpad
