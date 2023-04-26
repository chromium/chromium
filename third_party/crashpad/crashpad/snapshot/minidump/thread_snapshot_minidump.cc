// Copyright 2018 The Crashpad Authors
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

#include "snapshot/minidump/thread_snapshot_minidump.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>

#include "minidump/minidump_context.h"

namespace crashpad {
namespace internal {

ThreadSnapshotMinidump::ThreadSnapshotMinidump()
    : ThreadSnapshot(),
      minidump_thread_(),
      thread_name_(),
      context_(),
      stack_(),
      initialized_() {}

ThreadSnapshotMinidump::~ThreadSnapshotMinidump() {}

bool ThreadSnapshotMinidump::Initialize(
    FileReaderInterface* file_reader,
    RVA minidump_thread_rva,
    CPUArchitecture arch,
    const std::map<uint32_t, std::string>& thread_names) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  std::vector<unsigned char> minidump_context;

  if (!file_reader->SeekSet(minidump_thread_rva)) {
    return false;
  }

  if (!file_reader->ReadExactly(&minidump_thread_, sizeof(minidump_thread_))) {
    return false;
  }

  if (!file_reader->SeekSet(minidump_thread_.ThreadContext.Rva)) {
    return false;
  }

  minidump_context.resize(minidump_thread_.ThreadContext.DataSize);

  if (!file_reader->ReadExactly(minidump_context.data(),
                                minidump_context.size())) {
    return false;
  }

  if (!context_.Initialize(arch, minidump_context)) {
    return false;
  }

  RVA stack_info_location =
      minidump_thread_rva + offsetof(MINIDUMP_THREAD, Stack);

  if (!stack_.Initialize(file_reader, stack_info_location)) {
    return false;
  }
  const auto thread_name_iter = thread_names.find(minidump_thread_.ThreadId);
  if (thread_name_iter != thread_names.end()) {
    thread_name_ = thread_name_iter->second;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

uint64_t ThreadSnapshotMinidump::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_thread_.ThreadId;
}

std::string ThreadSnapshotMinidump::ThreadName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_name_;
}

int ThreadSnapshotMinidump::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_thread_.SuspendCount;
}

uint64_t ThreadSnapshotMinidump::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_thread_.Teb;
}

int ThreadSnapshotMinidump::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_thread_.Priority;
}

const CPUContext* ThreadSnapshotMinidump::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return context_.Get();
}

const MemorySnapshot* ThreadSnapshotMinidump::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

std::vector<const MemorySnapshot*> ThreadSnapshotMinidump::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // This doesn't correspond to anything minidump can give us, with the
  // exception of the BackingStore field in the MINIDUMP_THREAD_EX structure,
  // which is only valid for IA-64.
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
