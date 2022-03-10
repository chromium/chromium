// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include <mach/mach.h>
#include <stdint.h>

#include <atomic>
#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "client/ios_handler/prune_intermediate_dumps_and_crash_reports_thread.h"
#include "handler/crash_report_upload_thread.h"
#include "snapshot/ios/process_snapshot_ios_intermediate_dump.h"
#include "util/ios/ios_intermediate_dump_writer.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/misc/capture_context.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {
namespace internal {

//! \brief Manage intermediate minidump generation, and own the crash report
//!     upload thread and database.
class InProcessHandler {
 public:
  InProcessHandler();
  ~InProcessHandler();
  InProcessHandler(const InProcessHandler&) = delete;
  InProcessHandler& operator=(const InProcessHandler&) = delete;

  //! \brief Initializes the in-process handler.
  //!
  //! This method must be called only once, and must be successfully called
  //! before any other method in this class may be called.
  //!
  //! \param[in] database The path to a Crashpad database.
  //! \param[in] url The URL of an upload server.
  //! \param[in] annotations Process annotations to set in each crash report.
  //! \param[in] system_data An object containing various system data points.
  //! \return `true` if a handler to a pending intermediate dump could be
  //!     opened.
  bool Initialize(const base::FilePath& database,
                  const std::string& url,
                  const std::map<std::string, std::string>& annotations,
                  const IOSSystemDataCollector& system_data);

  //! \brief Generate an intermediate dump from a signal handler exception.
  //!      Writes the dump with the cached writer does not allow concurrent
  //!      exceptions to be written. It is expected the system will terminate
  //!      the application after this call.
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] siginfo A pointer to a `siginfo_t` object received by a signal
  //!     handler.
  //! \param[in] context A pointer to a `ucontext_t` object received by a
  //!     signal.
  void DumpExceptionFromSignal(const IOSSystemDataCollector& system_data,
                               siginfo_t* siginfo,
                               ucontext_t* context);

