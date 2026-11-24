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

#pragma once

#include <stdint.h>

#include <functional>

#include <unwindstack/Elf.h>
#include <unwindstack/Regs.h>

namespace unwindstack {

// Forward declarations.
class Memory;

class RegsArm : public RegsImpl<uint32_t> {
 public:
  RegsArm();
  virtual ~RegsArm() = default;

  ArchEnum Arch() override final;

  bool SetPcFromReturnAddress(Memory* process_memory) override;

  bool StepIfSignalHandler(uint64_t elf_offset, Elf* elf, Memory* process_memory) override;

  void IterateRegisters(std::function<void(const char*, uint64_t)>) override final;

  uint64_t pc() override;
  uint64_t sp() override;

  void set_pc(uint64_t pc) override;
  void set_sp(uint64_t sp) override;

  Regs* Clone() override final;

  static Regs* Read(const void* data);

  static Regs* CreateFromUcontext(void* ucontext);
};

}  // namespace unwindstack
