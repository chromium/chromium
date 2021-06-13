// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "snapshot/ios/thread_snapshot_ios_intermediate_dump.h"

#include "base/mac/mach_logging.h"
#include "snapshot/ios/intermediate_dump_reader_util.h"
#include "snapshot/mac/cpu_context_mac.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_list.h"
#include "util/ios/ios_intermediate_dump_map.h"

#include <vector>

namespace {

std::vector<uint8_t> GenerateStackMemoryFromFrames(const uint64_t* frames,
                                                   const size_t frame_count) {
  std::vector<uint8_t> stack_memory;
  if (frame_count < 2) {
    return stack_memory;
  }
  size_t pointer_size = sizeof(uintptr_t);
  size_t frame_record_size = 2 * pointer_size;
  size_t stack_size = frame_record_size * (frame_count - 1) + pointer_size;
  stack_memory.resize(stack_size);
  uintptr_t sp = stack_size - pointer_size;
  uintptr_t fp = 0;
  uintptr_t lr = 0;
  for (size_t current_frame = frame_count - 1; current_frame > 0;
       --current_frame) {
    memcpy(&stack_memory[0] + sp, &lr, sizeof(lr));
    sp -= pointer_size;
    memcpy(&stack_memory[0] + sp, &fp, sizeof(fp));
    fp = sp;
    sp -= pointer_size;
    lr = frames[current_frame];
  }

  if (sp != 0) {
    LOG(ERROR) << "kExpectedFinalSp should be 0, is " << sp;
  }
  if (fp != sizeof(uintptr_t)) {
    LOG(ERROR) << "kExpectedFinalFp should be sizeof(uintptr_t), is " << fp;
  }
  if (lr != frames[1]) {
    LOG(ERROR) << "lr should be " << frames[1] << ", is " << lr;
  }
  return stack_memory;
}

}  // namespace
namespace crashpad {
namespace internal {

using Key = IntermediateDumpKey;

ThreadSnapshotIOSIntermediateDump::ThreadSnapshotIOSIntermediateDump()
    : ThreadSnapshot(),
      context_(),
      stack_(),
      thread_id_(0),
      thread_specific_data_address_(0),
      suspend_count_(0),
      priority_(0),
      initialized_() {
#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arm64_;
#endif
}

ThreadSnapshotIOSIntermediateDump::~ThreadSnapshotIOSIntermediateDump() {}

bool ThreadSnapshotIOSIntermediateDump::Initialize(
    const IOSIntermediateDumpMap* thread_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  GetDataValueFromMap(thread_data, Key::kSuspendCount, &suspend_count_);
  GetDataValueFromMap(thread_data, Key::kPriority, &priority_);
  GetDataValueFromMap(thread_data, Key::kThreadID, &thread_id_);
  GetDataValueFromMap(
      thread_data, Key::kThreadDataAddress, &thread_specific_data_address_);

#if defined(ARCH_CPU_X86_64)
  typedef x86_thread_state64_t thread_state_type;
  typedef x86_float_state64_t float_state_type;
  typedef x86_debug_state64_t debug_state_type;
#elif defined(ARCH_CPU_ARM64)
  typedef arm_thread_state64_t thread_state_type;
  typedef arm_neon_state64_t float_state_type;
  typedef arm_debug_state64_t debug_state_type;
#endif

  thread_state_type thread_state;
  float_state_type float_state;
  debug_state_type debug_state;

  const IOSIntermediateDumpData* nsexception_frames =
      thread_data->GetAsData(Key::kThreadUncaughtNSExceptionFrames);
  const IOSIntermediateDumpData* thread_stack_data_dump =
      thread_data->GetAsData(Key::kStackRegionData);
  if (nsexception_frames && thread_stack_data_dump) {
    LOG(ERROR) << "Unexpected thread with kStackRegionData and "
               << "kThreadUncaughtNSExceptionFrames, using kStackRegionData";
  }
  if (thread_stack_data_dump) {
    vm_address_t stack_region_address;
    GetDataValueFromMap(
        thread_data, Key::kStackRegionAddress, &stack_region_address);

    const std::vector<uint8_t>& bytes = thread_stack_data_dump->bytes();
    const vm_address_t stack_region_data =
        reinterpret_cast<const vm_address_t>(bytes.data());
    vm_size_t stack_region_size = bytes.size();
    stack_.Initialize(
        stack_region_address, stack_region_data, stack_region_size);
  } else if (nsexception_frames) {
    const std::vector<uint8_t>& bytes = nsexception_frames->bytes();
    const uint64_t* frames = reinterpret_cast<const uint64_t*>(bytes.data());
    size_t frame_count = bytes.size() / sizeof(uint64_t);
    exception_stack_memory_ =
        GenerateStackMemoryFromFrames(frames, frame_count);
    stack_.Initialize(
        0,
        reinterpret_cast<vm_address_t>(&exception_stack_memory_[0]),
        exception_stack_memory_.size());
  } else {
    stack_.Initialize(0, 0, 0);
  }

  if (GetDataValueFromMap(thread_data, Key::kThreadState, &thread_state) &&
      GetDataValueFromMap(thread_data, Key::kFloatState, &float_state) &&
      GetDataValueFromMap(thread_data, Key::kDebugState, &debug_state)) {
#if defined(ARCH_CPU_X86_64)
    InitializeCPUContextX86_64(&context_x86_64_,
                               THREAD_STATE_NONE,
                               nullptr,
                               0,
                               &thread_state,
                               &float_state,
                               &debug_state);
#elif defined(ARCH_CPU_ARM64)
    InitializeCPUContextARM64(&context_arm64_,
                              THREAD_STATE_NONE,
                              nullptr,
                              0,
                              &thread_state,
                              &float_state,
                              &debug_state);

#else
#error Port to your CPU architecture
#endif
  }
  const IOSIntermediateDumpList* thread_context_memory_regions =
      GetListFromMap(thread_data, Key::kThreadContextMemoryRegions);
  if (thread_context_memory_regions) {
    for (auto& region : *thread_context_memory_regions) {
      vm_address_t address;
      const IOSIntermediateDumpData* region_data =
          region->GetAsData(Key::kThreadContextMemoryRegionData);
      if (!region_data)
        continue;
      if (GetDataValueFromMap(
              region.get(), Key::kThreadContextMemoryRegionAddress, &address)) {
        const std::vector<uint8_t>& bytes = region_data->bytes();
        vm_size_t data_size = bytes.size();
        if (data_size == 0)
          continue;

        const vm_address_t data =
            reinterpret_cast<const vm_address_t>(bytes.data());

        auto memory =
            std::make_unique<internal::MemorySnapshotIOSIntermediateDump>();
        memory->Initialize(address, data, data_size);
        extra_memory_.push_back(std::move(memory));
      }
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}
const CPUContext* ThreadSnapshotIOSIntermediateDump::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

const MemorySnapshot* ThreadSnapshotIOSIntermediateDump::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

uint64_t ThreadSnapshotIOSIntermediateDump::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

int ThreadSnapshotIOSIntermediateDump::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return suspend_count_;
}

int ThreadSnapshotIOSIntermediateDump::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return priority_;
}

uint64_t ThreadSnapshotIOSIntermediateDump::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_specific_data_address_;
}

std::vector<const MemorySnapshot*>
ThreadSnapshotIOSIntermediateDump::ExtraMemory() const {
  std::vector<const MemorySnapshot*> extra_memory;
  for (const auto& memory : extra_memory_) {
    extra_memory.push_back(memory.get());
  }
  return extra_memory;
}

}  // namespace internal
}  // namespace crashpad
