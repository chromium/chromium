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

#include "snapshot/ios/exception_snapshot_ios_intermediate_dump.h"

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "snapshot/cpu_context.h"
#include "snapshot/ios/intermediate_dump_reader_util.h"
#include "snapshot/mac/cpu_context_mac.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_list.h"
#include "util/ios/ios_intermediate_dump_writer.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {

namespace internal {

size_t ThreadStateLengthForFlavor(thread_state_flavor_t flavor) {
#if defined(ARCH_CPU_X86_64)
  switch (flavor) {
    case x86_THREAD_STATE:
      return sizeof(x86_thread_state_t);
    case x86_FLOAT_STATE:
      return sizeof(x86_float_state_t);
    case x86_DEBUG_STATE:
      return sizeof(x86_debug_state_t);
    case x86_THREAD_STATE64:
      return sizeof(x86_thread_state64_t);
    case x86_FLOAT_STATE64:
      return sizeof(x86_float_state64_t);
    case x86_DEBUG_STATE64:
      return sizeof(x86_debug_state64_t);
    default:
      return 0;
  }
#elif defined(ARCH_CPU_ARM64)
  switch (flavor) {
    case ARM_UNIFIED_THREAD_STATE:
      return sizeof(arm_unified_thread_state_t);
    case ARM_THREAD_STATE64:
      return sizeof(arm_thread_state64_t);
    case ARM_NEON_STATE64:
      return sizeof(arm_neon_state64_t);
    case ARM_DEBUG_STATE64:
      return sizeof(arm_debug_state64_t);
    default:
      return 0;
  }
#endif
}

using Key = IntermediateDumpKey;

ExceptionSnapshotIOSIntermediateDump::ExceptionSnapshotIOSIntermediateDump()
    : ExceptionSnapshot(),
      context_(),
      codes_(),
      thread_id_(0),
      exception_address_(0),
      exception_(0),
      exception_info_(0),
      initialized_() {
#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arm64_;
#else
#error Port to your CPU architecture
#endif
}

ExceptionSnapshotIOSIntermediateDump::~ExceptionSnapshotIOSIntermediateDump() {}

bool ExceptionSnapshotIOSIntermediateDump::InitializeFromSignal(
    const IOSIntermediateDumpMap* exception_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  DCHECK(exception_data);

  if (!GetDataValueFromMap(exception_data, Key::kThreadID, &thread_id_)) {
    LOG(ERROR) << "Exceptions require a thread id.";
    return false;
  }

#if defined(ARCH_CPU_X86_64)
  typedef x86_thread_state64_t thread_state_type;
  typedef x86_float_state64_t float_state_type;
#elif defined(ARCH_CPU_ARM64)
  typedef arm_thread_state64_t thread_state_type;
  typedef arm_neon_state64_t float_state_type;
#endif

  thread_state_type thread_state;
  float_state_type float_state;
  if (GetDataValueFromMap(exception_data, Key::kThreadState, &thread_state) &&
      GetDataValueFromMap(exception_data, Key::kFloatState, &float_state)) {
#if defined(ARCH_CPU_X86_64)
    x86_debug_state64_t empty_debug_state = {};
    InitializeCPUContextX86_64(&context_x86_64_,
                               THREAD_STATE_NONE,
                               nullptr,
                               0,
                               &thread_state,
                               &float_state,
                               &empty_debug_state);
#elif defined(ARCH_CPU_ARM64)
    arm_debug_state64_t empty_debug_state = {};
    InitializeCPUContextARM64(&context_arm64_,
                              THREAD_STATE_NONE,
                              nullptr,
                              0,
                              &thread_state,
                              &float_state,
                              &empty_debug_state);
#else
#error Port to your CPU architecture
#endif
  }

  GetDataValueFromMap(exception_data, Key::kSignalNumber, &exception_);
  GetDataValueFromMap(exception_data, Key::kSignalCode, &exception_info_);
  GetDataValueFromMap(exception_data, Key::kSignalAddress, &exception_address_);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ExceptionSnapshotIOSIntermediateDump::InitializeFromMachException(
    const IOSIntermediateDumpMap* exception_data,
    const IOSIntermediateDumpList* thread_list) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  DCHECK(exception_data);

  if (!GetDataValueFromMap(exception_data, Key::kThreadID, &thread_id_)) {
    LOG(ERROR) << "Exceptions require a thread id.";
    return false;
  }

  exception_type_t exception;
  if (GetDataValueFromMap(exception_data, Key::kException, &exception)) {
    codes_.push_back(exception);
    exception_ = exception;
  }

  mach_msg_type_number_t code_count;
  GetDataValueFromMap(exception_data, Key::kCodeCount, &code_count);

  const IOSIntermediateDumpData* code_dump =
      GetDataFromMap(exception_data, Key::kCode);
  if (code_dump) {
    const std::vector<uint8_t>& bytes = code_dump->bytes();
    const mach_exception_data_type_t* code =
        reinterpret_cast<const mach_exception_data_type_t*>(bytes.data());
    if (!code ||
        bytes.size() != sizeof(mach_exception_data_type_t) * code_count) {
      LOG(ERROR) << "Invalid mach exception code.";
    } else {
      // TODO: rationalize with the macOS implementation.
      for (mach_msg_type_number_t code_index = 0; code_index < code_count;
           ++code_index) {
        codes_.push_back(code[code_index]);
      }
      exception_info_ = code[0];
      exception_address_ = code[1];
    }
  }

  if (thread_list) {
    for (const auto& other_thread : *thread_list) {
      uint64_t other_thread_id;
      if (GetDataValueFromMap(
              other_thread.get(), Key::kThreadID, &other_thread_id)) {
        if (thread_id_ == other_thread_id) {
          LoadContextFromThread(exception_data, other_thread.get());
          break;
        }
      }
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ExceptionSnapshotIOSIntermediateDump::InitializeFromNSException(
    const IOSIntermediateDumpMap* exception_data,
    const IOSIntermediateDumpList* thread_list) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  DCHECK(exception_data);

  exception_ = EXC_SOFTWARE;
  exception_info_ = 0xDEADC0DE; /* uncaught NSException */

  if (!GetDataValueFromMap(exception_data, Key::kThreadID, &thread_id_)) {
    LOG(ERROR) << "Exceptions require a thread id.";
    return false;
  }

  if (thread_list) {
    for (const auto& other_thread : *thread_list) {
      uint64_t other_thread_id;
      if (GetDataValueFromMap(
              other_thread.get(), Key::kThreadID, &other_thread_id)) {
        if (thread_id_ == other_thread_id) {
          const IOSIntermediateDumpData* uncaught_exceptions =
              other_thread->GetAsData(Key::kThreadUncaughtNSExceptionFrames);
          if (uncaught_exceptions) {
            LoadContextFromUncaughtNSExceptionFrames(uncaught_exceptions,
                                                     other_thread.get());
          } else {
            LoadContextFromThread(exception_data, other_thread.get());
          }
          break;
        }
      }
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ExceptionSnapshotIOSIntermediateDump::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

uint64_t ExceptionSnapshotIOSIntermediateDump::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

uint32_t ExceptionSnapshotIOSIntermediateDump::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_;
}

uint32_t ExceptionSnapshotIOSIntermediateDump::ExceptionInfo() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_info_;
}

uint64_t ExceptionSnapshotIOSIntermediateDump::ExceptionAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_address_;
}

const std::vector<uint64_t>& ExceptionSnapshotIOSIntermediateDump::Codes()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return codes_;
}

std::vector<const MemorySnapshot*>
ExceptionSnapshotIOSIntermediateDump::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

void ExceptionSnapshotIOSIntermediateDump::LoadContextFromThread(
    const IOSIntermediateDumpMap* exception_data,
    const IOSIntermediateDumpMap* other_thread) {
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

  mach_msg_type_number_t state_count = 0;
  thread_state_flavor_t flavor = THREAD_STATE_NONE;
  if (GetDataValueFromMap(exception_data, Key::kStateCount, &state_count) &&
      GetDataValueFromMap(exception_data, Key::kFlavor, &flavor) &&
      GetDataValueFromMap(other_thread, Key::kThreadState, &thread_state) &&
      GetDataValueFromMap(other_thread, Key::kFloatState, &float_state) &&
      GetDataValueFromMap(other_thread, Key::kDebugState, &debug_state)) {
    size_t expected_length = ThreadStateLengthForFlavor(flavor);
    const IOSIntermediateDumpData* state_dump =
        GetDataFromMap(exception_data, Key::kState);
    if (state_dump) {
      const std::vector<uint8_t>& bytes = state_dump->bytes();
      size_t actual_length = bytes.size();
      if (expected_length == actual_length) {
        const ConstThreadState state =
            reinterpret_cast<const ConstThreadState>(bytes.data());
#if defined(ARCH_CPU_X86_64)
        InitializeCPUContextX86_64(&context_x86_64_,
                                   flavor,
                                   state,
                                   state_count,
                                   &thread_state,
                                   &float_state,
                                   &debug_state);
#elif defined(ARCH_CPU_ARM64)
        InitializeCPUContextARM64(&context_arm64_,
                                  flavor,
                                  state,
                                  state_count,
                                  &thread_state,
                                  &float_state,
                                  &debug_state);
#else
#error Port to your CPU architecture
#endif
      }
    }
  }

  // Normally, for EXC_BAD_ACCESS exceptions, the exception address is present
  // in code[1]. It may or may not be the instruction pointer address (usually
  // it’s not). code[1] may carry the exception address for other exception
  // types too, but it’s not guaranteed. But for all other exception types, the
  // instruction pointer will be the exception address, and in fact will be
  // equal to codes[1] when it’s carrying the exception address. In those cases,
  // just use the instruction pointer directly.
  bool code_1_is_exception_address = exception_ == EXC_BAD_ACCESS;

#if defined(ARCH_CPU_X86_64)
  // For x86 and x86_64 EXC_BAD_ACCESS exceptions, some code[0] values
  // indicate that code[1] does not (or may not) carry the exception address:
  // EXC_I386_GPFLT (10.9.5 xnu-2422.115.4/osfmk/i386/trap.c user_trap() for
  // T_GENERAL_PROTECTION) and the oddball (VM_PROT_READ | VM_PROT_EXECUTE)
  // which collides with EXC_I386_BOUNDFLT (10.9.5
  // xnu-2422.115.4/osfmk/i386/fpu.c fpextovrflt()). Other EXC_BAD_ACCESS
  // exceptions come through 10.9.5 xnu-2422.115.4/osfmk/i386/trap.c
  // user_page_fault_continue() and do contain the exception address in
  // code[1].
  if (exception_ == EXC_BAD_ACCESS &&
      (exception_info_ == EXC_I386_GPFLT ||
       exception_info_ == (VM_PROT_READ | VM_PROT_EXECUTE))) {
    code_1_is_exception_address = false;
  }
#endif

  if (!code_1_is_exception_address) {
    exception_address_ = context_.InstructionPointer();
  }
}

void ExceptionSnapshotIOSIntermediateDump::
    LoadContextFromUncaughtNSExceptionFrames(
        const IOSIntermediateDumpData* frames_dump,
        const IOSIntermediateDumpMap* other_thread) {
  const std::vector<uint8_t>& bytes = frames_dump->bytes();
  const uint64_t* frames = reinterpret_cast<const uint64_t*>(bytes.data());
  size_t num_frames = bytes.size() / sizeof(uint64_t);
  if (num_frames < 2) {
    return;
  }

#if defined(ARCH_CPU_X86_64)
  context_x86_64_ = {};
  context_x86_64_.rip = frames[0];  // instruction pointer
  context_x86_64_.rsp = frames[1];
#elif defined(ARCH_CPU_ARM64)
  context_arm64_ = {};
  context_arm64_.sp = 0;
  context_arm64_.pc = frames[0];
  context_arm64_.regs[30] = frames[1];  // link register
  context_arm64_.regs[29] = sizeof(uintptr_t);  // function pointers
#else
#error Port to your CPU architecture
#endif

  exception_address_ = frames[0];
}

}  // namespace internal
}  // namespace crashpad
