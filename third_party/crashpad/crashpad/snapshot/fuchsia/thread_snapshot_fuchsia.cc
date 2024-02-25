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

#include "snapshot/fuchsia/thread_snapshot_fuchsia.h"

#include "base/check_op.h"
#include "snapshot/fuchsia/cpu_context_fuchsia.h"

namespace crashpad {
namespace internal {

ThreadSnapshotFuchsia::ThreadSnapshotFuchsia()
    : ThreadSnapshot(),
      context_arch_(),
      context_(),
      stack_(),
      thread_name_(),
      thread_id_(ZX_KOID_INVALID),
      thread_specific_data_address_(0),
      initialized_() {}

ThreadSnapshotFuchsia::~ThreadSnapshotFuchsia() {}

bool ThreadSnapshotFuchsia::Initialize(
    ProcessReaderFuchsia* process_reader,
    const ProcessReaderFuchsia::Thread& thread) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_arch_;
  // TODO(fxbug.dev/42132536): Add vector context.
  InitializeCPUContextX86_64(
      thread.general_registers, thread.fp_registers, context_.x86_64);
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arch_;
  InitializeCPUContextARM64(
      thread.general_registers, thread.vector_registers, context_.arm64);
#elif defined(ARCH_CPU_RISCV64)
  context_.architecture = kCPUArchitectureRISCV64;
  context_.riscv64 = &context_arch_;
  InitializeCPUContextRISCV64(
      thread.general_registers, thread.fp_registers, context_.riscv64);
#else
#error Port.
#endif

  if (thread.stack_regions.empty()) {
    stack_.Initialize(process_reader->Memory(), 0, 0);
  } else {
    stack_.Initialize(process_reader->Memory(),
                      thread.stack_regions[0].base(),
                      thread.stack_regions[0].size());
    // TODO(scottmg): Handle split stack by adding other parts to ExtraMemory().
  }

  thread_name_ = thread.name;
  thread_id_ = thread.id;

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ThreadSnapshotFuchsia::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

const MemorySnapshot* ThreadSnapshotFuchsia::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

uint64_t ThreadSnapshotFuchsia::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

std::string ThreadSnapshotFuchsia::ThreadName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_name_;
}

int ThreadSnapshotFuchsia::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // There is not (currently) a suspend count for threads on Fuchsia.
  return 0;
}

int ThreadSnapshotFuchsia::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // There is not (currently) thread priorities on Fuchsia.
  return 0;
}

uint64_t ThreadSnapshotFuchsia::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_specific_data_address_;
}

std::vector<const MemorySnapshot*> ThreadSnapshotFuchsia::ExtraMemory() const {
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
