/*
 * Copyright (C) 2022 The Android Open Source Project
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
#include <unwindstack/MachineRiscv64.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsRiscv64.h>
#include <unwindstack/UcontextRiscv64.h>
#include <unwindstack/UserRiscv64.h>

namespace unwindstack {

RegsRiscv64::RegsRiscv64()
    : RegsImpl<uint64_t>(RISCV64_REG_MAX, Location(LOCATION_REGISTER, RISCV64_REG_RA)) {}

ArchEnum RegsRiscv64::Arch() {
  return ARCH_RISCV64;
}

uint64_t RegsRiscv64::pc() {
  return regs_[RISCV64_REG_PC];
}

uint64_t RegsRiscv64::sp() {
  return regs_[RISCV64_REG_SP];
}

void RegsRiscv64::set_pc(uint64_t pc) {
  regs_[RISCV64_REG_PC] = pc;
}

void RegsRiscv64::set_sp(uint64_t sp) {
  regs_[RISCV64_REG_SP] = sp;
}

bool RegsRiscv64::SetPcFromReturnAddress(Memory*) {
  uint64_t ra = regs_[RISCV64_REG_RA];
  if (regs_[RISCV64_REG_PC] == ra) {
    return false;
  }

  regs_[RISCV64_REG_PC] = ra;
  return true;
}

void RegsRiscv64::IterateRegisters(std::function<void(const char*, uint64_t)> fn) {
  fn("pc", regs_[RISCV64_REG_PC]);
  fn("ra", regs_[RISCV64_REG_RA]);
  fn("sp", regs_[RISCV64_REG_SP]);
  fn("gp", regs_[RISCV64_REG_GP]);
  fn("tp", regs_[RISCV64_REG_TP]);
  fn("t0", regs_[RISCV64_REG_T0]);
  fn("t1", regs_[RISCV64_REG_T1]);
  fn("t2", regs_[RISCV64_REG_T2]);
  fn("t3", regs_[RISCV64_REG_T3]);
  fn("t4", regs_[RISCV64_REG_T4]);
  fn("t5", regs_[RISCV64_REG_T5]);
  fn("t6", regs_[RISCV64_REG_T6]);
  fn("s0", regs_[RISCV64_REG_S0]);
  fn("s1", regs_[RISCV64_REG_S1]);
  fn("s2", regs_[RISCV64_REG_S2]);
  fn("s3", regs_[RISCV64_REG_S3]);
  fn("s4", regs_[RISCV64_REG_S4]);
  fn("s5", regs_[RISCV64_REG_S5]);
  fn("s6", regs_[RISCV64_REG_S6]);
  fn("s7", regs_[RISCV64_REG_S7]);
  fn("s8", regs_[RISCV64_REG_S8]);
  fn("s9", regs_[RISCV64_REG_S9]);
  fn("s10", regs_[RISCV64_REG_S10]);
  fn("s11", regs_[RISCV64_REG_S11]);
  fn("a0", regs_[RISCV64_REG_A0]);
  fn("a1", regs_[RISCV64_REG_A1]);
  fn("a2", regs_[RISCV64_REG_A2]);
  fn("a3", regs_[RISCV64_REG_A3]);
  fn("a4", regs_[RISCV64_REG_A4]);
  fn("a5", regs_[RISCV64_REG_A5]);
  fn("a6", regs_[RISCV64_REG_A6]);
  fn("a7", regs_[RISCV64_REG_A7]);
}

Regs* RegsRiscv64::Read(void* remote_data) {
  riscv64_user_regs* user = reinterpret_cast<riscv64_user_regs*>(remote_data);

  RegsRiscv64* regs = new RegsRiscv64();
  memcpy(regs->RawData(), &user->regs[0], RISCV64_REG_MAX * sizeof(uint64_t));
  // uint64_t* reg_data = reinterpret_cast<uint64_t*>(regs->RawData());
  return regs;
}

Regs* RegsRiscv64::CreateFromUcontext(void* ucontext) {
  riscv64_ucontext_t* riscv64_ucontext = reinterpret_cast<riscv64_ucontext_t*>(ucontext);

  RegsRiscv64* regs = new RegsRiscv64();
  memcpy(regs->RawData(), &riscv64_ucontext->uc_mcontext.__gregs[0],
         RISCV64_REG_MAX * sizeof(uint64_t));
  return regs;
}

bool RegsRiscv64::StepIfSignalHandler(uint64_t elf_offset, Elf* elf, Memory* process_memory) {
  // Read from elf memory since it is usually more expensive to read from
  // process memory.
  uint64_t data;
  if (!elf->memory()->ReadFully(elf_offset, &data, sizeof(data))) {
    return false;
  }
  // Look for the kernel sigreturn function.
  // __kernel_rt_sigreturn:
  // li a7, __NR_rt_sigreturn
  // scall

  const uint8_t li_scall[] = {0x93, 0x08, 0xb0, 0x08, 0x73, 0x00, 0x00, 0x00};
  if (memcmp(&data, &li_scall, 8) != 0) {
    return false;
  }

  // SP + sizeof(siginfo_t) + uc_mcontext offset + PC offset.
  if (!process_memory->ReadFully(regs_[RISCV64_REG_SP] + 0x80 + 0xb0 + 0x00, regs_.data(),
                                 sizeof(uint64_t) * (RISCV64_REG_MAX))) {
    return false;
  }
  return true;
}

Regs* RegsRiscv64::Clone() {
  return new RegsRiscv64(*this);
}

}  // namespace unwindstack
