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
#include <string.h>

#include <functional>

#include <unwindstack/Elf.h>
#include <unwindstack/MachineX86_64.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsX86_64.h>
#include <unwindstack/UcontextX86_64.h>
#include <unwindstack/UserX86_64.h>

namespace unwindstack {

RegsX86_64::RegsX86_64() : RegsImpl<uint64_t>(X86_64_REG_LAST, Location(LOCATION_SP_OFFSET, -8)) {}

ArchEnum RegsX86_64::Arch() {
  return ARCH_X86_64;
}

uint64_t RegsX86_64::pc() {
  return regs_[X86_64_REG_PC];
}

uint64_t RegsX86_64::sp() {
  return regs_[X86_64_REG_SP];
}

void RegsX86_64::set_pc(uint64_t pc) {
  regs_[X86_64_REG_PC] = pc;
}

void RegsX86_64::set_sp(uint64_t sp) {
  regs_[X86_64_REG_SP] = sp;
}

bool RegsX86_64::SetPcFromReturnAddress(Memory* process_memory) {
  // Attempt to get the return address from the top of the stack.
  uint64_t new_pc;
  if (!process_memory->ReadFully(regs_[X86_64_REG_SP], &new_pc, sizeof(new_pc)) ||
      new_pc == regs_[X86_64_REG_PC]) {
    return false;
  }

  regs_[X86_64_REG_PC] = new_pc;
  return true;
}

void RegsX86_64::IterateRegisters(std::function<void(const char*, uint64_t)> fn) {
  fn("rax", regs_[X86_64_REG_RAX]);
  fn("rbx", regs_[X86_64_REG_RBX]);
  fn("rcx", regs_[X86_64_REG_RCX]);
  fn("rdx", regs_[X86_64_REG_RDX]);
  fn("r8", regs_[X86_64_REG_R8]);
  fn("r9", regs_[X86_64_REG_R9]);
  fn("r10", regs_[X86_64_REG_R10]);
  fn("r11", regs_[X86_64_REG_R11]);
  fn("r12", regs_[X86_64_REG_R12]);
  fn("r13", regs_[X86_64_REG_R13]);
  fn("r14", regs_[X86_64_REG_R14]);
  fn("r15", regs_[X86_64_REG_R15]);
  fn("rdi", regs_[X86_64_REG_RDI]);
  fn("rsi", regs_[X86_64_REG_RSI]);
  fn("rbp", regs_[X86_64_REG_RBP]);
  fn("rsp", regs_[X86_64_REG_RSP]);
  fn("rip", regs_[X86_64_REG_RIP]);
}

Regs* RegsX86_64::Read(void* remote_data) {
  x86_64_user_regs* user = reinterpret_cast<x86_64_user_regs*>(remote_data);

  RegsX86_64* regs = new RegsX86_64();
  (*regs)[X86_64_REG_RAX] = user->rax;
  (*regs)[X86_64_REG_RBX] = user->rbx;
  (*regs)[X86_64_REG_RCX] = user->rcx;
  (*regs)[X86_64_REG_RDX] = user->rdx;
  (*regs)[X86_64_REG_R8] = user->r8;
  (*regs)[X86_64_REG_R9] = user->r9;
  (*regs)[X86_64_REG_R10] = user->r10;
  (*regs)[X86_64_REG_R11] = user->r11;
  (*regs)[X86_64_REG_R12] = user->r12;
  (*regs)[X86_64_REG_R13] = user->r13;
  (*regs)[X86_64_REG_R14] = user->r14;
  (*regs)[X86_64_REG_R15] = user->r15;
  (*regs)[X86_64_REG_RDI] = user->rdi;
  (*regs)[X86_64_REG_RSI] = user->rsi;
  (*regs)[X86_64_REG_RBP] = user->rbp;
  (*regs)[X86_64_REG_RSP] = user->rsp;
  (*regs)[X86_64_REG_RIP] = user->rip;

  return regs;
}

void RegsX86_64::SetFromUcontext(x86_64_ucontext_t* ucontext) {
  // R8-R15
  memcpy(&regs_[X86_64_REG_R8], &ucontext->uc_mcontext.r8, 8 * sizeof(uint64_t));

  // Rest of the registers.
  regs_[X86_64_REG_RDI] = ucontext->uc_mcontext.rdi;
  regs_[X86_64_REG_RSI] = ucontext->uc_mcontext.rsi;
  regs_[X86_64_REG_RBP] = ucontext->uc_mcontext.rbp;
  regs_[X86_64_REG_RBX] = ucontext->uc_mcontext.rbx;
  regs_[X86_64_REG_RDX] = ucontext->uc_mcontext.rdx;
  regs_[X86_64_REG_RAX] = ucontext->uc_mcontext.rax;
  regs_[X86_64_REG_RCX] = ucontext->uc_mcontext.rcx;
  regs_[X86_64_REG_RSP] = ucontext->uc_mcontext.rsp;
  regs_[X86_64_REG_RIP] = ucontext->uc_mcontext.rip;
}

Regs* RegsX86_64::CreateFromUcontext(void* ucontext) {
  x86_64_ucontext_t* x86_64_ucontext = reinterpret_cast<x86_64_ucontext_t*>(ucontext);

  RegsX86_64* regs = new RegsX86_64();
  regs->SetFromUcontext(x86_64_ucontext);
  return regs;
}

bool RegsX86_64::StepIfSignalHandler(uint64_t elf_offset, Elf* elf, Memory* process_memory) {
  // Read from elf memory since it is usually more expensive to read from
  // process memory.
  uint64_t data;
  auto elf_memory = elf->memory();
  if (!elf_memory->ReadFully(elf_offset, &data, sizeof(data)) || data != 0x0f0000000fc0c748) {
    return false;
  }

  uint8_t data2;
  if (!elf_memory->ReadFully(elf_offset + 8, &data2, sizeof(data2)) || data2 != 0x05) {
    return false;
  }

  // __restore_rt:
  // 0x48 0xc7 0xc0 0x0f 0x00 0x00 0x00   mov $0xf,%rax
  // 0x0f 0x05                            syscall

  // Read the mcontext data from the stack.
  // sp points to the ucontext data structure, read only the mcontext part.
  x86_64_ucontext_t x86_64_ucontext;
  if (!process_memory->ReadFully(regs_[X86_64_REG_SP] + 0x28, &x86_64_ucontext.uc_mcontext,
                                 sizeof(x86_64_mcontext_t))) {
    return false;
  }
  SetFromUcontext(&x86_64_ucontext);
  return true;
}

Regs* RegsX86_64::Clone() {
  return new RegsX86_64(*this);
}

}  // namespace unwindstack
