// Copyright 2021 The Crashpad Authors
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

#include "client/ios_handler/in_process_handler.h"

#include <stdio.h>
#include <sys/stat.h>

#include <algorithm>

#include "base/logging.h"
#include "client/ios_handler/in_process_intermediate_dump_handler.h"
#include "client/prune_crash_reports.h"
#include "client/settings.h"
#include "minidump/minidump_file_writer.h"
#include "util/file/directory_reader.h"
#include "util/file/filesystem.h"
#include "util/ios/raw_logging.h"

namespace {

// Creates directory at |path|.
bool CreateDirectory(const base::FilePath& path) {
  if (mkdir(path.value().c_str(), 0755) == 0) {
    return true;
  }
  if (errno != EEXIST) {
    PLOG(ERROR) << "mkdir " << path.value();
    return false;
  }
  return true;
}

// The file extension used to indicate a file is locked.
constexpr char kLockedExtension[] = ".locked";

// The seperator used to break the bundle id (e.g. com.chromium.ios) from the
// uuid in the intermediate dump file name.
constexpr char kBundleSeperator[] = "@";

// Zero-ed codes used by kMachExceptionFromNSException and
// kMachExceptionSimulated.
constexpr mach_exception_data_type_t kEmulatedMachExceptionCodes[2] = {};

}  // namespace

namespace crashpad {
namespace internal {

InProcessHandler::InProcessHandler() = default;

InProcessHandler::~InProcessHandler() {
  if (cached_writer_) {
    cached_writer_->Close();
  }
  UpdatePruneAndUploadThreads(false, UploadBehavior::kUploadWhenAppIsActive);
}

bool InProcessHandler::Initialize(
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    ProcessPendingReportsObservationCallback callback) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  annotations_ = annotations;
  database_ = CrashReportDatabase::Initialize(database);
  if (!database_) {
    return false;
  }
  bundle_identifier_and_seperator_ =
      system_data_.BundleIdentifier() + kBundleSeperator;

  if (!url.empty()) {
    // TODO(scottmg): options.rate_limit should be removed when we have a
    // configurable database setting to control upload limiting.
    // See https://crashpad.chromium.org/bug/23.
    CrashReportUploadThread::Options upload_thread_options;
    upload_thread_options.rate_limit = false;
    upload_thread_options.upload_gzip = true;
    upload_thread_options.watch_pending_reports = true;
    upload_thread_options.identify_client_via_url = true;

    upload_thread_.reset(new CrashReportUploadThread(
        database_.get(), url, upload_thread_options, callback));
  }

  if (!CreateDirectory(database))
    return false;
  static constexpr char kPendingSerializediOSDump[] =
      "pending-serialized-ios-dump";
  base_dir_ = database.Append(kPendingSerializediOSDump);
  if (!CreateDirectory(base_dir_))
    return false;

  bool is_app_extension = system_data_.IsExtension();
  prune_thread_.reset(new PruneIntermediateDumpsAndCrashReportsThread(
      database_.get(),
      PruneCondition::GetDefault(),
      base_dir_,
      bundle_identifier_and_seperator_,
      is_app_extension));
  if (is_app_extension || system_data_.IsApplicationActive())
    prune_thread_->Start();

  if (!is_app_extension) {
    system_data_.SetActiveApplicationCallback([this](bool active) {
      dispatch_async(
          dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            UpdatePruneAndUploadThreads(active,
                                        UploadBehavior::kUploadWhenAppIsActive);
          });
    });
  }

  base::FilePath cached_writer_path = NewLockedFilePath();
  cached_writer_ = CreateWriterWithPath(cached_writer_path);
  if (!cached_writer_.get())
    return false;

  // Cache the locked and unlocked path here so no allocations are needed during
  // any exceptions.
  cached_writer_path_ = cached_writer_path.value();
  cached_writer_unlocked_path_ =
      cached_writer_path.RemoveFinalExtension().value();
  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void InProcessHandler::DumpExceptionFromSignal(siginfo_t* siginfo,
                                               ucontext_t* context) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  ScopedLockedWriter writer(GetCachedWriter(),
                            cached_writer_path_.c_str(),
                            cached_writer_unlocked_path_.c_str());
  if (!writer.GetWriter()) {
    CRASHPAD_RAW_LOG("Cannot DumpExceptionFromSignal without writer");
    return;
  }
  ScopedReport report(writer.GetWriter(), system_data_, annotations_);
  InProcessIntermediateDumpHandler::WriteExceptionFromSignal(
      writer.GetWriter(), system_data_, siginfo, context);
}

