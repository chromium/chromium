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

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "client/ios_handler/prune_intermediate_dumps_and_crash_reports_thread.h"
#include "handler/crash_report_upload_thread.h"
#include "snapshot/ios/process_snapshot_ios_intermediate_dump.h"
#include "util/ios/ios_intermediate_dump_writer.h"
#include "util/ios/ios_system_data_collector.h"
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
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] siginfo A pointer to a `siginfo_t` object received by a signal
  //!     handler.
  //! \param[in] context A pointer to a `ucontext_t` object received by a
  //!     signal.
  void DumpExceptionFromSignal(const IOSSystemDataCollector& system_data,
                               siginfo_t* siginfo,
                               ucontext_t* context);

  //! \brief Generate an intermediate dump from a mach exception.
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

  //! \brief Generate an intermediate dump from an uncaught NSException.
  //!
  //! When the ObjcExceptionPreprocessor does not detect an NSException as it is
  //! thrown, the last-chance uncaught exception handler passes a list of call
  //! stack frame addresses.  Record them in the intermediate dump so a minidump
  //! with a 'fake' call stack is generated.
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] frames An array of call stack frame addresses.
  //! \param[in] num_frames The number of frames in |frames|.
  void DumpExceptionFromNSExceptionFrames(
      const IOSSystemDataCollector& system_data,
      const uint64_t* frames,
      const size_t num_frames);

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

  //! \brief Helper that swaps out the InProcessHandler's |writer_| with an
  //!     alternate writer so DumpWithContext does not interfere with the
  //!     |writer_| created on startup. This is useful for -DumpWithoutCrash,
  //!     which may write to an alternate location.
  class ScopedAlternateWriter {
   public:
    ScopedAlternateWriter(InProcessHandler* handler);
    ~ScopedAlternateWriter();
    ScopedAlternateWriter(const ScopedAlternateWriter&) = delete;
    ScopedAlternateWriter& operator=(const ScopedAlternateWriter&) = delete;
    //! \brief Open's an alternate dump writer in the same directory as the
    //!     default InProcessHandler's dump writer, so the file will be
    //!     processed with -ProcessIntermediateDumps()
    bool Open();

    //! \brief Open's an alternate dump writer in the client provided |path|.
    //!     The file will only be processed by calling
    //!     ProcessIntermediateDump(path)
    bool OpenAtPath(const base::FilePath& path);

    //! \brief The path of the alternate dump writer.
    const base::FilePath& path() { return path_; }

   private:
    InProcessHandler* handler_;
    std::unique_ptr<IOSIntermediateDumpWriter> original_writer_;
    base::FilePath path_;
  };

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

  std::unique_ptr<IOSIntermediateDumpWriter> GetWriter() {
    return std::move(writer_);
  }

  void SetWriter(std::unique_ptr<IOSIntermediateDumpWriter> writer) {
    writer_ = std::move(writer);
  }

  void SetOpenNewFileAfterReport(bool open_new_file_after_report) {
    open_new_file_after_report_ = open_new_file_after_report;
  }

  void SaveSnapshot(ProcessSnapshotIOSIntermediateDump& process_snapshot);

  // Process a maximum of 20 pending intermediate dumps. Dumps named with our
  // bundle id get first priority to prevent spamming.
  std::vector<base::FilePath> PendingFiles();

  bool OpenNewFile();
  void PostReportCleanup();

  bool upload_thread_started_ = false;
  bool open_new_file_after_report_ = true;
  std::map<std::string, std::string> annotations_;
  base::FilePath base_dir_;
  base::FilePath current_file_;
  std::unique_ptr<IOSIntermediateDumpWriter> writer_;
  std::unique_ptr<IOSIntermediateDumpWriter> alternate_mach_writer_;
  std::unique_ptr<CrashReportUploadThread> upload_thread_;
  std::unique_ptr<PruneIntermediateDumpsAndCrashReportsThread> prune_thread_;
  std::unique_ptr<CrashReportDatabase> database_;
  std::string bundle_identifier_and_seperator_;
  InitializationStateDcheck initialized_;
};

}  // namespace internal
}  // namespace crashpad
