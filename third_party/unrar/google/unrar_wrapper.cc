// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/unrar/google/unrar_wrapper.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/unrar/src/rar.hpp"

namespace third_party_unrar {

FileReader::FileReader(base::File file) : file_(std::move(file)) {}
FileReader::~FileReader() = default;

int64_t FileReader::Read(base::span<uint8_t> data) {
  std::optional<size_t> bytes_read = file_.ReadAtCurrentPos(data);
  return bytes_read.has_value() ? base::checked_cast<int64_t>(*bytes_read) : -1;
}

bool FileReader::Seek(int64_t offset) {
  return file_.Seek(base::File::FROM_BEGIN, offset) != -1;
}

int64_t FileReader::Tell() {
  return file_.Seek(base::File::FROM_CURRENT, 0);
}

int64_t FileReader::GetLength() {
  return file_.GetLength();
}

FileWriter::FileWriter(base::File file) : file_(std::move(file)) {}
FileWriter::~FileWriter() = default;

bool FileWriter::Write(base::span<const uint8_t> data) {
  if (!file_.IsValid())
    return false;
  return file_.WriteAtCurrentPos(data).has_value();
}

void FileWriter::Close() { }

RarReader::RarReader() = default;

RarReader::~RarReader() {
  ErrHandler.Clean();
}

bool RarReader::Open(std::unique_ptr<RarReaderDelegate> delegate,
                     base::File temp_file) {
  reader_ = std::move(delegate);
  temp_file_ = std::move(temp_file);

  command_ = std::make_unique<CommandData>();
  // Unrar forbids empty passwords, but requires that a password be provided for
  // encrypted archives. In order to support metadata encryption, we must
  // provide some password when opening the file.
  std::wstring password_flag =
      L"-p" + (password_.empty() ? L"x" : base::UTF8ToWide(password_));
  command_->ParseArg(password_flag.data());
  command_->ParseArg(const_cast<wchar_t*>(L"t"));

  if (!writer_) {
    // If no custom writer is set, use the default FileWriter writing to temp_file.
    writer_ = std::make_unique<FileWriter>(temp_file_.Duplicate());
  }

  command_->ParseDone();

  archive_ = std::make_unique<Archive>(command_.get());
  archive_->SetReaderDelegate(reader_.get());
  archive_->SetWriterDelegate(writer_.get());

  bool open_success = archive_->Open(L"dummy.rar");
  if (!open_success)
    return false;

  bool is_valid_archive = archive_->IsArchive(/*EnableBroken=*/true);
  if (!is_valid_archive)
    return false;

  extractor_ = std::make_unique<CmdExtract>(command_.get());
  extractor_->ExtractArchiveInit(*archive_);

  return true;
}

bool RarReader::ExtractNextEntry() {
  write_error_ = false;
  bool success = true, repeat = true;
  while (success || repeat) {
    temp_file_.Seek(base::File::Whence::FROM_BEGIN, 0);
    temp_file_.SetLength(0);
    size_t header_size = archive_->ReadHeader();
    repeat = false;
    success = extractor_->ExtractCurrentFile(
        *archive_, header_size, repeat);  // |repeat| is passed by reference

    if (archive_->GetHeaderType() == HEAD_FILE) {
#if defined(OS_WIN)
      current_entry_.file_path = base::FilePath(archive_->FileHead.FileName);
#else
      std::wstring wide_filename(archive_->FileHead.FileName);
      std::string filename(wide_filename.begin(), wide_filename.end());
      current_entry_.file_path = base::FilePath(filename);
#endif
      current_entry_.is_directory = archive_->FileHead.Dir;
      current_entry_.is_encrypted = archive_->FileHead.Encrypted;
      current_entry_.file_size =
          current_entry_.is_directory ? 0 : extractor_->GetCurrentFileSize();
      current_entry_.contents_valid =
          success && ErrHandler.GetErrorCode() == RARX_SUCCESS;
      ErrHandler.Clean();
      if (writer_) {
        writer_->Close();
      }
      if (success) {
        return true;
      }

      if (archive_->FileHead.Encrypted) {
        // Since Chromium doesn't have the password or the password was
        // incorrect, manually skip over the encrypted data and fill in the
        // metadata we do have.
        archive_->SeekToNext();
        return true;
      }

      if (extractor_->IsMissingNextVolume()) {
        // Since Chromium doesn't have the next volume, manually skip over this
        // file, but report the metadata we do have.
        archive_->SeekToNext();
        return true;
      }
    }
  }

  return false;
}

void RarReader::SetPassword(const std::string& password) {
  password_ = password;
}

void RarReader::SetWriterDelegate(std::unique_ptr<RarWriterDelegate> writer) {
  writer_ = std::move(writer);
}

bool RarReader::HeadersEncrypted() const {
  return archive_->Encrypted;
}

bool RarReader::HeaderDecryptionFailed() const {
  return archive_->FailedHeaderDecryption;
}

bool RarReader::HasWriteError() const {
   return write_error_;
}

}  // namespace third_party_unrar
