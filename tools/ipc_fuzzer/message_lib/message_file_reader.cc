// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ipc/ipc_message.h"
#include "tools/ipc_fuzzer/message_lib/message_cracker.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"
#include "tools/ipc_fuzzer/message_lib/message_file_format.h"
#include "tools/ipc_fuzzer/message_lib/message_names.h"

namespace ipc_fuzzer {

namespace {

// Helper class to read IPC message file into a MessageVector and
// fix message types.
class Reader {
 public:
  Reader(const base::FilePath& path);

  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  bool Read(MessageVector* messages);

 private:
  template <typename T>
  bool CutObject(const T** object);

  // Reads the header, checks magic and version.
  bool ReadHeader();

  bool MapFile();
  bool ReadMessages();

  // Last part of the file is a string table for message names.
  bool ReadStringTable();

  // Reads type <-> name mapping into name_map_. References string table.
  bool ReadNameTable();

  // Removes obsolete messages from the vector.
  bool RemoveUnknownMessages();

  // Does type -> name -> correct_type fixup.
  void FixMessageTypes();

  // Raw data.
  base::FilePath path_;
  base::MemoryMappedFile mapped_file_;
  std::string_view file_data_;
  std::string_view string_table_;

  // Parsed data.
  const FileHeader* header_;
  MessageVector* messages_;
  MessageNames name_map_;
};

Reader::Reader(const base::FilePath& path)
    : path_(path),
      header_(NULL),
      messages_(NULL) {
}

template <typename T>
bool Reader::CutObject(const T** object) {
  if (file_data_.size() < sizeof(T)) {
    LOG(ERROR) << "Unexpected EOF.";
    return false;
  }
  *object = reinterpret_cast<const T*>(file_data_.data());
  file_data_.remove_prefix(sizeof(T));
  return true;
}

bool Reader::ReadHeader() {
  if (!CutObject<FileHeader>(&header_))
    return false;
  if (header_->magic != FileHeader::kMagicValue) {
    LOG(ERROR) << path_.value() << " is not an IPC message file.";
    return false;
  }
  if (header_->version != FileHeader::kCurrentVersion) {
    LOG(ERROR) << "Wrong version for message file " << path_.value() << ". "
               << "File version is " << header_->version << ", "
               << "current version is " << FileHeader::kCurrentVersion << ".";
    return false;
  }
  return true;
}

bool Reader::MapFile() {
  if (!mapped_file_.Initialize(path_)) {
    LOG(ERROR) << "Failed to map testcase: " << path_.value();
    return false;
  }
  file_data_ = base::as_string_view(mapped_file_.bytes());
  return true;
}

bool Reader::ReadMessages() {
  for (size_t i = 0; i < header_->message_count; ++i) {
    const char* begin = file_data_.data();
    const char* end = begin + file_data_.size();
    IPC::Message::NextMessageInfo info;
    IPC::Message::FindNext(begin, end, &info);
    if (!info.message_found) {
      LOG(ERROR) << "Failed to parse message.";
      return false;
    }

    CHECK_EQ(info.message_end, info.pickle_end);
    size_t msglen = info.message_end - begin;
    if (msglen > INT_MAX) {
      LOG(ERROR) << "Message too large.";
      return false;
    }

    // Copy is necessary to fix message type later.
    IPC::Message const_message(begin, msglen);
    messages_->push_back(std::make_unique<IPC::Message>(const_message));
    file_data_.remove_prefix(msglen);
  }
  return true;
}

bool Reader::ReadStringTable() {
  size_t name_count = header_->name_count;
  if (!name_count)
    return true;
  if (name_count > file_data_.size() / sizeof(NameTableEntry)) {
    LOG(ERROR) << "Invalid name table size: " << name_count;
    return false;
  }

  size_t string_table_offset = name_count * sizeof(NameTableEntry);
  string_table_ = file_data_.substr(string_table_offset);
  if (string_table_.empty()) {
    LOG(ERROR) << "Missing string table.";
    return false;
  }
  if (string_table_.end()[-1] != '\0') {
    LOG(ERROR) << "String table doesn't end with NUL.";
    return false;
  }
  return true;
}

bool Reader::ReadNameTable() {
  for (size_t i = 0; i < header_->name_count; ++i) {
    const NameTableEntry* entry;
    if (!CutObject<NameTableEntry>(&entry))
      return false;
    size_t offset = entry->string_table_offset;
    if (offset >= string_table_.size()) {
      LOG(ERROR) << "Invalid string table offset: " << offset;
      return false;
    }
    name_map_.Add(entry->type, string_table_.data() + offset);
  }
  return true;
}

bool Reader::RemoveUnknownMessages() {
  MessageVector::iterator it = messages_->begin();
  while (it != messages_->end()) {
    uint32_t type = (*it)->type();
    if (!name_map_.TypeExists(type)) {
      LOG(ERROR) << "Missing name table entry for type " << type;
      return false;
    }
    const std::string& name = name_map_.TypeToName(type);
    if (!MessageNames::GetInstance()->NameExists(name)) {
      LOG(WARNING) << "Unknown message " << name;
      it = messages_->erase(it);
    } else {
      ++it;
    }
  }
  return true;
}

// Message types are based on line numbers, so a minor edit of *_messages.h
// changes the types of messages in that file. The types are fixed here to
// increase the lifetime of message files. This is only a partial fix because
// message arguments and structure layouts can change as well.
void Reader::FixMessageTypes() {
  for (const auto& message : *messages_) {
    uint32_t type = message->type();
    const std::string& name = name_map_.TypeToName(type);
    uint32_t correct_type = MessageNames::GetInstance()->NameToType(name);
    if (type != correct_type)
      MessageCracker::SetMessageType(message.get(), correct_type);
  }
}

bool Reader::Read(MessageVector* messages) {
  messages_ = messages;

  if (!MapFile())
    return false;
  if (!ReadHeader())
    return false;
  if (!ReadMessages())
    return false;
  if (!ReadStringTable())
    return false;
  if (!ReadNameTable())
    return false;
  if (!RemoveUnknownMessages())
    return false;
  FixMessageTypes();

  return true;
}

}  // namespace

bool MessageFile::Read(const base::FilePath& path, MessageVector* messages) {
  Reader reader(path);
  return reader.Read(messages);
}

}  // namespace ipc_fuzzer