void InProcessHandler::DumpExceptionFromMachException(
    exception_behavior_t behavior,
    thread_t thread,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t flavor,
    ConstThreadState old_state,
    mach_msg_type_number_t old_state_count) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  ScopedLockedWriter writer(GetCachedWriter(),
                            cached_writer_path_.c_str(),
                            cached_writer_unlocked_path_.c_str());
  if (!writer.GetWriter()) {
    CRASHPAD_RAW_LOG("Cannot DumpExceptionFromMachException without writer");
    return;
  }

  if (mach_exception_callback_for_testing_) {
    mach_exception_callback_for_testing_();
  }

  ScopedReport report(writer.GetWriter(), system_data_, annotations_);
  InProcessIntermediateDumpHandler::WriteExceptionFromMachException(
      writer.GetWriter(),
      behavior,
      thread,
      exception,
      code,
      code_count,
      flavor,
      old_state,
      old_state_count);
}

void InProcessHandler::DumpExceptionFromNSExceptionWithFrames(
    const uint64_t* frames,
    const size_t num_frames) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  ScopedLockedWriter writer(GetCachedWriter(),
                            cached_writer_path_.c_str(),
                            cached_writer_unlocked_path_.c_str());
  if (!writer.GetWriter()) {
    CRASHPAD_RAW_LOG(
        "Cannot DumpExceptionFromNSExceptionWithFrames without writer");
    return;
  }
  ScopedReport report(
      writer.GetWriter(), system_data_, annotations_, frames, num_frames);
  InProcessIntermediateDumpHandler::WriteExceptionFromNSException(
      writer.GetWriter());
}

bool InProcessHandler::DumpExceptionFromSimulatedMachException(
    const NativeCPUContext* context,
    exception_type_t exception,
    base::FilePath* path) {
  base::FilePath locked_path = NewLockedFilePath();
  *path = locked_path.RemoveFinalExtension();
  return DumpExceptionFromSimulatedMachExceptionAtPath(
      context, exception, locked_path);
}

bool InProcessHandler::DumpExceptionFromSimulatedMachExceptionAtPath(
    const NativeCPUContext* context,
    exception_type_t exception,
    const base::FilePath& path) {
  // This does not use the cached writer. It's expected that simulated
  // exceptions can be called multiple times and there is no expectation that
  // the application is in an unsafe state, or will be terminated after this
  // call.
  std::unique_ptr<IOSIntermediateDumpWriter> unsafe_writer =
      CreateWriterWithPath(path);
  base::FilePath writer_path_unlocked = path.RemoveFinalExtension();
  ScopedLockedWriter writer(unsafe_writer.get(),
                            path.value().c_str(),
                            writer_path_unlocked.value().c_str());
  if (!writer.GetWriter()) {
    CRASHPAD_RAW_LOG(
        "Cannot DumpExceptionFromSimulatedMachExceptionAtPath without writer");
    return false;
  }
  ScopedReport report(writer.GetWriter(), system_data_, annotations_);
  InProcessIntermediateDumpHandler::WriteExceptionFromMachException(
      writer.GetWriter(),
      MACH_EXCEPTION_CODES,
      mach_thread_self(),
      exception,
      kEmulatedMachExceptionCodes,
      std::size(kEmulatedMachExceptionCodes),
      MACHINE_THREAD_STATE,
      reinterpret_cast<ConstThreadState>(context),
      MACHINE_THREAD_STATE_COUNT);
  return true;
}

bool InProcessHandler::MoveIntermediateDumpAtPathToPending(
    const base::FilePath& path) {
  base::FilePath new_path_unlocked = NewLockedFilePath().RemoveFinalExtension();
  return MoveFileOrDirectory(path, new_path_unlocked);
}
void InProcessHandler::ProcessIntermediateDumps(
    const std::map<std::string, std::string>& annotations) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  for (auto& file : PendingFiles())
    ProcessIntermediateDump(file, annotations);
}

