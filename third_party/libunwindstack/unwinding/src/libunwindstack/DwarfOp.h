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

#include <deque>
#include <string>
#include <type_traits>
#include <vector>

#include <unwindstack/DwarfError.h>

#include "DwarfEncoding.h"
#include "RegsInfo.h"

namespace unwindstack {

// Forward declarations.
class DwarfMemory;
class Memory;
template <typename AddressType>
class RegsImpl;

template <typename AddressType>
class DwarfOp {
  // Signed version of AddressType
  typedef typename std::make_signed<AddressType>::type SignedType;

 public:
  DwarfOp(DwarfMemory* memory, Memory* regular_memory)
      : memory_(memory), regular_memory_(regular_memory) {}
  virtual ~DwarfOp() = default;

  bool Decode();

  bool Eval(uint64_t start, uint64_t end);

  void GetLogInfo(uint64_t start, uint64_t end, std::vector<std::string>* lines);

  AddressType StackAt(size_t index) { return stack_[index]; }
  size_t StackSize() { return stack_.size(); }

  void set_regs_info(RegsInfo<AddressType>* regs_info) { regs_info_ = regs_info; }

  const DwarfErrorData& last_error() { return last_error_; }
  DwarfErrorCode LastErrorCode() { return last_error_.code; }
  uint64_t LastErrorAddress() { return last_error_.address; }

  bool dex_pc_set() { return dex_pc_set_; }

  bool is_register() { return is_register_; }

  uint8_t cur_op() { return cur_op_; }

  Memory* regular_memory() { return regular_memory_; }

 protected:
  AddressType OperandAt(size_t index) { return operands_[index]; }
  size_t OperandsSize() { return operands_.size(); }

  AddressType StackPop() {
    AddressType value = stack_.front();
    stack_.pop_front();
    return value;
  }

 private:
  DwarfMemory* memory_;
  Memory* regular_memory_;

  RegsInfo<AddressType>* regs_info_;
  bool dex_pc_set_ = false;
  bool is_register_ = false;
  DwarfErrorData last_error_{DWARF_ERROR_NONE, 0};
  uint8_t cur_op_;
  std::vector<AddressType> operands_;
  std::deque<AddressType> stack_;

  inline AddressType bool_to_dwarf_bool(bool value) { return value ? 1 : 0; }

  // Op processing functions.
  bool op_deref();
  bool op_deref_size();
  bool op_push();
  bool op_dup();
  bool op_drop();
  bool op_over();
  bool op_pick();
  bool op_swap();
  bool op_rot();
  bool op_abs();
  bool op_and();
  bool op_div();
  bool op_minus();
  bool op_mod();
  bool op_mul();
  bool op_neg();
  bool op_not();
  bool op_or();
  bool op_plus();
  bool op_plus_uconst();
  bool op_shl();
  bool op_shr();
  bool op_shra();
  bool op_xor();
  bool op_bra();
  bool op_eq();
  bool op_ge();
  bool op_gt();
  bool op_le();
  bool op_lt();
  bool op_ne();
  bool op_skip();
  bool op_lit();
  bool op_reg();
  bool op_regx();
  bool op_breg();
  bool op_bregx();
  bool op_nop();
  bool op_not_implemented();

  using OpHandleFuncPtr = bool (DwarfOp::*)();
  static const OpHandleFuncPtr kOpHandleFuncList[];
};

}  // namespace unwindstack
