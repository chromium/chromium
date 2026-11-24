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

#include <stack>
#include <string>
#include <type_traits>
#include <vector>

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfLocation.h>
#include <unwindstack/DwarfMemory.h>
#include <unwindstack/DwarfStructs.h>

namespace unwindstack {

// Forward declarations.
enum ArchEnum : uint8_t;

// DWARF Standard home: http://dwarfstd.org/
// This code is based on DWARF 4: http://http://dwarfstd.org/doc/DWARF4.pdf
// See section 6.4.2.1 for a description of the DW_CFA_xxx values.

class DwarfCfaInfo {
 public:
  enum DisplayType : uint8_t {
    DWARF_DISPLAY_NONE = 0,
    DWARF_DISPLAY_REGISTER,
    DWARF_DISPLAY_NUMBER,
    DWARF_DISPLAY_SIGNED_NUMBER,
    DWARF_DISPLAY_EVAL_BLOCK,
    DWARF_DISPLAY_ADDRESS,
    DWARF_DISPLAY_SET_LOC,
    DWARF_DISPLAY_ADVANCE_LOC,
  };

  struct Info {
    // It may seem cleaner to just change the type of 'name' to 'const char *'.
    // However, having a pointer here would require relocation at runtime,
    // causing 'kTable' to be placed in data.rel.ro section instead of rodata
    // section, adding memory pressure to the system.  Note that this is only
    // safe because this is only used in C++ code.  C++ standard, unlike C
    // standard, mandates the array size to be large enough to hold the NULL
    // terminator when initialized with a string literal.
    const char name[36];
    uint8_t supported_version;
    uint8_t num_operands;
    uint8_t operands[2];
    uint8_t display_operands[2];
  };

  const static Info kTable[64];
};

template <typename AddressType>
class DwarfCfa {
  // Signed version of AddressType
  typedef typename std::make_signed<AddressType>::type SignedType;

 public:
  DwarfCfa(DwarfMemory* memory, const DwarfFde* fde, ArchEnum arch)
      : memory_(memory), fde_(fde), arch_(arch) {}
  virtual ~DwarfCfa() = default;

  bool GetLocationInfo(uint64_t pc, uint64_t start_offset, uint64_t end_offset,
                       DwarfLocations* loc_regs);

  bool Log(uint32_t indent, uint64_t pc, uint64_t start_offset, uint64_t end_offset);

  const DwarfErrorData& last_error() { return last_error_; }
  DwarfErrorCode LastErrorCode() { return last_error_.code; }
  uint64_t LastErrorAddress() { return last_error_.address; }

  AddressType cur_pc() { return cur_pc_; }

  void set_cie_loc_regs(const DwarfLocations* cie_loc_regs) { cie_loc_regs_ = cie_loc_regs; }

 protected:
  std::string GetOperandString(uint8_t operand, uint64_t value, uint64_t* cur_pc);

  bool LogOffsetRegisterString(uint32_t indent, uint64_t cfa_offset, uint8_t reg);

  bool LogInstruction(uint32_t indent, uint64_t cfa_offset, uint8_t op, uint64_t* cur_pc);

 private:
  DwarfErrorData last_error_;
  DwarfMemory* memory_;
  const DwarfFde* fde_;
  ArchEnum arch_;

  AddressType cur_pc_;
  const DwarfLocations* cie_loc_regs_ = nullptr;
  std::vector<AddressType> operands_;
  std::stack<DwarfLocations> loc_reg_state_;

  // CFA processing functions.
  bool cfa_nop(DwarfLocations*);
  bool cfa_set_loc(DwarfLocations*);
  bool cfa_advance_loc(DwarfLocations*);
  bool cfa_offset(DwarfLocations*);
  bool cfa_restore(DwarfLocations*);
  bool cfa_undefined(DwarfLocations*);
  bool cfa_same_value(DwarfLocations*);
  bool cfa_register(DwarfLocations*);
  bool cfa_remember_state(DwarfLocations*);
  bool cfa_restore_state(DwarfLocations*);
  bool cfa_def_cfa(DwarfLocations*);
  bool cfa_def_cfa_register(DwarfLocations*);
  bool cfa_def_cfa_offset(DwarfLocations*);
  bool cfa_def_cfa_expression(DwarfLocations*);
  bool cfa_expression(DwarfLocations*);
  bool cfa_offset_extended_sf(DwarfLocations*);
  bool cfa_def_cfa_sf(DwarfLocations*);
  bool cfa_def_cfa_offset_sf(DwarfLocations*);
  bool cfa_val_offset(DwarfLocations*);
  bool cfa_val_offset_sf(DwarfLocations*);
  bool cfa_val_expression(DwarfLocations*);
  bool cfa_gnu_negative_offset_extended(DwarfLocations*);
  bool cfa_aarch64_negate_ra_state(DwarfLocations*);

