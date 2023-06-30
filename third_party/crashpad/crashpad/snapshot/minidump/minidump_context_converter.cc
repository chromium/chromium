// Copyright 2019 The Crashpad Authors
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

#include "snapshot/minidump/minidump_context_converter.h"

#include <string.h>

#include <iterator>

#include "base/logging.h"
#include "minidump/minidump_context.h"

namespace crashpad {
namespace internal {

MinidumpContextConverter::MinidumpContextConverter() : initialized_() {
  context_.architecture = CPUArchitecture::kCPUArchitectureUnknown;
}

bool MinidumpContextConverter::Initialize(
    CPUArchitecture arch,
    const std::vector<unsigned char>& minidump_context) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (minidump_context.size() == 0) {
    // Thread has no context.
    context_.architecture = CPUArchitecture::kCPUArchitectureUnknown;
    INITIALIZATION_STATE_SET_VALID(initialized_);
    return true;
  }

  context_.architecture = arch;

  if (context_.architecture == CPUArchitecture::kCPUArchitectureX86) {
    context_memory_.resize(sizeof(CPUContextX86));
    context_.x86 = reinterpret_cast<CPUContextX86*>(context_memory_.data());
    const MinidumpContextX86* src =
        reinterpret_cast<const MinidumpContextX86*>(minidump_context.data());
    if (minidump_context.size() < sizeof(MinidumpContextX86)) {
      return false;
    }

    if (!(src->context_flags & kMinidumpContextX86)) {
      return false;
    }

    if (src->context_flags & kMinidumpContextX86Extended) {
      context_.x86->fxsave = src->fxsave;
    } else if (src->context_flags & kMinidumpContextX86FloatingPoint) {
      CPUContextX86::FsaveToFxsave(src->fsave, &context_.x86->fxsave);
    }

    context_.x86->eax = src->eax;
    context_.x86->ebx = src->ebx;
    context_.x86->ecx = src->ecx;
    context_.x86->edx = src->edx;
    context_.x86->edi = src->edi;
    context_.x86->esi = src->esi;
    context_.x86->ebp = src->ebp;
    context_.x86->esp = src->esp;
    context_.x86->eip = src->eip;
    context_.x86->eflags = src->eflags;
    context_.x86->cs = static_cast<uint16_t>(src->cs);
    context_.x86->ds = static_cast<uint16_t>(src->ds);
    context_.x86->es = static_cast<uint16_t>(src->es);
    context_.x86->fs = static_cast<uint16_t>(src->fs);
    context_.x86->gs = static_cast<uint16_t>(src->gs);
    context_.x86->ss = static_cast<uint16_t>(src->ss);
    context_.x86->dr0 = src->dr0;
    context_.x86->dr1 = src->dr1;
    context_.x86->dr2 = src->dr2;
    context_.x86->dr3 = src->dr3;
    context_.x86->dr6 = src->dr6;
    context_.x86->dr7 = src->dr7;

    // Minidump passes no value for dr4/5. Our output context has space for
    // them. According to spec they're obsolete, but when present read as
    // aliases for dr6/7, so we'll do this.
    context_.x86->dr4 = src->dr6;
    context_.x86->dr5 = src->dr7;
  } else if (context_.architecture == CPUArchitecture::kCPUArchitectureX86_64) {
    context_memory_.resize(sizeof(CPUContextX86_64));
    context_.x86_64 =
        reinterpret_cast<CPUContextX86_64*>(context_memory_.data());
    const MinidumpContextAMD64* src =
        reinterpret_cast<const MinidumpContextAMD64*>(minidump_context.data());
    if (minidump_context.size() < sizeof(MinidumpContextAMD64)) {
      return false;
    }

    if (!(src->context_flags & kMinidumpContextAMD64)) {
      return false;
    }

    context_.x86_64->fxsave = src->fxsave;
    context_.x86_64->cs = src->cs;
    context_.x86_64->fs = src->fs;
    context_.x86_64->gs = src->gs;
    context_.x86_64->rflags = src->eflags;
    context_.x86_64->dr0 = src->dr0;
    context_.x86_64->dr1 = src->dr1;
    context_.x86_64->dr2 = src->dr2;
    context_.x86_64->dr3 = src->dr3;
    context_.x86_64->dr6 = src->dr6;
    context_.x86_64->dr7 = src->dr7;
    context_.x86_64->rax = src->rax;
    context_.x86_64->rcx = src->rcx;
    context_.x86_64->rdx = src->rdx;
    context_.x86_64->rbx = src->rbx;
    context_.x86_64->rsp = src->rsp;
    context_.x86_64->rbp = src->rbp;
    context_.x86_64->rsi = src->rsi;
    context_.x86_64->rdi = src->rdi;
    context_.x86_64->r8 = src->r8;
    context_.x86_64->r9 = src->r9;
    context_.x86_64->r10 = src->r10;
    context_.x86_64->r11 = src->r11;
    context_.x86_64->r12 = src->r12;
    context_.x86_64->r13 = src->r13;
    context_.x86_64->r14 = src->r14;
    context_.x86_64->r15 = src->r15;
    context_.x86_64->rip = src->rip;

    // See comments on x86 above.
    context_.x86_64->dr4 = src->dr6;
    context_.x86_64->dr5 = src->dr7;
  } else if (context_.architecture == CPUArchitecture::kCPUArchitectureARM) {
    context_memory_.resize(sizeof(CPUContextARM));
    context_.arm = reinterpret_cast<CPUContextARM*>(context_memory_.data());
    const MinidumpContextARM* src =
        reinterpret_cast<const MinidumpContextARM*>(minidump_context.data());
    if (minidump_context.size() < sizeof(MinidumpContextARM)) {
      return false;
    }

    if (!(src->context_flags & kMinidumpContextARM)) {
      return false;
    }

    for (size_t i = 0; i < std::size(src->regs); i++) {
      context_.arm->regs[i] = src->regs[i];
    }

    context_.arm->fp = src->fp;
    context_.arm->ip = src->ip;
    context_.arm->sp = src->sp;
    context_.arm->lr = src->lr;
    context_.arm->pc = src->pc;
    context_.arm->cpsr = src->cpsr;
    context_.arm->vfp_regs.fpscr = src->fpscr;

    for (size_t i = 0; i < std::size(src->vfp); i++) {
      context_.arm->vfp_regs.vfp[i] = src->vfp[i];
    }

    context_.arm->have_fpa_regs = false;
    context_.arm->have_vfp_regs =
        !!(src->context_flags & kMinidumpContextARMVFP);
  } else if (context_.architecture == CPUArchitecture::kCPUArchitectureARM64) {
    context_memory_.resize(sizeof(CPUContextARM64));
    context_.arm64 = reinterpret_cast<CPUContextARM64*>(context_memory_.data());
    const MinidumpContextARM64* src =
        reinterpret_cast<const MinidumpContextARM64*>(minidump_context.data());
    if (minidump_context.size() < sizeof(MinidumpContextARM64)) {
      return false;
    }

    if (!(src->context_flags & kMinidumpContextARM64)) {
      return false;
    }

    for (size_t i = 0; i < std::size(src->regs); i++) {
      context_.arm64->regs[i] = src->regs[i];
    }

    context_.arm64->regs[29] = src->fp;
    context_.arm64->regs[30] = src->lr;

    for (size_t i = 0; i < std::size(src->fpsimd); i++) {
      context_.arm64->fpsimd[i] = src->fpsimd[i];
    }

    context_.arm64->sp = src->sp;
    context_.arm64->pc = src->pc;
    context_.arm64->fpcr = src->fpcr;
    context_.arm64->fpsr = src->fpsr;
    context_.arm64->spsr = src->cpsr;
  } else if (context_.architecture == CPUArchitecture::kCPUArchitectureMIPSEL) {
    context_memory_.resize(sizeof(CPUContextMIPS));
    context_.mipsel = reinterpret_cast<CPUContextMIPS*>(context_memory_.data());
    const MinidumpContextMIPS* src =
        reinterpret_cast<const MinidumpContextMIPS*>(minidump_context.data());
    if (minidump_context.size() < sizeof(MinidumpContextMIPS)) {
      return false;
    }

    if (!(src->context_flags & kMinidumpContextMIPS)) {
      return false;
    }

    for (size_t i = 0; i < std::size(src->regs); i++) {
      context_.mipsel->regs[i] = src->regs[i];
    }

    context_.mipsel->mdhi = static_cast<uint32_t>(src->mdhi);
    context_.mipsel->mdlo = static_cast<uint32_t>(src->mdlo);
    context_.mipsel->dsp_control = src->dsp_control;

    for (size_t i = 0; i < std::size(src->hi); i++) {
      context_.mipsel->hi[i] = src->hi[i];
      context_.mipsel->lo[i] = src->lo[i];
    }

    context_.mipsel->cp0_epc = static_cast<uint32_t>(src->epc);
    context_.mipsel->cp0_badvaddr = static_cast<uint32_t>(src->badvaddr);
    context_.mipsel->cp0_status = src->status;
    context_.mipsel->cp0_cause = src->cause;
    context_.mipsel->fpcsr = src->fpcsr;
    context_.mipsel->fir = src->fir;

    memcpy(&context_.mipsel->fpregs, &src->fpregs, sizeof(src->fpregs));
  } else if (context_.architecture ==
             CPUArchitecture::kCPUArchitectureMIPS64EL) {
    context_memory_.resize(sizeof(CPUContextMIPS64));
    context_.mips64 =
        reinterpret_cast<CPUContextMIPS64*>(context_memory_.data());
    const MinidumpContextMIPS64* src =
        reinterpret_cast<const MinidumpContextMIPS64*>(minidump_context.data());
    if (minidump_context.size() < sizeof(MinidumpContextMIPS64)) {
      return false;
    }

    if (!(src->context_flags & kMinidumpContextMIPS64)) {
      return false;
    }

    for (size_t i = 0; i < std::size(src->regs); i++) {
      context_.mips64->regs[i] = src->regs[i];
    }

    context_.mips64->mdhi = src->mdhi;
    context_.mips64->mdlo = src->mdlo;
    context_.mips64->dsp_control = src->dsp_control;

    for (size_t i = 0; i < std::size(src->hi); i++) {
      context_.mips64->hi[i] = src->hi[i];
      context_.mips64->lo[i] = src->lo[i];
    }

    context_.mips64->cp0_epc = src->epc;
    context_.mips64->cp0_badvaddr = src->badvaddr;
    context_.mips64->cp0_status = src->status;
    context_.mips64->cp0_cause = src->cause;
    context_.mips64->fpcsr = src->fpcsr;
    context_.mips64->fir = src->fir;

    memcpy(&context_.mips64->fpregs, &src->fpregs, sizeof(src->fpregs));
  } else if (context_.architecture ==
             CPUArchitecture::kCPUArchitectureRISCV64) {
    context_memory_.resize(sizeof(CPUContextRISCV64));
    context_.riscv64 =
        reinterpret_cast<CPUContextRISCV64*>(context_memory_.data());
    const MinidumpContextRISCV64* src =
        reinterpret_cast<const MinidumpContextRISCV64*>(
            minidump_context.data());
    if (minidump_context.size() < sizeof(MinidumpContextRISCV64)) {
      return false;
    }

    if (!(src->context_flags & kMinidumpContextRISCV64)) {
      return false;
    }

    context_.riscv64->pc = src->pc;

    static_assert(sizeof(context_.riscv64->regs) == sizeof(src->regs),
                  "GPR size mismatch");
    memcpy(&context_.riscv64->regs, &src->regs, sizeof(src->regs));

    static_assert(sizeof(context_.riscv64->fpregs) == sizeof(src->fpregs),
                  "FPR size mismatch");
    memcpy(&context_.riscv64->fpregs, &src->fpregs, sizeof(src->fpregs));

    context_.riscv64->fcsr = src->fcsr;
  } else {
    // Architecture is listed as "unknown".
    DLOG(ERROR) << "Unknown architecture";
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

}  // namespace internal
}  // namespace crashpad
