/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_ITERATOR_H_
#define ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_ITERATOR_H_

#include <iterator>

#include <android-base/logging.h>

#include "base/macros.h"
#include "dex_instruction.h"

namespace art {

class DexInstructionPcPair {
 public:
  ALWAYS_INLINE const Instruction& Inst() const {
    return *Instruction::At(instructions_ + DexPc());
  }

  ALWAYS_INLINE const Instruction* operator->() const {
    return &Inst();
  }

  ALWAYS_INLINE uint32_t DexPc() const {
    return dex_pc_;
  }

  ALWAYS_INLINE const uint16_t* Instructions() const {
    return instructions_;
  }

 protected:
  explicit DexInstructionPcPair(const uint16_t* instructions, uint32_t dex_pc)
      : instructions_(instructions), dex_pc_(dex_pc) {}

  const uint16_t* instructions_ = nullptr;
  uint32_t dex_pc_ = 0;

  friend class DexInstructionIteratorBase;
  friend class DexInstructionIterator;
  friend class SafeDexInstructionIterator;
};

// Base helper class to prevent duplicated comparators.
class DexInstructionIteratorBase {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = DexInstructionPcPair;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = void;

  explicit DexInstructionIteratorBase(const Instruction* inst, uint32_t dex_pc)
      : data_(reinterpret_cast<const uint16_t*>(inst), dex_pc) {}

  const Instruction& Inst() const {
    return data_.Inst();
  }

  // Return the dex pc for an iterator compared to the code item begin.
  ALWAYS_INLINE uint32_t DexPc() const {
    return data_.DexPc();
  }

  // Instructions from the start of the code item.
  ALWAYS_INLINE const uint16_t* Instructions() const {
    return data_.Instructions();
  }

 protected:
  DexInstructionPcPair data_;
};

static ALWAYS_INLINE inline bool operator==(const DexInstructionIteratorBase& lhs,
                                            const DexInstructionIteratorBase& rhs) {
  DCHECK_EQ(lhs.Instructions(), rhs.Instructions()) << "Comparing different code items.";
  return lhs.DexPc() == rhs.DexPc();
}

static inline bool operator!=(const DexInstructionIteratorBase& lhs,
                              const DexInstructionIteratorBase& rhs) {
  return !(lhs == rhs);
}

static inline bool operator<(const DexInstructionIteratorBase& lhs,
                             const DexInstructionIteratorBase& rhs) {
  DCHECK_EQ(lhs.Instructions(), rhs.Instructions()) << "Comparing different code items.";
  return lhs.DexPc() < rhs.DexPc();
}

static inline bool operator>(const DexInstructionIteratorBase& lhs,
                             const DexInstructionIteratorBase& rhs) {
  return rhs < lhs;
}

static inline bool operator<=(const DexInstructionIteratorBase& lhs,
                              const DexInstructionIteratorBase& rhs) {
  return !(rhs < lhs);
}

static inline bool operator>=(const DexInstructionIteratorBase& lhs,
                              const DexInstructionIteratorBase& rhs) {
  return !(lhs < rhs);
}

// A helper class for a code_item's instructions using range based for loop syntax.
class DexInstructionIterator : public DexInstructionIteratorBase {
 public:
  using DexInstructionIteratorBase::DexInstructionIteratorBase;

  explicit DexInstructionIterator(const uint16_t* inst, uint32_t dex_pc)
      : DexInstructionIteratorBase(inst != nullptr ? Instruction::At(inst) : nullptr, dex_pc) {}

  explicit DexInstructionIterator(const DexInstructionPcPair& pair)
      : DexInstructionIterator(pair.Instructions(), pair.DexPc()) {}

  // Value after modification.
  DexInstructionIterator& operator++() {
    data_.dex_pc_ += Inst().SizeInCodeUnits();
    return *this;
  }

  // Value before modification.
  DexInstructionIterator operator++(int) {
    DexInstructionIterator temp = *this;
    ++*this;
    return temp;
  }

  const value_type& operator*() const {
    return data_;
  }

  const Instruction* operator->() const {
    return &data_.Inst();
  }

  // Return the dex pc for the iterator.
  ALWAYS_INLINE uint32_t DexPc() const {
    return data_.DexPc();
  }
};

// A safe version of DexInstructionIterator that is guaranteed to not go past the end of the code
// item.
class SafeDexInstructionIterator : public DexInstructionIteratorBase {
 public:
  explicit SafeDexInstructionIterator(const DexInstructionIteratorBase& start,
                                      const DexInstructionIteratorBase& end)
      : DexInstructionIteratorBase(&start.Inst(), start.DexPc())
      , num_code_units_(end.DexPc()) {
    DCHECK_EQ(start.Instructions(), end.Instructions())
        << "start and end must be in the same code item.";
  }

  // Value after modification, does not read past the end of the allowed region. May increment past
  // the end of the code item though.
  SafeDexInstructionIterator& operator++() {
    AssertValid();
    const size_t size_code_units = Inst().CodeUnitsRequiredForSizeComputation();
    const size_t available = NumCodeUnits() - DexPc();
    if (UNLIKELY(size_code_units > available)) {
      error_state_ = true;
      return *this;
    }
    const size_t instruction_code_units = Inst().SizeInCodeUnits();
    if (UNLIKELY(instruction_code_units > available)) {
      error_state_ = true;
      return *this;
    }
    data_.dex_pc_ += instruction_code_units;
    return *this;
  }

  // Value before modification.
  SafeDexInstructionIterator operator++(int) {
    SafeDexInstructionIterator temp = *this;
    ++*this;
    return temp;
  }

  const value_type& operator*() const {
    AssertValid();
    return data_;
  }

  const Instruction* operator->() const {
    AssertValid();
    return &data_.Inst();
  }

  // Return the current instruction of the iterator.
  ALWAYS_INLINE const Instruction& Inst() const {
    return data_.Inst();
  }

  const uint16_t* Instructions() const {
    return data_.Instructions();
  }

  // Returns true if the iterator is in an error state. This occurs when an instruction couldn't
  // have its size computed without reading past the end iterator.
  bool IsErrorState() const {
    return error_state_;
  }

 private:
  ALWAYS_INLINE void AssertValid() const {
    DCHECK(!IsErrorState());
    DCHECK_LT(DexPc(), NumCodeUnits());
  }

  ALWAYS_INLINE uint32_t NumCodeUnits() const {
    return num_code_units_;
  }

  const uint32_t num_code_units_ = 0;
  bool error_state_ = false;
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_ITERATOR_H_
