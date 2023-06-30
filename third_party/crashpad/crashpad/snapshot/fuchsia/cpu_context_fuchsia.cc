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

void InitializeCPUContextX86_64(
    const zx_thread_state_general_regs_t& thread_context,
    const zx_thread_state_fp_regs_t& float_context,
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

  context->fxsave.fcw = float_context.fcw;
  context->fxsave.fsw = float_context.fsw;
  context->fxsave.ftw = float_context.ftw;
  context->fxsave.fop = float_context.fop;
  context->fxsave.fpu_ip_64 = float_context.fip;
  context->fxsave.fpu_dp_64 = float_context.fdp;

  for (size_t i = 0; i < std::size(float_context.st); ++i) {
    memcpy(&context->fxsave.st_mm[i],
           &float_context.st[i],
           sizeof(float_context.st[i]));
  }
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

#elif defined(ARCH_CPU_RISCV64)

void InitializeCPUContextRISCV64(
    const zx_thread_state_general_regs_t& thread_context,
    const zx_thread_state_fp_regs_t& float_context,
    CPUContextRISCV64* context) {
  context->pc = thread_context.pc;
  context->regs[0] = thread_context.ra;
  context->regs[1] = thread_context.sp;
  context->regs[2] = thread_context.gp;
  context->regs[3] = thread_context.tp;
  context->regs[4] = thread_context.t0;
  context->regs[5] = thread_context.t1;
  context->regs[6] = thread_context.t2;
  context->regs[7] = thread_context.s0;
  context->regs[8] = thread_context.s1;
  context->regs[9] = thread_context.a0;
  context->regs[10] = thread_context.a1;
  context->regs[11] = thread_context.a2;
  context->regs[12] = thread_context.a3;
  context->regs[13] = thread_context.a4;
  context->regs[14] = thread_context.a5;
  context->regs[15] = thread_context.a6;
  context->regs[16] = thread_context.a7;
  context->regs[17] = thread_context.s2;
  context->regs[18] = thread_context.s3;
  context->regs[19] = thread_context.s4;
  context->regs[20] = thread_context.s5;
  context->regs[21] = thread_context.s6;
  context->regs[22] = thread_context.s7;
  context->regs[23] = thread_context.s8;
  context->regs[24] = thread_context.s9;
  context->regs[25] = thread_context.s10;
  context->regs[26] = thread_context.s11;
  context->regs[27] = thread_context.t3;
  context->regs[28] = thread_context.t4;
  context->regs[29] = thread_context.t5;
  context->regs[30] = thread_context.t6;

  for (size_t i = 0; i < std::size(context->fpregs); ++i) {
    context->fpregs[i] = float_context.q[i].low;
  }

  context->fcsr = float_context.fcsr;
}

#endif  // ARCH_CPU_X86_64

}  // namespace internal
}  // namespace crashpad
