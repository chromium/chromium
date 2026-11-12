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

#if defined(__BIONIC__) && !defined(__ANDROID_NDK__)
#include <bionic/pac.h>
#endif

#include <unwindstack/Elf.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/UcontextArm64.h>
#include <unwindstack/UserArm64.h>

namespace unwindstack {

RegsArm64::RegsArm64()
    : RegsImpl<uint64_t>(ARM64_REG_LAST, Location(LOCATION_REGISTER, ARM64_REG_LR)) {
  ResetPseudoRegisters();
  pac_mask_ = 0;
}

ArchEnum RegsArm64::Arch() {
  return ARCH_ARM64;
}

uint64_t RegsArm64::pc() {
  return regs_[ARM64_REG_PC];
}

uint64_t RegsArm64::sp() {
  return regs_[ARM64_REG_SP];
}

static uint64_t strip_pac(uint64_t pc, uint64_t mask) {
  // If the target is aarch64 then the return address may have been
  // signed using the Armv8.3-A Pointer Authentication extension. The
  // original return address can be restored by stripping out the
  // authentication code using a mask or xpaclri. xpaclri is a NOP on
  // pre-Armv8.3-A architectures.
  if (mask) {
    pc &= ~mask;
  } else {
#if defined(__BIONIC__) && !defined(__ANDROID_NDK__)
    pc = __bionic_clear_pac_bits(pc);
#endif
  }
  return pc;
}

void RegsArm64::set_pc(uint64_t pc) {
  if ((0 != pc) && IsRASigned()) {
    pc = strip_pac(pc, pac_mask_);
  }
  regs_[ARM64_REG_PC] = pc;
}

void RegsArm64::set_sp(uint64_t sp) {
  regs_[ARM64_REG_SP] = sp;
}

void RegsArm64::fallback_pc() {
  // As a last resort, try stripping the PC of the pointer
  // authentication code.
  regs_[ARM64_REG_PC] = strip_pac(regs_[ARM64_REG_PC], pac_mask_);
}

bool RegsArm64::SetPcFromReturnAddress(Memory*) {
  uint64_t lr = regs_[ARM64_REG_LR];
  if (regs_[ARM64_REG_PC] == lr) {
    return false;
  }

  regs_[ARM64_REG_PC] = lr;
  return true;
}

void RegsArm64::IterateRegisters(std::function<void(const char*, uint64_t)> fn) {
  fn("x0", regs_[ARM64_REG_R0]);
  fn("x1", regs_[ARM64_REG_R1]);
  fn("x2", regs_[ARM64_REG_R2]);
  fn("x3", regs_[ARM64_REG_R3]);
  fn("x4", regs_[ARM64_REG_R4]);
  fn("x5", regs_[ARM64_REG_R5]);
  fn("x6", regs_[ARM64_REG_R6]);
  fn("x7", regs_[ARM64_REG_R7]);
  fn("x8", regs_[ARM64_REG_R8]);
  fn("x9", regs_[ARM64_REG_R9]);
  fn("x10", regs_[ARM64_REG_R10]);
  fn("x11", regs_[ARM64_REG_R11]);
  fn("x12", regs_[ARM64_REG_R12]);
  fn("x13", regs_[ARM64_REG_R13]);
  fn("x14", regs_[ARM64_REG_R14]);
  fn("x15", regs_[ARM64_REG_R15]);
  fn("x16", regs_[ARM64_REG_R16]);
  fn("x17", regs_[ARM64_REG_R17]);
  fn("x18", regs_[ARM64_REG_R18]);
  fn("x19", regs_[ARM64_REG_R19]);
  fn("x20", regs_[ARM64_REG_R20]);
  fn("x21", regs_[ARM64_REG_R21]);
  fn("x22", regs_[ARM64_REG_R22]);
  fn("x23", regs_[ARM64_REG_R23]);
  fn("x24", regs_[ARM64_REG_R24]);
  fn("x25", regs_[ARM64_REG_R25]);
  fn("x26", regs_[ARM64_REG_R26]);
  fn("x27", regs_[ARM64_REG_R27]);
  fn("x28", regs_[ARM64_REG_R28]);
  fn("x29", regs_[ARM64_REG_R29]);
  fn("lr", regs_[ARM64_REG_LR]);
  fn("sp", regs_[ARM64_REG_SP]);
  fn("pc", regs_[ARM64_REG_PC]);
  fn("pst", regs_[ARM64_REG_PSTATE]);
}

Regs* RegsArm64::Read(void* remote_data) {
  arm64_user_regs* user = reinterpret_cast<arm64_user_regs*>(remote_data);

  RegsArm64* regs = new RegsArm64();
  memcpy(regs->RawData(), &user->regs[0], (ARM64_REG_R30 + 1) * sizeof(uint64_t));
  uint64_t* reg_data = reinterpret_cast<uint64_t*>(regs->RawData());
  reg_data[ARM64_REG_SP] = user->sp;
  reg_data[ARM64_REG_PC] = user->pc;
  reg_data[ARM64_REG_PSTATE] = user->pstate;
  return regs;
}

Regs* RegsArm64::CreateFromUcontext(void* ucontext) {
  arm64_ucontext_t* arm64_ucontext = reinterpret_cast<arm64_ucontext_t*>(ucontext);

  RegsArm64* regs = new RegsArm64();
  memcpy(regs->RawData(), &arm64_ucontext->uc_mcontext.regs[0], ARM64_REG_LAST * sizeof(uint64_t));
  return regs;
}

bool RegsArm64::StepIfSignalHandler(uint64_t elf_offset, Elf* elf, Memory* process_memory) {
  // Read from elf memory since it is usually more expensive to read from
  // process memory.
  uint64_t data;
  if (!elf->memory()->ReadFully(elf_offset, &data, sizeof(data))) {
    return false;
  }

  // Look for the kernel sigreturn function.
  // __kernel_rt_sigreturn:
  // 0xd2801168     mov x8, #0x8b
  // 0xd4000001     svc #0x0
  if (data != 0xd4000001d2801168ULL) {
    return false;
  }

  // SP + sizeof(siginfo_t) + uc_mcontext offset + X0 offset.
  if (!process_memory->ReadFully(regs_[ARM64_REG_SP] + 0x80 + 0xb0 + 0x08, regs_.data(),
                                 sizeof(uint64_t) * ARM64_REG_LAST)) {
    return false;
  }
  return true;
}

void RegsArm64::ResetPseudoRegisters(void) {
  // DWARF for AArch64 says RA_SIGN_STATE should be initialized to 0.
  this->SetPseudoRegister(Arm64Reg::ARM64_PREG_RA_SIGN_STATE, 0);
}

bool RegsArm64::SetPseudoRegister(uint16_t id, uint64_t value) {
  if ((id >= Arm64Reg::ARM64_PREG_FIRST) && (id < Arm64Reg::ARM64_PREG_LAST)) {
    pseudo_regs_[id - Arm64Reg::ARM64_PREG_FIRST] = value;
    return true;
  }
  return false;
}

bool RegsArm64::GetPseudoRegister(uint16_t id, uint64_t* value) {
  if ((id >= Arm64Reg::ARM64_PREG_FIRST) && (id < Arm64Reg::ARM64_PREG_LAST)) {
    *value = pseudo_regs_[id - Arm64Reg::ARM64_PREG_FIRST];
    return true;
  }
  return false;
}

bool RegsArm64::IsRASigned() {
  uint64_t value;
  auto result = this->GetPseudoRegister(Arm64Reg::ARM64_PREG_RA_SIGN_STATE, &value);
  return (result && (value != 0));
}

void RegsArm64::SetPACMask(uint64_t mask) {
  pac_mask_ = mask;
}

Regs* RegsArm64::Clone() {
  return new RegsArm64(*this);
}

}  // namespace unwindstack