void InProcessHandler::ProcessIntermediateDump(
    const base::FilePath& file,
    const std::map<std::string, std::string>& annotations) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  ProcessSnapshotIOSIntermediateDump process_snapshot;
  if (process_snapshot.InitializeWithFilePath(file, annotations)) {
    SaveSnapshot(process_snapshot);
  }
}

void InProcessHandler::StartProcessingPendingReports(
    UploadBehavior upload_behavior) {
  if (!upload_thread_)
    return;

  upload_thread_enabled_ = true;

  // This may be a no-op if IsApplicationActive is false, as it is not safe to
  // start the upload thread when in the background (due to the potential for
  // flocked files in shared containers).
  // TODO(crbug.com/crashpad/400): Consider moving prune and upload thread to
  // BackgroundTasks and/or NSURLSession. This might allow uploads to continue
  // in the background.
  UpdatePruneAndUploadThreads(system_data_.IsApplicationActive(),
                              upload_behavior);
}

void InProcessHandler::UpdatePruneAndUploadThreads(
    bool active,
    UploadBehavior upload_behavior) {
  base::AutoLock lock_owner(prune_and_upload_lock_);
  // TODO(crbug.com/crashpad/400): Consider moving prune and upload thread to
  // BackgroundTasks and/or NSURLSession. This might allow uploads to continue
  // in the background.
  bool threads_should_run;
  switch (upload_behavior) {
    case UploadBehavior::kUploadWhenAppIsActive:
      threads_should_run = active;
      break;
    case UploadBehavior::kUploadImmediately:
      threads_should_run = true;
      break;
  }
  if (threads_should_run) {
    if (!prune_thread_->is_running())
      prune_thread_->Start();
    if (upload_thread_enabled_ && !upload_thread_->is_running()) {
      upload_thread_->Start();
    }
  } else {
    if (prune_thread_->is_running())
      prune_thread_->Stop();
    if (upload_thread_enabled_ && upload_thread_->is_running())
      upload_thread_->Stop();
  }
}

void InProcessHandler::SaveSnapshot(
    ProcessSnapshotIOSIntermediateDump& process_snapshot) {
  std::unique_ptr<CrashReportDatabase::NewReport> new_report;
  CrashReportDatabase::OperationStatus database_status =
      database_->PrepareNewCrashReport(&new_report);
  if (database_status != CrashReportDatabase::kNoError) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kPrepareNewCrashReportFailed);
    return;
  }
  process_snapshot.SetReportID(new_report->ReportID());

  UUID client_id;
  Settings* const settings = database_->GetSettings();
  if (settings && settings->GetClientID(&client_id)) {
    process_snapshot.SetClientID(client_id);
  }

  MinidumpFileWriter minidump;
  minidump.InitializeFromSnapshot(&process_snapshot);
  if (!minidump.WriteEverything(new_report->Writer())) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kMinidumpWriteFailed);
    return;
  }
  UUID uuid;
  database_status =
      database_->FinishedWritingCrashReport(std::move(new_report), &uuid);
  if (database_status != CrashReportDatabase::kNoError) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
    return;
  }

  if (upload_thread_) {
    upload_thread_->ReportPending(uuid);
  }
}

std::vector<base::FilePath> InProcessHandler::PendingFiles() {
  DirectoryReader reader;
  std::vector<base::FilePath> files;
  if (!reader.Open(base_dir_)) {
    return files;
  }
  base::FilePath file;
  DirectoryReader::Result result;

  // Because the intermediate dump directory is expected to be shared,
  // mitigate any spamming by limiting this to |kMaxPendingFiles|.
  constexpr size_t kMaxPendingFiles = 20;

  // Track other application bundles separately, so they don't spam our
  // intermediate dumps into never getting processed.
  std::vector<base::FilePath> other_files;

  base::FilePath cached_writer_path(cached_writer_path_);
  while ((result = reader.NextFile(&file)) ==
         DirectoryReader::Result::kSuccess) {
    // Don't try to process files marked as 'locked' from a different bundle id.
    bool bundle_match =
        file.value().compare(0,
                             bundle_identifier_and_seperator_.size(),
                             bundle_identifier_and_seperator_) == 0;
    if (!bundle_match && file.FinalExtension() == kLockedExtension) {
      continue;
    }

    // Never process the current cached writer path.
    file = base_dir_.Append(file);
    if (file == cached_writer_path)
      continue;

    // Otherwise, include any other unlocked, or locked files matching
    // |bundle_identifier_and_seperator_|.
    if (bundle_match) {
      files.push_back(file);
      if (files.size() >= kMaxPendingFiles)
        return files;
    } else {
      other_files.push_back(file);
    }
  }

  auto end_iterator =
      other_files.begin() +
      std::min(kMaxPendingFiles - files.size(), other_files.size());
  files.insert(files.end(), other_files.begin(), end_iterator);
  return files;
}

