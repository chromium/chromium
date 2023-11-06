// Copyright 2022 The Crashpad Authors
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

#include "minidump/minidump_thread_name_list_writer.h"

#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "minidump/minidump_thread_id_map.h"
#include "snapshot/thread_snapshot.h"
#include "util/file/file_writer.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

MinidumpThreadNameWriter::MinidumpThreadNameWriter()
    : MinidumpWritable(), rva_of_thread_name_(), thread_id_(), name_() {}

MinidumpThreadNameWriter::~MinidumpThreadNameWriter() {}

void MinidumpThreadNameWriter::InitializeFromSnapshot(
    const ThreadSnapshot* thread_snapshot,
    const MinidumpThreadIDMap& thread_id_map) {
  DCHECK_EQ(state(), kStateMutable);

  const auto it = thread_id_map.find(thread_snapshot->ThreadID());
  DCHECK(it != thread_id_map.end());
  SetThreadId(it->second);
  SetThreadName(thread_snapshot->ThreadName());
}

RVA64 MinidumpThreadNameWriter::RvaOfThreadName() const {
  DCHECK_EQ(state(), kStateWritable);

  return rva_of_thread_name_;
}

uint32_t MinidumpThreadNameWriter::ThreadId() const {
  DCHECK_EQ(state(), kStateWritable);

  return thread_id_;
}

bool MinidumpThreadNameWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  name_->RegisterRVA(&rva_of_thread_name_);

  return MinidumpWritable::Freeze();
}

void MinidumpThreadNameWriter::SetThreadName(const std::string& name) {
  DCHECK_EQ(state(), kStateMutable);

  if (!name_) {
    name_.reset(new internal::MinidumpUTF16StringWriter());
  }
  name_->SetUTF8(name);
}

size_t MinidumpThreadNameWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  // This object doesn’t directly write anything itself. Its parent writes the
  // MINIDUMP_THREAD_NAME objects as part of a MINIDUMP_THREAD_NAME_LIST, and
  // its children are responsible for writing themselves.
  return 0;
}

std::vector<internal::MinidumpWritable*> MinidumpThreadNameWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK(name_);

  std::vector<MinidumpWritable*> children;
  children.emplace_back(name_.get());

  return children;
}

bool MinidumpThreadNameWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  // This object doesn’t directly write anything itself. Its
  // MINIDUMP_THREAD_NAME is written by its parent as part of a
  // MINIDUMP_THREAD_NAME_LIST, and its children are responsible for writing
  // themselves.
  return true;
}

MinidumpThreadNameListWriter::MinidumpThreadNameListWriter()
    : MinidumpStreamWriter(), thread_names_() {}

MinidumpThreadNameListWriter::~MinidumpThreadNameListWriter() {}

void MinidumpThreadNameListWriter::InitializeFromSnapshot(
    const std::vector<const ThreadSnapshot*>& thread_snapshots,
    const MinidumpThreadIDMap& thread_id_map) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(thread_names_.empty());

  for (const ThreadSnapshot* thread_snapshot : thread_snapshots) {
    auto thread = std::make_unique<MinidumpThreadNameWriter>();
    thread->InitializeFromSnapshot(thread_snapshot, thread_id_map);
    AddThreadName(std::move(thread));
  }
}

void MinidumpThreadNameListWriter::AddThreadName(
    std::unique_ptr<MinidumpThreadNameWriter> thread_name) {
  DCHECK_EQ(state(), kStateMutable);

  thread_names_.emplace_back(std::move(thread_name));
}

bool MinidumpThreadNameListWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpStreamWriter::Freeze()) {
    return false;
  }

  size_t thread_name_count = thread_names_.size();
  if (!AssignIfInRange(&thread_name_list_.NumberOfThreadNames,
                       thread_name_count)) {
    LOG(ERROR) << "thread_name_count " << thread_name_count << " out of range";
    return false;
  }

  return true;
}

size_t MinidumpThreadNameListWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(thread_name_list_) +
         thread_names_.size() * sizeof(MINIDUMP_THREAD_NAME);
}

std::vector<internal::MinidumpWritable*>
MinidumpThreadNameListWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);

  std::vector<MinidumpWritable*> children;
  children.reserve(thread_names_.size());
  for (const auto& thread_name : thread_names_) {
    children.emplace_back(thread_name.get());
  }

  return children;
}

bool MinidumpThreadNameListWriter::WriteObject(
    FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  WritableIoVec iov;
  iov.iov_base = &thread_name_list_;
  iov.iov_len = sizeof(thread_name_list_);
  std::vector<WritableIoVec> iovecs(1, iov);
  iovecs.reserve(thread_names_.size() + 1);

  std::vector<MINIDUMP_THREAD_NAME> minidump_thread_names;
  minidump_thread_names.reserve(thread_names_.size());
  for (const auto& thread_name : thread_names_) {
    auto& minidump_thread_name = minidump_thread_names.emplace_back();
    minidump_thread_name.ThreadId = thread_name->ThreadId();
    minidump_thread_name.RvaOfThreadName = thread_name->RvaOfThreadName();
    iov.iov_base = &minidump_thread_name;
    iov.iov_len = sizeof(minidump_thread_name);
    iovecs.push_back(iov);
  }

  return file_writer->WriteIoVec(&iovecs);
}

MinidumpStreamType MinidumpThreadNameListWriter::StreamType() const {
  return kMinidumpStreamTypeThreadNameList;
}

}  // namespace crashpad
