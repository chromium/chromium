// Copyright 2022 The Crashpad Authors. All rights reserved.
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

#include "base/logging.h"
#include "util/file/file_writer.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

MinidumpThreadNameWriter::MinidumpThreadNameWriter()
    : MinidumpWritable(), thread_name_(), name_() {}

MinidumpThreadNameWriter::~MinidumpThreadNameWriter() {}

const MINIDUMP_THREAD_NAME* MinidumpThreadNameWriter::MinidumpThreadName()
    const {
  DCHECK_EQ(state(), kStateWritable);

  return &thread_name_;
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

  // This object doesn’t directly write anything itself. Its
  // MINIDUMP_THREAD_NAME is written by its parent as part of a
  // MINIDUMP_THREAD_NAME_LIST, and its children are responsible for writing
  // themselves.
  return 0;
}

std::vector<internal::MinidumpWritable*> MinidumpThreadNameWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK(name_);

  std::vector<MinidumpWritable*> children;
  children.emplace_back(name_.get());

  return children;
}

bool MinidumpThreadNameWriter::WillWriteAtOffsetImpl(FileOffset offset) {
  DCHECK_EQ(state(), kStateFrozen);

  // This cannot use RegisterRVA(&thread_name_.RvaOfThreadName), since
  // &MINIDUMP_THREAD_NAME_LIST::RvaOfThreadName is not aligned on a pointer
  // boundary, so it causes failures on 32-bit ARM.
  //
  // Instead, manually update the RVA64 to the current file offset since the
  // child thread_name_ will write its contents at that offset.
  decltype(thread_name_.RvaOfThreadName) local_rva_of_thread_name;
  if (!AssignIfInRange(&local_rva_of_thread_name, offset)) {
    LOG(ERROR) << "offset " << offset << " out of range";
    return false;
  }
  thread_name_.RvaOfThreadName = local_rva_of_thread_name;
  return MinidumpWritable::WillWriteAtOffsetImpl(offset);
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

  for (const auto& thread_name : thread_names_) {
    iov.iov_base = thread_name->MinidumpThreadName();
    iov.iov_len = sizeof(MINIDUMP_THREAD_NAME);
    iovecs.emplace_back(iov);
  }

  return file_writer->WriteIoVec(&iovecs);
}

MinidumpStreamType MinidumpThreadNameListWriter::StreamType() const {
  return kMinidumpStreamTypeThreadNameList;
}

}  // namespace crashpad
