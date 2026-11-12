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
#include <unwindstack/MachineArm.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsArm.h>
#include <unwindstack/UcontextArm.h>
#include <unwindstack/UserArm.h>

namespace unwindstack {

RegsArm::RegsArm() : RegsImpl<uint32_t>(ARM_REG_LAST, Location(LOCATION_REGISTER, ARM_REG_LR)) {}

ArchEnum RegsArm::Arch() {
  return ARCH_ARM;
}

uint64_t RegsArm::pc() {
  return regs_[ARM_REG_PC];
}

uint64_t RegsArm::sp() {
  return regs_[ARM_REG_SP];
}

void RegsArm::set_pc(uint64_t pc) {
  regs_[ARM_REG_PC] = pc;
}

void RegsArm::set_sp(uint64_t sp) {
  regs_[ARM_REG_SP] = sp;
}

bool RegsArm::SetPcFromReturnAddress(Memory*) {
  uint32_t lr = regs_[ARM_REG_LR];
  if (regs_[ARM_REG_PC] == lr) {
    return false;
  }

  regs_[ARM_REG_PC] = lr;
  return true;
}

void RegsArm::IterateRegisters(std::function<void(const char*, uint64_t)> fn) {
  fn("r0", regs_[ARM_REG_R0]);
  fn("r1", regs_[ARM_REG_R1]);
  fn("r2", regs_[ARM_REG_R2]);
  fn("r3", regs_[ARM_REG_R3]);
  fn("r4", regs_[ARM_REG_R4]);
  fn("r5", regs_[ARM_REG_R5]);
  fn("r6", regs_[ARM_REG_R6]);
  fn("r7", regs_[ARM_REG_R7]);
  fn("r8", regs_[ARM_REG_R8]);
  fn("r9", regs_[ARM_REG_R9]);
  fn("r10", regs_[ARM_REG_R10]);
  fn("r11", regs_[ARM_REG_R11]);
  fn("ip", regs_[ARM_REG_R12]);
  fn("sp", regs_[ARM_REG_SP]);
  fn("lr", regs_[ARM_REG_LR]);
  fn("pc", regs_[ARM_REG_PC]);
}

Regs* RegsArm::Read(void* remote_data) {
  arm_user_regs* user = reinterpret_cast<arm_user_regs*>(remote_data);

  RegsArm* regs = new RegsArm();
  memcpy(regs->RawData(), &user->regs[0], ARM_REG_LAST * sizeof(uint32_t));
  return regs;
}

Regs* RegsArm::CreateFromUcontext(void* ucontext) {
  arm_ucontext_t* arm_ucontext = reinterpret_cast<arm_ucontext_t*>(ucontext);

  RegsArm* regs = new RegsArm();
  memcpy(regs->RawData(), &arm_ucontext->uc_mcontext.regs[0], ARM_REG_LAST * sizeof(uint32_t));
  return regs;
}

bool RegsArm::StepIfSignalHandler(uint64_t elf_offset, Elf* elf, Memory* process_memory) {
  // Read from elf memory since it is usually more expensive to read from
  // process memory.
  uint32_t data;
  if (!elf->memory()->ReadFully(elf_offset, &data, sizeof(data))) {
    return false;
  }

  uint64_t offset = 0;
  if (data == 0xe3a07077 || data == 0xef900077 || data == 0xdf002777) {
    uint64_t sp = regs_[ARM_REG_SP];
    // non-RT sigreturn call.
    // __restore:
    //
    // Form 1 (arm):
    // 0x77 0x70              mov r7, #0x77
    // 0xa0 0xe3              svc 0x00000000
    //
    // Form 2 (arm):
    // 0x77 0x00 0x90 0xef    svc 0x00900077
    //
    // Form 3 (thumb):
    // 0x77 0x27              movs r7, #77
    // 0x00 0xdf              svc 0
    if (!process_memory->ReadFully(sp, &data, sizeof(data))) {
      return false;
    }
    if (data == 0x5ac3c35a) {
      // SP + uc_mcontext offset + r0 offset.
      offset = sp + 0x14 + 0xc;
    } else {
      // SP + r0 offset
      offset = sp + 0xc;
    }
  } else if (data == 0xe3a070ad || data == 0xef9000ad || data == 0xdf0027ad) {
    uint64_t sp = regs_[ARM_REG_SP];
    // RT sigreturn call.
    // __restore_rt:
    //
    // Form 1 (arm):
    // 0xad 0x70      mov r7, #0xad
    // 0xa0 0xe3      svc 0x00000000
    //
    // Form 2 (arm):
    // 0xad 0x00 0x90 0xef    svc 0x009000ad
    //
    // Form 3 (thumb):
    // 0xad 0x27              movs r7, #ad
    // 0x00 0xdf              svc 0
    if (!process_memory->ReadFully(sp, &data, sizeof(data))) {
      return false;
    }
    if (data == sp + 8) {
      // SP + 8 + sizeof(siginfo_t) + uc_mcontext_offset + r0 offset
      offset = sp + 8 + 0x80 + 0x14 + 0xc;
    } else {
      // SP + sizeof(siginfo_t) + uc_mcontext_offset + r0 offset
      offset = sp + 0x80 + 0x14 + 0xc;
    }
  }
  if (offset == 0) {
    return false;
  }

  if (!process_memory->ReadFully(offset, regs_.data(), sizeof(uint32_t) * ARM_REG_LAST)) {
    return false;
  }
  return true;
}

Regs* RegsArm::Clone() {
  return new RegsArm(*this);
}

}  // namespace unwindstack
