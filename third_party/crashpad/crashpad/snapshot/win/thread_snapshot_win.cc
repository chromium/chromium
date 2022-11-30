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

#include "snapshot/win/thread_snapshot_win.h"

#include <iterator>
#include <vector>

#include "base/check_op.h"
#include "base/memory/page_size.h"
#include "base/numerics/safe_conversions.h"
#include "snapshot/capture_memory.h"
#include "snapshot/win/capture_memory_delegate_win.h"
#include "snapshot/win/cpu_context_win.h"
#include "snapshot/win/process_reader_win.h"

namespace crashpad {
namespace internal {

namespace {
#if defined(ARCH_CPU_X86_64)

XSAVE_CET_U_FORMAT* LocateXStateCetU(CONTEXT* context) {
  // GetEnabledXStateFeatures needs Windows 7 SP1.
  static auto locate_xstate_feature = []() {
    HINSTANCE kernel32 = GetModuleHandle(L"Kernel32.dll");
    return reinterpret_cast<decltype(LocateXStateFeature)*>(
        GetProcAddress(kernel32, "LocateXStateFeature"));
  }();
  if (!locate_xstate_feature)
    return nullptr;

  DWORD cet_u_size = 0;
  return reinterpret_cast<XSAVE_CET_U_FORMAT*>(
      locate_xstate_feature(context, XSTATE_CET_U, &cet_u_size));
}
#endif  // defined(ARCH_CPU_X86_64)
}  // namespace

ThreadSnapshotWin::ThreadSnapshotWin()
    : ThreadSnapshot(),
      context_(),
      stack_(),
      teb_(),
      thread_(),
      initialized_() {
}

ThreadSnapshotWin::~ThreadSnapshotWin() {
}

bool ThreadSnapshotWin::Initialize(
    ProcessReaderWin* process_reader,
    const ProcessReaderWin::Thread& process_reader_thread,
    uint32_t* gather_indirectly_referenced_memory_bytes_remaining) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  thread_ = process_reader_thread;
  if (process_reader->GetProcessInfo().LoggingRangeIsFullyReadable(
          CheckedRange<WinVMAddress, WinVMSize>(thread_.stack_region_address,
                                                thread_.stack_region_size))) {
    stack_.Initialize(process_reader->Memory(),
                      thread_.stack_region_address,
                      thread_.stack_region_size);
  } else {
    stack_.Initialize(process_reader->Memory(), 0, 0);
  }

  if (process_reader->GetProcessInfo().LoggingRangeIsFullyReadable(
          CheckedRange<WinVMAddress, WinVMSize>(thread_.teb_address,
                                                thread_.teb_size))) {
    teb_.Initialize(
        process_reader->Memory(), thread_.teb_address, thread_.teb_size);
  } else {
    teb_.Initialize(process_reader->Memory(), 0, 0);
  }

#if defined(ARCH_CPU_X86)
  context_.architecture = kCPUArchitectureX86;
  context_.x86 = &context_union_.x86;
  InitializeX86Context(process_reader_thread.context.context<CONTEXT>(),
                       context_.x86);
#elif defined(ARCH_CPU_X86_64)
  if (process_reader->Is64Bit()) {
    context_.architecture = kCPUArchitectureX86_64;
    context_.x86_64 = &context_union_.x86_64;
    CONTEXT* context = process_reader_thread.context.context<CONTEXT>();
    InitializeX64Context(context, context_.x86_64);
    // Capturing process must have CET enabled. If captured process does not,
    // then this will not set any state in the context snapshot.
    if (IsXStateFeatureEnabled(XSTATE_MASK_CET_U)) {
      XSAVE_CET_U_FORMAT* cet_u = LocateXStateCetU(context);
      if (cet_u && cet_u->Ia32CetUMsr && cet_u->Ia32Pl3SspMsr) {
        InitializeX64XStateCet(context, cet_u, context_.x86_64);
      }
    }
  } else {
    context_.architecture = kCPUArchitectureX86;
    context_.x86 = &context_union_.x86;
    InitializeX86Context(process_reader_thread.context.context<WOW64_CONTEXT>(),
                         context_.x86);
  }
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_union_.arm64;
  InitializeARM64Context(process_reader_thread.context.context<CONTEXT>(),
                         context_.arm64);
#else
#error Unsupported Windows Arch
#endif  // ARCH_CPU_X86

#if defined(ARCH_CPU_X86_64)
  // Unconditionally store page around ssp if it is present.
  if (process_reader->Is64Bit() && context_.x86_64->xstate.cet_u.ssp) {
    WinVMAddress page_size =
        base::checked_cast<WinVMAddress>(base::GetPageSize());
    WinVMAddress page_mask = ~(page_size - 1);
    WinVMAddress ssp_base = context_.x86_64->xstate.cet_u.ssp & page_mask;
    if (process_reader->GetProcessInfo().LoggingRangeIsFullyReadable(
            CheckedRange<WinVMAddress, WinVMSize>(ssp_base, page_size))) {
      auto region = std::make_unique<MemorySnapshotGeneric>();
      region->Initialize(process_reader->Memory(), ssp_base, page_size);
      pointed_to_memory_.push_back(std::move(region));
    }
  }
#endif  // ARCH_CPU_X86_64

  CaptureMemoryDelegateWin capture_memory_delegate(
      process_reader,
      thread_,
      &pointed_to_memory_,
      gather_indirectly_referenced_memory_bytes_remaining);
  CaptureMemory::PointedToByContext(context_, &capture_memory_delegate);
  if (gather_indirectly_referenced_memory_bytes_remaining) {
    CaptureMemory::PointedToByMemoryRange(stack_, &capture_memory_delegate);
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ThreadSnapshotWin::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

const MemorySnapshot* ThreadSnapshotWin::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

uint64_t ThreadSnapshotWin::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.id;
}

std::string ThreadSnapshotWin::ThreadName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.name;
}

int ThreadSnapshotWin::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.suspend_count;
}

int ThreadSnapshotWin::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.priority;
}

uint64_t ThreadSnapshotWin::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.teb_address;
}

std::vector<const MemorySnapshot*> ThreadSnapshotWin::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const MemorySnapshot*> result;
  result.reserve(1 + pointed_to_memory_.size());
  result.push_back(&teb_);
  for (const auto& pointed_to_memory : pointed_to_memory_) {
    result.push_back(pointed_to_memory.get());
  }
  return result;
}

}  // namespace internal
}  // namespace crashpad
