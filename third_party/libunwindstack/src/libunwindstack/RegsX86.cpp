/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>

#include <functional>

#include <unwindstack/Elf.h>
#include <unwindstack/MachineX86.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsX86.h>
#include <unwindstack/UcontextX86.h>
#include <unwindstack/UserX86.h>

namespace unwindstack {

RegsX86::RegsX86() : RegsImpl<uint32_t>(X86_REG_LAST, Location(LOCATION_SP_OFFSET, -4)) {}

ArchEnum RegsX86::Arch() {
  return ARCH_X86;
}

uint64_t RegsX86::pc() {
  return regs_[X86_REG_PC];
}

uint64_t RegsX86::sp() {
  return regs_[X86_REG_SP];
}

void RegsX86::set_pc(uint64_t pc) {
  regs_[X86_REG_PC] = static_cast<uint32_t>(pc);
}

void RegsX86::set_sp(uint64_t sp) {
  regs_[X86_REG_SP] = static_cast<uint32_t>(sp);
}

bool RegsX86::SetPcFromReturnAddress(Memory* process_memory) {
  // Attempt to get the return address from the top of the stack.
  uint32_t new_pc;
  if (!process_memory->ReadFully(regs_[X86_REG_SP], &new_pc, sizeof(new_pc)) ||
      new_pc == regs_[X86_REG_PC]) {
    return false;
  }

  regs_[X86_REG_PC] = new_pc;
  return true;
}

void RegsX86::IterateRegisters(std::function<void(const char*, uint64_t)> fn) {
  fn("eax", regs_[X86_REG_EAX]);
  fn("ebx", regs_[X86_REG_EBX]);
  fn("ecx", regs_[X86_REG_ECX]);
  fn("edx", regs_[X86_REG_EDX]);
  fn("ebp", regs_[X86_REG_EBP]);
  fn("edi", regs_[X86_REG_EDI]);
  fn("esi", regs_[X86_REG_ESI]);
  fn("esp", regs_[X86_REG_ESP]);
  fn("eip", regs_[X86_REG_EIP]);
}

Regs* RegsX86::Read(void* user_data) {
  x86_user_regs* user = reinterpret_cast<x86_user_regs*>(user_data);

  RegsX86* regs = new RegsX86();
  (*regs)[X86_REG_EAX] = user->eax;
  (*regs)[X86_REG_EBX] = user->ebx;
  (*regs)[X86_REG_ECX] = user->ecx;
  (*regs)[X86_REG_EDX] = user->edx;
  (*regs)[X86_REG_EBP] = user->ebp;
  (*regs)[X86_REG_EDI] = user->edi;
  (*regs)[X86_REG_ESI] = user->esi;
  (*regs)[X86_REG_ESP] = user->esp;
  (*regs)[X86_REG_EIP] = user->eip;

  return regs;
}

void RegsX86::SetFromUcontext(x86_ucontext_t* ucontext) {
  // Put the registers in the expected order.
  regs_[X86_REG_EDI] = ucontext->uc_mcontext.edi;
  regs_[X86_REG_ESI] = ucontext->uc_mcontext.esi;
  regs_[X86_REG_EBP] = ucontext->uc_mcontext.ebp;
  regs_[X86_REG_ESP] = ucontext->uc_mcontext.esp;
  regs_[X86_REG_EBX] = ucontext->uc_mcontext.ebx;
  regs_[X86_REG_EDX] = ucontext->uc_mcontext.edx;
  regs_[X86_REG_ECX] = ucontext->uc_mcontext.ecx;
  regs_[X86_REG_EAX] = ucontext->uc_mcontext.eax;
  regs_[X86_REG_EIP] = ucontext->uc_mcontext.eip;
}

Regs* RegsX86::CreateFromUcontext(void* ucontext) {
  x86_ucontext_t* x86_ucontext = reinterpret_cast<x86_ucontext_t*>(ucontext);

  RegsX86* regs = new RegsX86();
  regs->SetFromUcontext(x86_ucontext);
  return regs;
}

bool RegsX86::StepIfSignalHandler(uint64_t elf_offset, Elf* elf, Memory* process_memory) {
  // Read from elf memory since it is usually more expensive to read from
  // process memory.
  uint64_t data;
  if (!elf->memory()->ReadFully(elf_offset, &data, sizeof(data))) {
    return false;
  }

  if (data == 0x80cd00000077b858ULL) {
    // Without SA_SIGINFO set, the return sequence is:
    //
    //   __restore:
    //   0x58                            pop %eax
    //   0xb8 0x77 0x00 0x00 0x00        movl 0x77,%eax
    //   0xcd 0x80                       int 0x80
    //
    // SP points at arguments:
    //   int signum
    //   struct sigcontext (same format as mcontext)
    struct x86_mcontext_t context;
    if (!process_memory->ReadFully(regs_[X86_REG_SP] + 4, &context, sizeof(context))) {
      return false;
    }
    regs_[X86_REG_EBP] = context.ebp;
    regs_[X86_REG_ESP] = context.esp;
    regs_[X86_REG_EBX] = context.ebx;
    regs_[X86_REG_EDX] = context.edx;
    regs_[X86_REG_ECX] = context.ecx;
    regs_[X86_REG_EAX] = context.eax;
    regs_[X86_REG_EIP] = context.eip;
    return true;
  } else if ((data & 0x00ffffffffffffffULL) == 0x0080cd000000adb8ULL) {
    // With SA_SIGINFO set, the return sequence is:
    //
    //   __restore_rt:
    //   0xb8 0xad 0x00 0x00 0x00        movl 0xad,%eax
    //   0xcd 0x80                       int 0x80
    //
    // SP points at arguments:
    //   int signum
    //   siginfo*
    //   ucontext*

    // Get the location of the sigcontext data.
    uint32_t ptr;
    if (!process_memory->ReadFully(regs_[X86_REG_SP] + 8, &ptr, sizeof(ptr))) {
      return false;
    }
    // Only read the portion of the data structure we care about.
    x86_ucontext_t x86_ucontext;
    if (!process_memory->ReadFully(ptr + 0x14, &x86_ucontext.uc_mcontext, sizeof(x86_mcontext_t))) {
      return false;
    }
    SetFromUcontext(&x86_ucontext);
    return true;
  }
  return false;
}

Regs* RegsX86::Clone() {
  return new RegsX86(*this);
}

}  // namespace unwindstack
