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

#include "client/ios_handler/in_process_handler.h"

#include <stdio.h>
#include <sys/stat.h>

#include "base/logging.h"
#include "client/ios_handler/in_process_intermediate_dump_handler.h"
#include "client/prune_crash_reports.h"
#include "client/settings.h"
#include "minidump/minidump_file_writer.h"
#include "util/file/directory_reader.h"
#include "util/file/filesystem.h"

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

}  // namespace

namespace crashpad {
namespace internal {

InProcessHandler::InProcessHandler() = default;

InProcessHandler::~InProcessHandler() {
  if (upload_thread_started_ && upload_thread_) {
    upload_thread_->Stop();
  }
  prune_thread_->Stop();
}

bool InProcessHandler::Initialize(
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const IOSSystemDataCollector& system_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  annotations_ = annotations;
  database_ = CrashReportDatabase::Initialize(database);
  if (!database_) {
    return false;
  }
  bundle_identifier_and_seperator_ =
      system_data.BundleIdentifier() + kBundleSeperator;

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
        database_.get(), url, upload_thread_options));
  }

  if (!CreateDirectory(database))
    return false;
  static constexpr char kPendingSerializediOSDump[] =
      "pending-serialized-ios-dump";
  base_dir_ = database.Append(kPendingSerializediOSDump);
  if (!CreateDirectory(base_dir_))
    return false;

  prune_thread_.reset(new PruneIntermediateDumpsAndCrashReportsThread(
      database_.get(),
      PruneCondition::GetDefault(),
      base_dir_,
      bundle_identifier_and_seperator_,
      system_data.IsExtension()));
  prune_thread_->Start();

  if (!OpenNewFile())
    return false;

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void InProcessHandler::DumpExceptionFromSignal(
    const IOSSystemDataCollector& system_data,
    siginfo_t* siginfo,
    ucontext_t* context) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  {
    ScopedReport report(writer_.get(), system_data, annotations_);
    InProcessIntermediateDumpHandler::WriteExceptionFromSignal(
        writer_.get(), system_data, siginfo, context);
  }
  PostReportCleanup();
}

void InProcessHandler::DumpExceptionFromMachException(
    const IOSSystemDataCollector& system_data,
    exception_behavior_t behavior,
    thread_t thread,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t flavor,
    ConstThreadState old_state,
    mach_msg_type_number_t old_state_count) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  {
    ScopedReport report(writer_.get(), system_data, annotations_);
    InProcessIntermediateDumpHandler::WriteExceptionFromMachException(
        writer_.get(),
        behavior,
        thread,
        exception,
        code,
        code_count,
        flavor,
        old_state,
        old_state_count);
  }
  PostReportCleanup();
}

void InProcessHandler::DumpExceptionFromNSExceptionFrames(
    const IOSSystemDataCollector& system_data,
    const uint64_t* frames,
    const size_t num_frames) {
  {
    ScopedReport report(
        writer_.get(), system_data, annotations_, frames, num_frames);
    InProcessIntermediateDumpHandler::WriteExceptionFromNSException(
        writer_.get());
  }
  PostReportCleanup();
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

void InProcessHandler::StartProcessingPendingReports() {
  if (!upload_thread_started_ && upload_thread_) {
    upload_thread_->Start();
    upload_thread_started_ = true;
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
  }
  UUID uuid;
  database_status =
      database_->FinishedWritingCrashReport(std::move(new_report), &uuid);
  if (database_status != CrashReportDatabase::kNoError) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
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

    // Never process the current file.
    file = base_dir_.Append(file);
    if (file == current_file_)
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

InProcessHandler::ScopedAlternateWriter::ScopedAlternateWriter(
    InProcessHandler* handler)
    : handler_(handler) {}

bool InProcessHandler::ScopedAlternateWriter::Open() {
  UUID uuid;
  uuid.InitializeWithNew();
  const std::string uuid_string = uuid.ToString();
  return OpenAtPath(handler_->base_dir_.Append(uuid_string));
}

bool InProcessHandler::ScopedAlternateWriter::OpenAtPath(
    const base::FilePath& path) {
  path_ = path;
  handler_->SetOpenNewFileAfterReport(false);
  original_writer_ = handler_->GetWriter();
  auto writer = std::make_unique<IOSIntermediateDumpWriter>();
  if (!writer->Open(path_)) {
    DLOG(ERROR) << "Unable to open alternate intermediate dump file: "
                << path_.value();
    return false;
  }
  handler_->SetWriter(std::move(writer));
  return true;
}

InProcessHandler::ScopedAlternateWriter::~ScopedAlternateWriter() {
  handler_->SetWriter(std::move(original_writer_));
  handler_->SetOpenNewFileAfterReport(true);
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
  InProcessIntermediateDumpHandler::WriteHeader(writer);
  InProcessIntermediateDumpHandler::WriteProcessInfo(writer, annotations);
  InProcessIntermediateDumpHandler::WriteSystemInfo(writer, system_data);
}

InProcessHandler::ScopedReport::~ScopedReport() {
  // Write threads and modules last (after the exception itself is written by
  // DumpExceptionFrom*.)
  InProcessIntermediateDumpHandler::WriteThreadInfo(
      writer_, frames_, num_frames_);
  InProcessIntermediateDumpHandler::WriteModuleInfo(writer_);
}

bool InProcessHandler::OpenNewFile() {
  if (!current_file_.empty()) {
    // Remove .lock extension so this dump can be processed on next run by this
    // client, or a client with a different bundle id that can access this dump.
    base::FilePath new_path = current_file_.RemoveFinalExtension();
    MoveFileOrDirectory(current_file_, new_path);
  }
  UUID uuid;
  uuid.InitializeWithNew();
  const std::string file_string =
      bundle_identifier_and_seperator_ + uuid.ToString() + kLockedExtension;
  current_file_ = base_dir_.Append(file_string);
  writer_ = std::make_unique<IOSIntermediateDumpWriter>();
  if (!writer_->Open(current_file_)) {
    DLOG(ERROR) << "Unable to open intermediate dump file: "
                << current_file_.value();
    return false;
  }
  return true;
}

void InProcessHandler::PostReportCleanup() {
  if (writer_) {
    writer_->Close();
    writer_.reset();
  }
  if (open_new_file_after_report_)
    OpenNewFile();
}

}  // namespace internal
}  // namespace crashpad
