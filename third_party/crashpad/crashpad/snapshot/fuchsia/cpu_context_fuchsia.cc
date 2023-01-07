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

#include "snapshot/fuchsia/cpu_context_fuchsia.h"

#include <string.h>

namespace crashpad {
namespace internal {

#if defined(ARCH_CPU_X86_64)

void InitializeCPUContextX86_64_NoFloatingPoint(
    const zx_thread_state_general_regs_t& thread_context,
    CPUContextX86_64* context) {
  memset(context, 0, sizeof(*context));
  context->rax = thread_context.rax;
  context->rbx = thread_context.rbx;
  context->rcx = thread_context.rcx;
  context->rdx = thread_context.rdx;
  context->rdi = thread_context.rdi;
  context->rsi = thread_context.rsi;
  context->rbp = thread_context.rbp;
  context->rsp = thread_context.rsp;
  context->r8 = thread_context.r8;
  context->r9 = thread_context.r9;
  context->r10 = thread_context.r10;
  context->r11 = thread_context.r11;
  context->r12 = thread_context.r12;
  context->r13 = thread_context.r13;
  context->r14 = thread_context.r14;
  context->r15 = thread_context.r15;
  context->rip = thread_context.rip;
  context->rflags = thread_context.rflags;
}

#elif defined(ARCH_CPU_ARM64)

void InitializeCPUContextARM64(
    const zx_thread_state_general_regs_t& thread_context,
    const zx_thread_state_vector_regs_t& vector_context,
    CPUContextARM64* context) {
  memset(context, 0, sizeof(*context));

  // Fuchsia stores the link register (x30) on its own while Crashpad stores it
  // with the other general purpose x0-x28 and x29 frame pointer registers. So
  // we expect the size and number of elements to be off by one unit.
  static_assert(sizeof(context->regs) - sizeof(context->regs[30]) ==
                    sizeof(thread_context.r),
                "registers size mismatch");
  static_assert((sizeof(context->regs) - sizeof(context->regs[30])) /
                        sizeof(context->regs[0]) ==
                    sizeof(thread_context.r) / sizeof(thread_context.r[0]),
                "registers number of elements mismatch");
  memcpy(&context->regs, &thread_context.r, sizeof(thread_context.r));
  context->regs[30] = thread_context.lr;
  context->sp = thread_context.sp;
  context->pc = thread_context.pc;

  // Only the NZCV flags (bits 31 to 28 respectively) of the cpsr register are
  // readable and writable by userland on ARM64.
  constexpr uint32_t kNZCV = 0xf0000000;
  // Fuchsia uses the old "cspr" terminology from armv7 while Crashpad uses the
  // new "spsr" terminology for armv8.
  context->spsr = thread_context.cpsr & kNZCV;
  if (thread_context.cpsr >
      std::numeric_limits<decltype(context->spsr)>::max()) {
    LOG(WARNING) << "cpsr truncation: we only expect the first 32 bits to be "
                    "set in the cpsr";
  }
  context->spsr =
      static_cast<decltype(context->spsr)>(thread_context.cpsr) & kNZCV;

  context->fpcr = vector_context.fpcr;
  context->fpsr = vector_context.fpsr;
  static_assert(sizeof(context->fpsimd) == sizeof(vector_context.v),
                "registers size mismatch");
  memcpy(&context->fpsimd, &vector_context.v, sizeof(vector_context.v));
}

#endif  // ARCH_CPU_X86_64

}  // namespace internal
}  // namespace crashpad
