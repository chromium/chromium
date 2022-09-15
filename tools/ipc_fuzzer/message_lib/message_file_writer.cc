// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <set>

#include "base/files/file.h"
#include "base/logging.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"
#include "tools/ipc_fuzzer/message_lib/message_file_format.h"
#include "tools/ipc_fuzzer/message_lib/message_names.h"

namespace ipc_fuzzer {

namespace {

// Helper class to write a MessageVector + message names to a file.
class Writer {
 public:
  Writer(const base::FilePath& path);

  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  ~Writer() {}
  bool Write(const MessageVector& messages);

 private:
  bool OpenFile();

  // Helper to append data to file_.
  bool WriteBlob(const void *buffer, size_t size);

  // Collects a set of MessageVector message types. Corresponding message
  // names need to be included in the file.
  bool CollectMessageTypes();

  bool WriteHeader();
  bool WriteMessages();

  // Each name table entry is a message type + string table offset.
  bool WriteNameTable();

  // String table contains the actual message names.
  bool WriteStringTable();

  typedef std::set<uint32_t> TypesSet;
  base::FilePath path_;
  base::File file_;
  const MessageVector* messages_;
  TypesSet types_;
};

Writer::Writer(const base::FilePath& path) : path_(path), messages_(NULL) {
}

bool Writer::OpenFile() {
  file_.Initialize(path_,
                   base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file_.IsValid()) {
    LOG(ERROR) << "Failed to create IPC message file: " << path_.value();
    return false;
  }
  return true;
}

bool Writer::WriteBlob(const void *buffer, size_t size) {
  if (size > INT_MAX)
    return false;
  const char* char_buffer = static_cast<const char*>(buffer);
  int ret = file_.WriteAtCurrentPos(char_buffer, size);
  if (ret != static_cast<int>(size)) {
    LOG(ERROR) << "Failed to write " << size << " bytes.";
    return false;
  }
  return true;
}

bool Writer::CollectMessageTypes() {
  for (size_t i = 0; i < messages_->size(); ++i) {
    uint32_t type = (*messages_)[i]->type();
    if (!MessageNames::GetInstance()->TypeExists(type)) {
      LOG(ERROR) << "Unknown message type: " << type;
      return false;
    }
    types_.insert(type);
  }
  return true;
}

bool Writer::WriteHeader() {
  FileHeader header;
  if (messages_->size() > UINT_MAX)
    return false;
  header.magic = FileHeader::kMagicValue;
  header.version = FileHeader::kCurrentVersion;
  header.message_count = messages_->size();
  header.name_count = types_.size();
  if (!WriteBlob(&header, sizeof(FileHeader)))
    return false;
  return true;
}

bool Writer::WriteMessages() {
  for (size_t i = 0; i < messages_->size(); ++i) {
    IPC::Message* message = (*messages_)[i].get();
    if (!WriteBlob(message->data(), message->size()))
      return false;
  }
  return true;
}

bool Writer::WriteNameTable() {
  size_t string_table_offset = 0;
  NameTableEntry entry;

  for (TypesSet::iterator it = types_.begin(); it != types_.end(); ++it) {
    if (string_table_offset > UINT_MAX)
      return false;
    entry.type = *it;
    entry.string_table_offset = string_table_offset;
    if (!WriteBlob(&entry, sizeof(NameTableEntry)))
      return false;
    const std::string& name = MessageNames::GetInstance()->TypeToName(*it);
    string_table_offset += name.length() + 1;
  }
  return true;
}

bool Writer::WriteStringTable() {
  for (TypesSet::iterator it = types_.begin(); it != types_.end(); ++it) {
    const std::string& name = MessageNames::GetInstance()->TypeToName(*it);
    if (!WriteBlob(name.c_str(), name.length() + 1))
      return false;
  }
  return true;
}

bool Writer::Write(const MessageVector& messages) {
  messages_ = &messages;

  if (!OpenFile())
    return false;
  if (!CollectMessageTypes())
    return false;
  if (!WriteHeader())
    return false;
  if (!WriteMessages())
    return false;
  if (!WriteNameTable())
    return false;
  if (!WriteStringTable())
    return false;

  return true;
}

}  // namespace

bool MessageFile::Write(const base::FilePath& path,
                        const MessageVector& messages) {
  Writer writer(path);
  return writer.Write(messages);
}

}  // namespace ipc_fuzzer