IOSIntermediateDumpWriter* InProcessHandler::GetCachedWriter() {
  static_assert(
      std::atomic<uint64_t>::is_always_lock_free,
      "std::atomic_compare_exchange_strong uint64_t may not be signal-safe");
  uint64_t thread_self;
  // This is only safe when passing pthread_self(), otherwise this can lock.
  pthread_threadid_np(pthread_self(), &thread_self);
  uint64_t expected = 0;
  if (!std::atomic_compare_exchange_strong(
          &exception_thread_id_, &expected, thread_self)) {
    if (expected == thread_self) {
      // Another exception came in from this thread, which means it's likely
      // that our own handler crashed. We could open up a new intermediate dump
      // and try to save this dump, but we could end up endlessly writing dumps.
      // Instead, give up.
    } else {
      // Another thread is handling a crash. Sleep forever.
      while (1) {
        sleep(std::numeric_limits<unsigned int>::max());
      }
    }
    return nullptr;
  }

  return cached_writer_.get();
}

std::unique_ptr<IOSIntermediateDumpWriter>
InProcessHandler::CreateWriterWithPath(const base::FilePath& writer_path) {
  std::unique_ptr<IOSIntermediateDumpWriter> writer =
      std::make_unique<IOSIntermediateDumpWriter>();
  if (!writer->Open(writer_path)) {
    DLOG(ERROR) << "Unable to open intermediate dump file: "
                << writer_path.value();
    return nullptr;
  }
  return writer;
}

const base::FilePath InProcessHandler::NewLockedFilePath() {
  UUID uuid;
  uuid.InitializeWithNew();
  const std::string file_string =
      bundle_identifier_and_seperator_ + uuid.ToString() + kLockedExtension;
  return base_dir_.Append(file_string);
}

InProcessHandler::ScopedReport::ScopedReport(
    IOSIntermediateDumpWriter* writer,
    const IOSSystemDataCollector& system_data,
    const std::map<std::string, std::string>& annotations,
    const uint64_t* frames,
    const size_t num_frames)
    : writer_(writer),
      frames_(frames),
      num_frames_(num_frames),
      rootMap_(writer) {
  DCHECK(writer);
  // Grab the report creation time before writing the report.
  uint64_t report_time_nanos = ClockMonotonicNanoseconds();
  InProcessIntermediateDumpHandler::WriteHeader(writer);
  InProcessIntermediateDumpHandler::WriteProcessInfo(writer, annotations);
  InProcessIntermediateDumpHandler::WriteSystemInfo(
      writer, system_data, report_time_nanos);
}

InProcessHandler::ScopedReport::~ScopedReport() {
  // Write threads and modules last (after the exception itself is written by
  // DumpExceptionFrom*.)
  InProcessIntermediateDumpHandler::WriteThreadInfo(
      writer_, frames_, num_frames_);
  InProcessIntermediateDumpHandler::WriteModuleInfo(writer_);
}

InProcessHandler::ScopedLockedWriter::ScopedLockedWriter(
    IOSIntermediateDumpWriter* writer,
    const char* writer_path,
    const char* writer_unlocked_path)
    : writer_path_(writer_path),
      writer_unlocked_path_(writer_unlocked_path),
      writer_(writer) {}

InProcessHandler::ScopedLockedWriter::~ScopedLockedWriter() {
  if (!writer_)
    return;

  writer_->Close();
  if (rename(writer_path_, writer_unlocked_path_) != 0) {
    CRASHPAD_RAW_LOG("Could not remove locked extension.");
    CRASHPAD_RAW_LOG(writer_path_);
    CRASHPAD_RAW_LOG(writer_unlocked_path_);
  }
}

}  // namespace internal
}  // namespace crashpad
