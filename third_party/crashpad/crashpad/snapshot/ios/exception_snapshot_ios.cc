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

#include "snapshot/ios/exception_snapshot_ios.h"

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "snapshot/cpu_context.h"
#include "snapshot/mac/cpu_context_mac.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {

namespace internal {

ExceptionSnapshotIOS::ExceptionSnapshotIOS()
    : ExceptionSnapshot(),
      context_(),
      codes_(),
      thread_id_(0),
      exception_address_(0),
      exception_(0),
      exception_info_(0),
      initialized_() {}

ExceptionSnapshotIOS::~ExceptionSnapshotIOS() {}

void ExceptionSnapshotIOS::InitializeFromSignal(const siginfo_t* siginfo,
                                                const ucontext_t* context) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  mcontext_t mcontext = context->uc_mcontext;
#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
  x86_debug_state64_t empty_debug_state = {};
  InitializeCPUContextX86_64(&context_x86_64_,
                             THREAD_STATE_NONE,
                             nullptr,
                             0,
                             &mcontext->__ss,
                             &mcontext->__fs,
                             &empty_debug_state);
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arm64_;
  arm_debug_state64_t empty_debug_state = {};
  InitializeCPUContextARM64(&context_arm64_,
                            THREAD_STATE_NONE,
                            nullptr,
                            0,
                            &mcontext->__ss,
                            &mcontext->__ns,
                            &empty_debug_state);
#else
#error Port to your CPU architecture
#endif

  // Thread ID.
  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(mach_thread_self(),
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "thread_identifier_info";
  } else {
    thread_id_ = identifier_info.thread_id;
  }

  exception_ = siginfo->si_signo;
  exception_info_ = siginfo->si_code;

  // TODO(justincohen): Investigate recording more codes_.

  exception_address_ = FromPointerCast<uintptr_t>(siginfo->si_addr);

  // TODO(justincohen): Record the source of the exception (signal, mach, etc).

  INITIALIZATION_STATE_SET_VALID(initialized_);
}

void ExceptionSnapshotIOS::InitializeFromMachException(
    exception_behavior_t behavior,
    thread_t exception_thread,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t flavor,
    ConstThreadState state,
    mach_msg_type_number_t state_count) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  codes_.push_back(exception);
  // TODO: rationalize with the macOS implementation.
  for (mach_msg_type_number_t code_index = 0; code_index < code_count;
       ++code_index) {
    codes_.push_back(code[code_index]);
  }
  exception_ = exception;
  exception_info_ = code[0];

  // For serialization, float_state and, on x86, debug_state, will be identical
  // between here and the thread_snapshot version for thread_id.  That means
  // this block getting float_state and debug_state can be skipped when doing
  // proper serialization.
#if defined(ARCH_CPU_X86_64)
  x86_thread_state64_t thread_state;
  x86_float_state64_t float_state;
  x86_debug_state64_t debug_state;
  mach_msg_type_number_t thread_state_count = x86_THREAD_STATE64_COUNT;
  mach_msg_type_number_t float_state_count = x86_FLOAT_STATE64_COUNT;
  mach_msg_type_number_t debug_state_count = x86_DEBUG_STATE64_COUNT;
  const thread_state_flavor_t kThreadStateFlavor = x86_THREAD_STATE64;
  const thread_state_flavor_t kFloatStateFlavor = x86_FLOAT_STATE64;
  const thread_state_flavor_t kDebugStateFlavor = x86_DEBUG_STATE64;
#elif defined(ARCH_CPU_ARM64)
  arm_thread_state64_t thread_state;
  arm_neon_state64_t float_state;
  arm_debug_state64_t debug_state;
  mach_msg_type_number_t float_state_count = ARM_NEON_STATE64_COUNT;
  mach_msg_type_number_t thread_state_count = ARM_THREAD_STATE64_COUNT;
  mach_msg_type_number_t debug_state_count = ARM_DEBUG_STATE64_COUNT;
  const thread_state_flavor_t kThreadStateFlavor = ARM_THREAD_STATE64;
  const thread_state_flavor_t kFloatStateFlavor = ARM_NEON_STATE64;
  const thread_state_flavor_t kDebugStateFlavor = ARM_DEBUG_STATE64;
#endif

  kern_return_t kr =
      thread_get_state(exception_thread,
                       kThreadStateFlavor,
                       reinterpret_cast<thread_state_t>(&thread_state),
                       &thread_state_count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "thread_get_state(" << kThreadStateFlavor << ")";
  }

  kr = thread_get_state(exception_thread,
                        kFloatStateFlavor,
                        reinterpret_cast<thread_state_t>(&float_state),
                        &float_state_count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "thread_get_state(" << kFloatStateFlavor << ")";
  }

  kr = thread_get_state(exception_thread,
                        kDebugStateFlavor,
                        reinterpret_cast<thread_state_t>(&debug_state),
                        &debug_state_count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "thread_get_state(" << kDebugStateFlavor << ")";
  }

#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
  InitializeCPUContextX86_64(&context_x86_64_,
                             flavor,
                             state,
                             state_count,
                             &thread_state,
                             &float_state,
                             &debug_state);
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arm64_;
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

  // Thread ID.
  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kr = thread_info(mach_thread_self(),
                   THREAD_IDENTIFIER_INFO,
                   reinterpret_cast<thread_info_t>(&identifier_info),
                   &count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "thread_identifier_info";
  } else {
    thread_id_ = identifier_info.thread_id;
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

  if (code_1_is_exception_address) {
    exception_address_ = code[1];
  } else {
    exception_address_ = context_.InstructionPointer();
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
}

const CPUContext* ExceptionSnapshotIOS::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

uint64_t ExceptionSnapshotIOS::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

uint32_t ExceptionSnapshotIOS::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_;
}

uint32_t ExceptionSnapshotIOS::ExceptionInfo() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_info_;
}

uint64_t ExceptionSnapshotIOS::ExceptionAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_address_;
}

const std::vector<uint64_t>& ExceptionSnapshotIOS::Codes() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return codes_;
}

std::vector<const MemorySnapshot*> ExceptionSnapshotIOS::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
