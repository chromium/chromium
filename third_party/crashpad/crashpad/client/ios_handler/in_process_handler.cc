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
#include "client/settings.h"
#include "minidump/minidump_file_writer.h"
#include "util/file/directory_reader.h"
#include "util/file/filesystem.h"

namespace {

// Creates directory at |path|.
void CreateDirectory(const base::FilePath& path) {
  if (mkdir(path.value().c_str(), 0755) == 0) {
    return;
  }
  if (errno != EEXIST) {
    PLOG(ERROR) << "mkdir " << path.value();
  }
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
  upload_thread_->Stop();
}

bool InProcessHandler::Initialize(
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const IOSSystemDataCollector& system_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  annotations_ = annotations;
  database_ = CrashReportDatabase::Initialize(database);
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

  CreateDirectory(database);
  static constexpr char kPendingSerializediOSDump[] =
      "pending-serialized-ios-dump";
  base_dir_ = database.Append(kPendingSerializediOSDump);
  CreateDirectory(base_dir_);

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
    ScopedReport report(writer_.get(), system_data);
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
    ScopedReport report(writer_.get(), system_data);
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
    ScopedReport report(writer_.get(), system_data, frames, num_frames);
    InProcessIntermediateDumpHandler::WriteExceptionFromNSException(
        writer_.get());
  }
  PostReportCleanup();
}

void InProcessHandler::ProcessIntermediateDumps(
    const std::map<std::string, std::string>& extra_annotations) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::map<std::string, std::string> annotations(annotations_);
  annotations.insert(extra_annotations.begin(), extra_annotations.end());

  for (auto& file : PendingFiles())
    ProcessIntermediateDumpWithCompleteAnnotations(file, annotations);
}

void InProcessHandler::ProcessIntermediateDump(
    const base::FilePath& file,
    const std::map<std::string, std::string>& extra_annotations) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::map<std::string, std::string> annotations(annotations_);
  annotations.insert(extra_annotations.begin(), extra_annotations.end());
  ProcessIntermediateDumpWithCompleteAnnotations(file, annotations);
}

void InProcessHandler::StartProcessingPendingReports() {
  if (!upload_thread_started_ && upload_thread_) {
    upload_thread_->Start();
    upload_thread_started_ = true;
  }
}

void InProcessHandler::ProcessIntermediateDumpWithCompleteAnnotations(
    const base::FilePath& file,
    const std::map<std::string, std::string>& annotations) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  ProcessSnapshotIOSIntermediateDump process_snapshot;
  if (process_snapshot.Initialize(file, annotations)) {
    SaveSnapshot(process_snapshot);
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
  while ((result = reader.NextFile(&file)) ==
         DirectoryReader::Result::kSuccess) {
    // Don't try to process files marked as 'locked' from a different bundle id.
    if (file.value().compare(0,
                             bundle_identifier_and_seperator_.size(),
                             bundle_identifier_and_seperator_) != 0 &&
        file.FinalExtension() == kLockedExtension) {
      continue;
    }

    // Never process the current file.
    file = base_dir_.Append(file);
    if (file == current_file_)
      continue;

    // Otherwise, include any other unlocked, or locked files matching
    // |bundle_identifier_and_seperator_|.
    files.push_back(file);
  }
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
    const uint64_t* frames,
    const size_t num_frames)
    : rootMap_(writer) {
  InProcessIntermediateDumpHandler::WriteHeader(writer);
  InProcessIntermediateDumpHandler::WriteProcessInfo(writer);
  InProcessIntermediateDumpHandler::WriteSystemInfo(writer, system_data);
  InProcessIntermediateDumpHandler::WriteThreadInfo(writer, frames, num_frames);
  InProcessIntermediateDumpHandler::WriteModuleInfo(writer);
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
