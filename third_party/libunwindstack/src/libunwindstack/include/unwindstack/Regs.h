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
#include <unistd.h>

#include <functional>
#include <string>
#include <vector>

#include <unwindstack/Arch.h>
#include <unwindstack/Error.h>

namespace unwindstack {

// Forward declarations.
class Elf;
class Memory;

class Regs {
 public:
  enum LocationEnum : uint8_t {
    LOCATION_UNKNOWN = 0,
    LOCATION_REGISTER,
    LOCATION_SP_OFFSET,
  };

  struct Location {
    Location(LocationEnum type, int16_t value) : type(type), value(value) {}

    LocationEnum type;
    int16_t value;
  };

  Regs(uint16_t total_regs, const Location& return_loc)
      : total_regs_(total_regs), return_loc_(return_loc) {}
  virtual ~Regs() = default;

  virtual ArchEnum Arch() = 0;

  bool Is32Bit() { return ArchIs32Bit(Arch()); }

  virtual void* RawData() = 0;
  virtual uint64_t pc() = 0;
  virtual uint64_t sp() = 0;

  virtual void set_pc(uint64_t pc) = 0;
  virtual void set_sp(uint64_t sp) = 0;

  uint64_t dex_pc() { return dex_pc_; }
  void set_dex_pc(uint64_t dex_pc) { dex_pc_ = dex_pc; }

  virtual void fallback_pc() {}

  virtual void ResetPseudoRegisters() {}
  virtual bool SetPseudoRegister(uint16_t, uint64_t) { return false; }
  virtual bool GetPseudoRegister(uint16_t, uint64_t*) { return false; }

  virtual bool StepIfSignalHandler(uint64_t elf_offset, Elf* elf, Memory* process_memory) = 0;

  virtual bool SetPcFromReturnAddress(Memory* process_memory) = 0;

  virtual void IterateRegisters(std::function<void(const char*, uint64_t)>) = 0;

  uint16_t total_regs() { return total_regs_; }

  virtual Regs* Clone() = 0;

  static ArchEnum CurrentArch();
  static ArchEnum RemoteGetArch(pid_t pid, ErrorCode* error_code = nullptr);
  static Regs* RemoteGet(pid_t pid, ErrorCode* error_code = nullptr);
  static Regs* CreateFromUcontext(ArchEnum arch, void* ucontext);
  static Regs* CreateFromLocal();

 protected:
  uint16_t total_regs_;
  Location return_loc_;
  uint64_t dex_pc_ = 0;
};

template <typename AddressType>
class RegsImpl : public Regs {
 public:
  RegsImpl(uint16_t total_regs, Location return_loc)
      : Regs(total_regs, return_loc), regs_(total_regs) {}
  virtual ~RegsImpl() = default;

  inline AddressType& operator[](size_t reg) { return regs_[reg]; }

  void* RawData() override { return regs_.data(); }

  virtual void IterateRegisters(std::function<void(const char*, uint64_t)> fn) override {
    for (size_t i = 0; i < regs_.size(); ++i) {
      fn(std::to_string(i).c_str(), regs_[i]);
    }
  }

 protected:
  std::vector<AddressType> regs_;
};

uint64_t GetPcAdjustment(uint64_t rel_pc, Elf* elf, ArchEnum arch);

}  // namespace unwindstack
