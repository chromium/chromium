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

#include <elf.h>
#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <memory>
#include <unordered_map>

#include <unwindstack/ElfInterface.h>
#include <unwindstack/Memory.h>

namespace unwindstack {

class ElfInterfaceArm : public ElfInterface32 {
 public:
  ElfInterfaceArm(std::shared_ptr<Memory>& memory) : ElfInterface32(memory) {}
  virtual ~ElfInterfaceArm() = default;

  class iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = uint32_t;
    using difference_type = ptrdiff_t;
    using pointer = uint32_t*;
    using reference = uint32_t&;

    iterator(ElfInterfaceArm* interface, size_t index) : interface_(interface), index_(index) { }

    iterator& operator++() { index_++; return *this; }
    iterator& operator++(int increment) { index_ += increment; return *this; }
    iterator& operator--() { index_--; return *this; }
    iterator& operator--(int decrement) { index_ -= decrement; return *this; }

    bool operator==(const iterator& rhs) const { return this->index_ == rhs.index_; }
    bool operator!=(const iterator& rhs) const { return this->index_ != rhs.index_; }

    uint32_t operator*() {
      uint32_t addr = interface_->addrs_[index_];
      if (addr == 0) {
        if (!interface_->GetPrel31Addr(interface_->start_offset_ + index_ * 8, &addr)) {
          return 0;
        }
        interface_->addrs_[index_] = addr;
      }
      return addr;
    }

   private:
    ElfInterfaceArm* interface_ = nullptr;
    size_t index_ = 0;
  };

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, total_entries_); }

  bool Init(int64_t* section_bias) override;

  bool GetPrel31Addr(uint32_t offset, uint32_t* addr);

  bool FindEntry(uint32_t pc, uint64_t* entry_offset);

  void HandleUnknownType(uint32_t type, uint64_t ph_offset, uint64_t ph_filesz) override;

  bool Step(uint64_t pc, Regs* regs, Memory* process_memory, bool* finished,
            bool* is_signal_frame) override;

  bool StepExidx(uint64_t pc, Regs* regs, Memory* process_memory, bool* finished);

  bool GetFunctionName(uint64_t addr, SharedString* name, uint64_t* offset) override;

  uint64_t start_offset() { return start_offset_; }

  size_t total_entries() { return total_entries_; }

  void set_load_bias(uint64_t load_bias) { load_bias_ = load_bias; }

 protected:
  uint64_t start_offset_ = 0;
  size_t total_entries_ = 0;
  uint64_t load_bias_ = 0;

  std::unordered_map<size_t, uint32_t> addrs_;
};

}  // namespace unwindstack