  //! \brief Generate an intermediate dump from a mach exception. Writes the
  //!     dump with the cached writer does not allow concurrent exceptions to be
  //!     written. It is expected the system will terminate the application
  //!     after this call.
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] behavior
  //! \param[in] thread
  //! \param[in] exception
  //! \param[in] code
  //! \param[in] code_count
  //! \param[in,out] flavor
  //! \param[in] old_state
  //! \param[in] old_state_count
  void DumpExceptionFromMachException(const IOSSystemDataCollector& system_data,
                                      exception_behavior_t behavior,
                                      thread_t thread,
                                      exception_type_t exception,
                                      const mach_exception_data_type_t* code,
                                      mach_msg_type_number_t code_count,
                                      thread_state_flavor_t flavor,
                                      ConstThreadState old_state,
                                      mach_msg_type_number_t old_state_count);

  //! \brief Generate an intermediate dump from a NSException caught with its
  //!     associated CPU context. Because the method for intercepting
  //!     exceptions is imperfect, uses a new writer for the intermediate dump,
  //!     as it is possible for further exceptions to happen.
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] context
  void DumpExceptionFromNSExceptionWithContext(
      const IOSSystemDataCollector& system_data,
      NativeCPUContext* context);

  //! \brief Generate an intermediate dump from an uncaught NSException.
  //!
  //! When the ObjcExceptionPreprocessor does not detect an NSException as it is
  //! thrown, the last-chance uncaught exception handler passes a list of call
  //! stack frame addresses.  Record them in the intermediate dump so a minidump
  //! with a 'fake' call stack is generated.  Writes the dump with the cached
  //! writer does not allow concurrent exceptions to be written. It is expected
  //! the system will terminate the application after this call.

  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] frames An array of call stack frame addresses.
  //! \param[in] num_frames The number of frames in |frames|.
  void DumpExceptionFromNSExceptionWithFrames(
      const IOSSystemDataCollector& system_data,
      const uint64_t* frames,
      const size_t num_frames);

  //! \brief Generate a simulated intermediate dump similar to a Mach exception
  //!     in the same base directory as other exceptions. Does not use the
  //!     cached writer.
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] context A pointer to a NativeCPUContext object for this
  //!     simulated exception.
  //! \param[out] path The path of the intermediate dump generated.
  //! \return `true` if the pending intermediate dump could be written.
  bool DumpExceptionFromSimulatedMachException(
      const IOSSystemDataCollector& system_data,
      const NativeCPUContext* context,
      base::FilePath* path);

  //! \brief Generate a simulated intermediate dump similar to a Mach exception
  //!     at a specific path. Does not use the cached writer.
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] context A pointer to a NativeCPUContext object for this
  //!     simulated exception.
  //! \param[in] path Path to where the intermediate dump should be written.
  //! \return `true` if the pending intermediate dump could be written.
  bool DumpExceptionFromSimulatedMachExceptionAtPath(
      const IOSSystemDataCollector& system_data,
      const NativeCPUContext* context,
      const base::FilePath& path);

  //! \brief Requests that the handler convert all intermediate dumps into
  //!     minidumps and trigger an upload if possible.
  //!
  //! \param[in] annotations Process annotations to set in each crash report.
  void ProcessIntermediateDumps(
      const std::map<std::string, std::string>& annotations);

  //! \brief Requests that the handler convert a specific intermediate dump into
  //!     a minidump and trigger an upload if possible.
  //!
  //! \param[in] path Path to the specific intermediate dump.
  //! \param[in] annotations Process annotations to set in each crash report.
  void ProcessIntermediateDump(
      const base::FilePath& path,
      const std::map<std::string, std::string>& annotations = {});

  //! \brief Requests that the handler begin in-process uploading of any
  //!     pending reports.
  void StartProcessingPendingReports();

  //! \brief Inject a callback into Mach handling. Intended to be used by
  //!     tests to trigger a reentrant exception.
  void SetMachExceptionCallbackForTesting(void (*callback)()) {
    mach_exception_callback_for_testing_ = callback;
  }

 private:
  //! \brief Helper to start and end intermediate reports.
  class ScopedReport {
   public:
    ScopedReport(IOSIntermediateDumpWriter* writer,
                 const IOSSystemDataCollector& system_data,
                 const std::map<std::string, std::string>& annotations,
                 const uint64_t* frames = nullptr,
                 const size_t num_frames = 0);
    ~ScopedReport();
    ScopedReport(const ScopedReport&) = delete;
    ScopedReport& operator=(const ScopedReport&) = delete;

   private:
    IOSIntermediateDumpWriter* writer_;
    const uint64_t* frames_;
    const size_t num_frames_;
    IOSIntermediateDumpWriter::ScopedRootMap rootMap_;
  };

  //! \brief Helper to manage closing the intermediate dump writer and unlocking
  //!     the dump file (renaming the file) after the report is written.
  class ScopedLockedWriter {
   public:
    ScopedLockedWriter(IOSIntermediateDumpWriter* writer,
                       const char* writer_path,
                       const char* writer_unlocked_path);

    //! \brief Close the writer_ and rename to the file with path without the
    //!     .locked extension.
    ~ScopedLockedWriter();

    ScopedLockedWriter(const ScopedLockedWriter&) = delete;
    ScopedLockedWriter& operator=(const ScopedLockedWriter&) = delete;

    IOSIntermediateDumpWriter* GetWriter() { return writer_; }

   private:
    const char* writer_path_;
    const char* writer_unlocked_path_;
    IOSIntermediateDumpWriter* writer_;
  };

  //! \brief Writes a minidump to the Crashpad database from the
  //!     \a process_snapshot, and triggers the upload_thread_ if started.
  void SaveSnapshot(ProcessSnapshotIOSIntermediateDump& process_snapshot);

  //! \brief Process a maximum of 20 pending intermediate dumps. Dumps named
  //!     with our bundle id get first priority to prevent spamming.
  std::vector<base::FilePath> PendingFiles();

  //! \brief Lock access to the cached intermediate dump writer from
  //!     concurrent signal, Mach exception and uncaught NSExceptions so that
  //!     the first exception wins. If the same thread triggers another
  //!     reentrant exception, ignore it. If a different thread triggers a
  //!     concurrent exception, sleep indefinitely.
  IOSIntermediateDumpWriter* GetCachedWriter();

  //! \brief Open a new intermediate dump writer from \a writer_path.
  std::unique_ptr<IOSIntermediateDumpWriter> CreateWriterWithPath(
      const base::FilePath& writer_path);

  //! \brief Generates a new file path to be used by an intermediate dump
  //! writer built from base_dir_,, bundle_identifier_and_seperator_, a new
  //! UUID, with a .locked extension.
  const base::FilePath NewLockedFilePath();

  // Intended to be used by tests triggering a reentrant exception. Called
  // in DumpExceptionFromMachException after aquiring the cached_writer_.
  void (*mach_exception_callback_for_testing_)() = nullptr;

  bool upload_thread_started_ = false;
  std::map<std::string, std::string> annotations_;
  base::FilePath base_dir_;
  std::string cached_writer_path_;
  std::string cached_writer_unlocked_path_;
  std::unique_ptr<IOSIntermediateDumpWriter> cached_writer_;
  std::atomic<uint64_t> exception_thread_id_ = 0;
  std::unique_ptr<CrashReportUploadThread> upload_thread_;
  std::unique_ptr<PruneIntermediateDumpsAndCrashReportsThread> prune_thread_;
  std::unique_ptr<CrashReportDatabase> database_;
  std::string bundle_identifier_and_seperator_;
  InitializationStateDcheck initialized_;
};

}  // namespace internal
}  // namespace crashpad
