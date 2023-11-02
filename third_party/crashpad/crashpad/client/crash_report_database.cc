// Copyright 2015 The Crashpad Authors
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

#include "client/crash_report_database.h"

#include <sys/stat.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "util/file/directory_reader.h"
#include "util/file/filesystem.h"

namespace crashpad {

namespace {
constexpr base::FilePath::CharType kAttachmentsDirectory[] =
    FILE_PATH_LITERAL("attachments");

bool AttachmentNameIsOK(const std::string& name) {
  for (const char c : name) {
    if (c != '_' && c != '-' && c != '.' && !isalnum(c))
      return false;
  }
  return true;
}
}  // namespace

CrashReportDatabase::Report::Report()
    : uuid(),
      file_path(),
      id(),
      creation_time(0),
      uploaded(false),
      last_upload_attempt_time(0),
      upload_attempts(0),
      upload_explicitly_requested(false),
      total_size(0u) {}

CrashReportDatabase::NewReport::NewReport()
    : writer_(std::make_unique<FileWriter>()),
      file_remover_(),
      attachment_writers_(),
      attachment_removers_(),
      uuid_(),
      database_() {}

CrashReportDatabase::NewReport::~NewReport() = default;

bool CrashReportDatabase::NewReport::Initialize(
    CrashReportDatabase* database,
    const base::FilePath& directory,
    const base::FilePath::StringType& extension) {
  database_ = database;

  if (!uuid_.InitializeWithNew()) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  const std::wstring uuid_string = uuid_.ToWString();
#else
  const std::string uuid_string = uuid_.ToString();
#endif

  const base::FilePath path = directory.Append(uuid_string + extension);
  if (!writer_->Open(
          path, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly)) {
    return false;
  }
  file_remover_.reset(path);
  return true;
}

FileReaderInterface* CrashReportDatabase::NewReport::Reader() {
  auto reader = std::make_unique<FileReader>();
  if (!reader->Open(file_remover_.get())) {
    return nullptr;
  }
  reader_ = std::move(reader);
  return reader_.get();
}

FileWriter* CrashReportDatabase::NewReport::AddAttachment(
    const std::string& name) {
  if (!AttachmentNameIsOK(name)) {
    LOG(ERROR) << "invalid name for attachment " << name;
    return nullptr;
  }
  base::FilePath report_attachments_dir = database_->AttachmentsPath(uuid_);
  if (!LoggingCreateDirectory(
          report_attachments_dir, FilePermissions::kOwnerOnly, true)) {
    return nullptr;
  }
#if BUILDFLAG(IS_WIN)
  const std::wstring name_string = base::UTF8ToWide(name);
#else
  const std::string name_string = name;
#endif
  base::FilePath attachment_path = report_attachments_dir.Append(name_string);
  auto writer = std::make_unique<FileWriter>();
  if (!writer->Open(attachment_path,
                    FileWriteMode::kCreateOrFail,
                    FilePermissions::kOwnerOnly)) {
    return nullptr;
  }
  attachment_writers_.emplace_back(std::move(writer));
  attachment_removers_.emplace_back(ScopedRemoveFile(attachment_path));
  return attachment_writers_.back().get();
}

void CrashReportDatabase::UploadReport::InitializeAttachments() {
  base::FilePath report_attachments_dir = database_->AttachmentsPath(uuid);
  DirectoryReader dir_reader;
  if (!dir_reader.Open(report_attachments_dir)) {
    return;
  }

  base::FilePath filename;
  DirectoryReader::Result dir_result;
  while ((dir_result = dir_reader.NextFile(&filename)) ==
         DirectoryReader::Result::kSuccess) {
    const base::FilePath filepath(report_attachments_dir.Append(filename));
    std::unique_ptr<FileReader> file_reader(std::make_unique<FileReader>());
    if (!file_reader->Open(filepath)) {
      continue;
    }
    attachment_readers_.emplace_back(std::move(file_reader));
#if BUILDFLAG(IS_WIN)
    const std::string name_string = base::WideToUTF8(filename.value());
#else
    const std::string name_string = filename.value();
#endif
    attachment_map_[name_string] = attachment_readers_.back().get();
  }
}

CrashReportDatabase::UploadReport::UploadReport()
    : Report(),
      reader_(std::make_unique<FileReader>()),
      database_(nullptr),
      attachment_readers_(),
      attachment_map_(),
      report_metrics_(false) {}

CrashReportDatabase::UploadReport::~UploadReport() {
  if (database_) {
    database_->RecordUploadAttempt(this, false, std::string());
  }
}

bool CrashReportDatabase::UploadReport::Initialize(const base::FilePath& path,
                                                   CrashReportDatabase* db) {
  database_ = db;
  InitializeAttachments();
  return reader_->Open(path);
}

CrashReportDatabase::OperationStatus CrashReportDatabase::RecordUploadComplete(
    std::unique_ptr<const UploadReport> report_in,
    const std::string& id) {
  UploadReport* report = const_cast<UploadReport*>(report_in.get());

  report->database_ = nullptr;
  return RecordUploadAttempt(report, true, id);
}

base::FilePath CrashReportDatabase::AttachmentsPath(const UUID& uuid) {
#if BUILDFLAG(IS_WIN)
  const std::wstring uuid_string = uuid.ToWString();
#else
  const std::string uuid_string = uuid.ToString();
#endif

  return DatabasePath().Append(kAttachmentsDirectory).Append(uuid_string);
}

base::FilePath CrashReportDatabase::AttachmentsRootPath() {
  return DatabasePath().Append(kAttachmentsDirectory);
}

void CrashReportDatabase::RemoveAttachmentsByUUID(const UUID& uuid) {
  base::FilePath report_attachment_dir = AttachmentsPath(uuid);
  if (!IsDirectory(report_attachment_dir, /*allow_symlinks=*/false)) {
    return;
  }
  DirectoryReader reader;
  if (!reader.Open(report_attachment_dir)) {
    return;
  }

  base::FilePath filename;
  DirectoryReader::Result result;
  while ((result = reader.NextFile(&filename)) ==
         DirectoryReader::Result::kSuccess) {
    const base::FilePath attachment_path(
        report_attachment_dir.Append(filename));
    LoggingRemoveFile(attachment_path);
  }

  LoggingRemoveDirectory(report_attachment_dir);
}

}  // namespace crashpad