  using process_func = bool (DwarfCfa::*)(DwarfLocations*);
  constexpr static process_func kCallbackTable[64] = {
      // 0x00 DW_CFA_nop
      &DwarfCfa::cfa_nop,
      // 0x01 DW_CFA_set_loc
      &DwarfCfa::cfa_set_loc,
      // 0x02 DW_CFA_advance_loc1
      &DwarfCfa::cfa_advance_loc,
      // 0x03 DW_CFA_advance_loc2
      &DwarfCfa::cfa_advance_loc,
      // 0x04 DW_CFA_advance_loc4
      &DwarfCfa::cfa_advance_loc,
      // 0x05 DW_CFA_offset_extended
      &DwarfCfa::cfa_offset,
      // 0x06 DW_CFA_restore_extended
      &DwarfCfa::cfa_restore,
      // 0x07 DW_CFA_undefined
      &DwarfCfa::cfa_undefined,
      // 0x08 DW_CFA_same_value
      &DwarfCfa::cfa_same_value,
      // 0x09 DW_CFA_register
      &DwarfCfa::cfa_register,
      // 0x0a DW_CFA_remember_state
      &DwarfCfa::cfa_remember_state,
      // 0x0b DW_CFA_restore_state
      &DwarfCfa::cfa_restore_state,
      // 0x0c DW_CFA_def_cfa
      &DwarfCfa::cfa_def_cfa,
      // 0x0d DW_CFA_def_cfa_register
      &DwarfCfa::cfa_def_cfa_register,
      // 0x0e DW_CFA_def_cfa_offset
      &DwarfCfa::cfa_def_cfa_offset,
      // 0x0f DW_CFA_def_cfa_expression
      &DwarfCfa::cfa_def_cfa_expression,
      // 0x10 DW_CFA_expression
      &DwarfCfa::cfa_expression,
      // 0x11 DW_CFA_offset_extended_sf
      &DwarfCfa::cfa_offset_extended_sf,
      // 0x12 DW_CFA_def_cfa_sf
      &DwarfCfa::cfa_def_cfa_sf,
      // 0x13 DW_CFA_def_cfa_offset_sf
      &DwarfCfa::cfa_def_cfa_offset_sf,
      // 0x14 DW_CFA_val_offset
      &DwarfCfa::cfa_val_offset,
      // 0x15 DW_CFA_val_offset_sf
      &DwarfCfa::cfa_val_offset_sf,
      // 0x16 DW_CFA_val_expression
      &DwarfCfa::cfa_val_expression,
      // 0x17 illegal cfa
      nullptr,
      // 0x18 illegal cfa
      nullptr,
      // 0x19 illegal cfa
      nullptr,
      // 0x1a illegal cfa
      nullptr,
      // 0x1b illegal cfa
      nullptr,
      // 0x1c DW_CFA_lo_user (Treat this as illegal)
      nullptr,
      // 0x1d illegal cfa
      nullptr,
      // 0x1e illegal cfa
      nullptr,
      // 0x1f illegal cfa
      nullptr,
      // 0x20 illegal cfa
      nullptr,
      // 0x21 illegal cfa
      nullptr,
      // 0x22 illegal cfa
      nullptr,
      // 0x23 illegal cfa
      nullptr,
      // 0x24 illegal cfa
      nullptr,
      // 0x25 illegal cfa
      nullptr,
      // 0x26 illegal cfa
      nullptr,
      // 0x27 illegal cfa
      nullptr,
      // 0x28 illegal cfa
      nullptr,
      // 0x29 illegal cfa
      nullptr,
      // 0x2a illegal cfa
      nullptr,
      // 0x2b illegal cfa
      nullptr,
      // 0x2c illegal cfa
      nullptr,
      // 0x2d DW_CFA_AARCH64_negate_ra_state (aarch64 only)
      // DW_CFA_GNU_window_save on other architectures.
      &DwarfCfa::cfa_aarch64_negate_ra_state,
      // 0x2e DW_CFA_GNU_args_size
      &DwarfCfa::cfa_nop,
      // 0x2f DW_CFA_GNU_negative_offset_extended
      &DwarfCfa::cfa_gnu_negative_offset_extended,
      // 0x30 illegal cfa
      nullptr,
      // 0x31 illegal cfa
      nullptr,
      // 0x32 illegal cfa
      nullptr,
      // 0x33 illegal cfa
      nullptr,
      // 0x34 illegal cfa
      nullptr,
      // 0x35 illegal cfa
      nullptr,
      // 0x36 illegal cfa
      nullptr,
      // 0x37 illegal cfa
      nullptr,
      // 0x38 illegal cfa
      nullptr,
      // 0x39 illegal cfa
      nullptr,
      // 0x3a illegal cfa
      nullptr,
      // 0x3b illegal cfa
      nullptr,
      // 0x3c illegal cfa
      nullptr,
      // 0x3d illegal cfa
      nullptr,
      // 0x3e illegal cfa
      nullptr,
      // 0x3f DW_CFA_hi_user (Treat this as illegal)
      nullptr,
  };
};

}  // namespace unwindstack
