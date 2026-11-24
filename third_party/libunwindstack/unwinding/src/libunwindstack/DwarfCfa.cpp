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

#include <inttypes.h>
#include <stdint.h>

#include <string>
#include <type_traits>
#include <vector>

#include <android-base/macros.h>
#include <android-base/stringprintf.h>

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfLocation.h>
#include <unwindstack/Elf.h>
#include <unwindstack/Log.h>
#include <unwindstack/MachineArm64.h>

#include "DwarfCfa.h"
#include "DwarfEncoding.h"
#include "DwarfOp.h"

namespace unwindstack {

template <typename AddressType>
constexpr typename DwarfCfa<AddressType>::process_func DwarfCfa<AddressType>::kCallbackTable[64];

template <typename AddressType>
bool DwarfCfa<AddressType>::GetLocationInfo(uint64_t pc, uint64_t start_offset, uint64_t end_offset,
                                            DwarfLocations* loc_regs) {
  if (cie_loc_regs_ != nullptr) {
    for (const auto& entry : *cie_loc_regs_) {
      (*loc_regs)[entry.first] = entry.second;
    }
  }
  last_error_.code = DWARF_ERROR_NONE;
  last_error_.address = 0;

  memory_->set_cur_offset(start_offset);
  uint64_t cfa_offset;
  cur_pc_ = fde_->pc_start;
  loc_regs->pc_start = cur_pc_;
  while (true) {
    if (cur_pc_ > pc) {
      loc_regs->pc_end = cur_pc_;
      return true;
    }
    if ((cfa_offset = memory_->cur_offset()) >= end_offset) {
      loc_regs->pc_end = fde_->pc_end;
      return true;
    }
    loc_regs->pc_start = cur_pc_;
    operands_.clear();
    // Read the cfa information.
    uint8_t cfa_value;
    if (!memory_->ReadBytes(&cfa_value, 1)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_->cur_offset();
      return false;
    }
    uint8_t cfa_low = cfa_value & 0x3f;
    // Check the 2 high bits.
    switch (cfa_value >> 6) {
      case 1:
        cur_pc_ += cfa_low * fde_->cie->code_alignment_factor;
        break;
      case 2: {
        uint64_t offset;
        if (!memory_->ReadULEB128(&offset)) {
          last_error_.code = DWARF_ERROR_MEMORY_INVALID;
          last_error_.address = memory_->cur_offset();
          return false;
        }
        SignedType signed_offset =
            static_cast<SignedType>(offset) * fde_->cie->data_alignment_factor;
        (*loc_regs)[cfa_low] = {.type = DWARF_LOCATION_OFFSET,
                                .values = {static_cast<uint64_t>(signed_offset)}};
        break;
      }
      case 3: {
        if (cie_loc_regs_ == nullptr) {
          Log::Error("Invalid: restore while processing cie.");
          last_error_.code = DWARF_ERROR_ILLEGAL_STATE;
          return false;
        }

        auto reg_entry = cie_loc_regs_->find(cfa_low);
        if (reg_entry == cie_loc_regs_->end()) {
          loc_regs->erase(cfa_low);
        } else {
          (*loc_regs)[cfa_low] = reg_entry->second;
        }
        break;
      }
      case 0: {
        const auto handle_func = DwarfCfa<AddressType>::kCallbackTable[cfa_low];
        if (handle_func == nullptr) {
          last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
          return false;
        }

        const auto cfa = &DwarfCfaInfo::kTable[cfa_low];
        for (size_t i = 0; i < cfa->num_operands; i++) {
          if (cfa->operands[i] == DW_EH_PE_block) {
            uint64_t block_length;
            if (!memory_->ReadULEB128(&block_length)) {
              last_error_.code = DWARF_ERROR_MEMORY_INVALID;
              last_error_.address = memory_->cur_offset();
              return false;
            }
            operands_.push_back(block_length);
            memory_->set_cur_offset(memory_->cur_offset() + block_length);
            continue;
          }
          uint64_t value;
          if (!memory_->ReadEncodedValue<AddressType>(cfa->operands[i], &value)) {
            last_error_.code = DWARF_ERROR_MEMORY_INVALID;
            last_error_.address = memory_->cur_offset();
            return false;
          }
          operands_.push_back(value);
        }

        if (!(this->*handle_func)(loc_regs)) {
          return false;
        }
        break;
      }
    }
  }
}

template <typename AddressType>
std::string DwarfCfa<AddressType>::GetOperandString(uint8_t operand, uint64_t value,
                                                    uint64_t* cur_pc) {
  std::string string;
  switch (operand) {
    case DwarfCfaInfo::DWARF_DISPLAY_REGISTER:
      string = " register(" + std::to_string(value) + ")";
      break;
    case DwarfCfaInfo::DWARF_DISPLAY_SIGNED_NUMBER:
      string += " " + std::to_string(static_cast<SignedType>(value));
      break;
    case DwarfCfaInfo::DWARF_DISPLAY_ADVANCE_LOC:
      *cur_pc += value;
      FALLTHROUGH_INTENDED;
      // Fall through to log the value.
    case DwarfCfaInfo::DWARF_DISPLAY_NUMBER:
      string += " " + std::to_string(value);
      break;
    case DwarfCfaInfo::DWARF_DISPLAY_SET_LOC:
      *cur_pc = value;
      FALLTHROUGH_INTENDED;
      // Fall through to log the value.
    case DwarfCfaInfo::DWARF_DISPLAY_ADDRESS:
      if (std::is_same<AddressType, uint32_t>::value) {
        string += android::base::StringPrintf(" 0x%" PRIx32, static_cast<uint32_t>(value));
      } else {
        string += android::base::StringPrintf(" 0x%" PRIx64, static_cast<uint64_t>(value));
      }
      break;
    default:
      string = " unknown";
  }
  return string;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::LogOffsetRegisterString(uint32_t indent, uint64_t cfa_offset,
                                                    uint8_t reg) {
  uint64_t offset;
  if (!memory_->ReadULEB128(&offset)) {
    return false;
  }
  uint64_t end_offset = memory_->cur_offset();
  memory_->set_cur_offset(cfa_offset);

  std::string raw_data = "Raw Data:";
  for (uint64_t i = cfa_offset; i < end_offset; i++) {
    uint8_t value;
    if (!memory_->ReadBytes(&value, 1)) {
      return false;
    }
    raw_data += android::base::StringPrintf(" 0x%02x", value);
  }
  Log::Info(indent, "DW_CFA_offset register(%d) %" PRId64, reg, offset);
  Log::Info(indent, "%s", raw_data.c_str());
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::LogInstruction(uint32_t indent, uint64_t cfa_offset, uint8_t op,
                                           uint64_t* cur_pc) {
  const auto* cfa = &DwarfCfaInfo::kTable[op];
  if (cfa->name[0] == '\0' || (arch_ != ARCH_ARM64 && op == 0x2d)) {
    if (op == 0x2d) {
      Log::Info(indent, "Illegal (Only valid on aarch64)");
    } else {
      Log::Info(indent, "Illegal");
    }
    Log::Info(indent, "Raw Data: 0x%02x", op);
    return true;
  }

  std::string log_string(cfa->name);
  std::vector<std::string> expression_lines;
  for (size_t i = 0; i < cfa->num_operands; i++) {
    if (cfa->operands[i] == DW_EH_PE_block) {
      // This is a Dwarf Expression.
      uint64_t end_offset;
      if (!memory_->ReadULEB128(&end_offset)) {
        return false;
      }
      log_string += " " + std::to_string(end_offset);
      end_offset += memory_->cur_offset();

      DwarfOp<AddressType> op(memory_, nullptr);
      op.GetLogInfo(memory_->cur_offset(), end_offset, &expression_lines);
      memory_->set_cur_offset(end_offset);
    } else {
      uint64_t value;
      if (!memory_->ReadEncodedValue<AddressType>(cfa->operands[i], &value)) {
        return false;
      }
      log_string += GetOperandString(cfa->display_operands[i], value, cur_pc);
    }
  }
  Log::Info(indent, "%s", log_string.c_str());

  // Get the raw bytes of the data.
  uint64_t end_offset = memory_->cur_offset();
  memory_->set_cur_offset(cfa_offset);
  std::string raw_data("Raw Data:");
  for (uint64_t i = 0; i < end_offset - cfa_offset; i++) {
    uint8_t value;
    if (!memory_->ReadBytes(&value, 1)) {
      return false;
    }

    // Only show 10 raw bytes per line.
    if ((i % 10) == 0 && i != 0) {
      Log::Info(indent, "%s", raw_data.c_str());
      raw_data.clear();
    }
    if (raw_data.empty()) {
      raw_data = "Raw Data:";
    }
    raw_data += android::base::StringPrintf(" 0x%02x", value);
  }
  if (!raw_data.empty()) {
    Log::Info(indent, "%s", raw_data.c_str());
  }

  // Log any of the expression data.
  for (const auto& line : expression_lines) {
    Log::Info(indent + 1, "%s", line.c_str());
  }
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::Log(uint32_t indent, uint64_t pc, uint64_t start_offset,
                                uint64_t end_offset) {
  memory_->set_cur_offset(start_offset);
  uint64_t cfa_offset;
  uint64_t cur_pc = fde_->pc_start;
  uint64_t old_pc = cur_pc;
  while ((cfa_offset = memory_->cur_offset()) < end_offset && cur_pc <= pc) {
    // Read the cfa information.
    uint8_t cfa_value;
    if (!memory_->ReadBytes(&cfa_value, 1)) {
      return false;
    }

    // Check the 2 high bits.
    uint8_t cfa_low = cfa_value & 0x3f;
    switch (cfa_value >> 6) {
      case 0:
        if (!LogInstruction(indent, cfa_offset, cfa_low, &cur_pc)) {
          return false;
        }
        break;
      case 1:
        Log::Info(indent, "DW_CFA_advance_loc %d", cfa_low);
        Log::Info(indent, "Raw Data: 0x%02x", cfa_value);
        cur_pc += cfa_low * fde_->cie->code_alignment_factor;
        break;
      case 2:
        if (!LogOffsetRegisterString(indent, cfa_offset, cfa_low)) {
          return false;
        }
        break;
      case 3:
        Log::Info(indent, "DW_CFA_restore register(%d)", cfa_low);
        Log::Info(indent, "Raw Data: 0x%02x", cfa_value);
        break;
    }
    if (cur_pc != old_pc) {
      // This forces a newline or empty log line.
      Log::Info("");
      Log::Info(indent, "PC 0x%" PRIx64, cur_pc);
    }
    old_pc = cur_pc;
  }
  return true;
}

// Static data.
template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_nop(DwarfLocations*) {
  return true;
}

template <>
bool DwarfCfa<uint32_t>::cfa_set_loc(DwarfLocations*) {
  uint32_t cur_pc = cur_pc_;
  uint32_t new_pc = operands_[0];
  if (new_pc < cur_pc) {
    Log::Info("Warning: PC is moving backwards: old 0x%" PRIx32 " new 0x%" PRIx32, cur_pc, new_pc);
  }
  cur_pc_ = new_pc;
  return true;
}

template <>
bool DwarfCfa<uint64_t>::cfa_set_loc(DwarfLocations*) {
  uint64_t cur_pc = cur_pc_;
  uint64_t new_pc = operands_[0];
  if (new_pc < cur_pc) {
    Log::Info("Warning: PC is moving backwards: old 0x%" PRIx64 " new 0x%" PRIx64, cur_pc, new_pc);
  }
  cur_pc_ = new_pc;
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_advance_loc(DwarfLocations*) {
  cur_pc_ += operands_[0] * fde_->cie->code_alignment_factor;
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_offset(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  (*loc_regs)[reg] = {.type = DWARF_LOCATION_OFFSET, .values = {operands_[1]}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_restore(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  if (cie_loc_regs_ == nullptr) {
    Log::Error("Invalid: restore while processing cie.");
    last_error_.code = DWARF_ERROR_ILLEGAL_STATE;
    return false;
  }
  auto reg_entry = cie_loc_regs_->find(reg);
  if (reg_entry == cie_loc_regs_->end()) {
    loc_regs->erase(reg);
  } else {
    (*loc_regs)[reg] = reg_entry->second;
  }
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_undefined(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  (*loc_regs)[reg] = {.type = DWARF_LOCATION_UNDEFINED};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_same_value(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  loc_regs->erase(reg);
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_register(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  AddressType reg_dst = operands_[1];
  (*loc_regs)[reg] = {.type = DWARF_LOCATION_REGISTER, .values = {reg_dst}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_remember_state(DwarfLocations* loc_regs) {
  loc_reg_state_.push(*loc_regs);
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_restore_state(DwarfLocations* loc_regs) {
  if (loc_reg_state_.size() == 0) {
    Log::Info("Warning: Attempt to restore without remember.");
    return true;
  }
  *loc_regs = loc_reg_state_.top();
  loc_reg_state_.pop();
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_def_cfa(DwarfLocations* loc_regs) {
  (*loc_regs)[CFA_REG] = {.type = DWARF_LOCATION_REGISTER, .values = {operands_[0], operands_[1]}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_def_cfa_register(DwarfLocations* loc_regs) {
  auto cfa_location = loc_regs->find(CFA_REG);
  if (cfa_location == loc_regs->end() || cfa_location->second.type != DWARF_LOCATION_REGISTER) {
    Log::Error("Attempt to set new register, but cfa is not already set to a register.");
    last_error_.code = DWARF_ERROR_ILLEGAL_STATE;
    return false;
  }

  cfa_location->second.values[0] = operands_[0];
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_def_cfa_offset(DwarfLocations* loc_regs) {
  // Changing the offset if this is not a register is illegal.
  auto cfa_location = loc_regs->find(CFA_REG);
  if (cfa_location == loc_regs->end() || cfa_location->second.type != DWARF_LOCATION_REGISTER) {
    Log::Error("Attempt to set offset, but cfa is not set to a register.");
    last_error_.code = DWARF_ERROR_ILLEGAL_STATE;
    return false;
  }
  cfa_location->second.values[1] = operands_[0];
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_def_cfa_expression(DwarfLocations* loc_regs) {
  // There is only one type of expression for CFA evaluation and the DWARF
  // specification is unclear whether it returns the address or the
  // dereferenced value. GDB expects the value, so will we.
  (*loc_regs)[CFA_REG] = {.type = DWARF_LOCATION_VAL_EXPRESSION,
                          .values = {operands_[0], memory_->cur_offset()}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_expression(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  (*loc_regs)[reg] = {.type = DWARF_LOCATION_EXPRESSION,
                      .values = {operands_[1], memory_->cur_offset()}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_offset_extended_sf(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  SignedType value = static_cast<SignedType>(operands_[1]) * fde_->cie->data_alignment_factor;
  (*loc_regs)[reg] = {.type = DWARF_LOCATION_OFFSET, .values = {static_cast<uint64_t>(value)}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_def_cfa_sf(DwarfLocations* loc_regs) {
  SignedType offset = static_cast<SignedType>(operands_[1]) * fde_->cie->data_alignment_factor;
  (*loc_regs)[CFA_REG] = {.type = DWARF_LOCATION_REGISTER,
                          .values = {operands_[0], static_cast<uint64_t>(offset)}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_def_cfa_offset_sf(DwarfLocations* loc_regs) {
  // Changing the offset if this is not a register is illegal.
  auto cfa_location = loc_regs->find(CFA_REG);
  if (cfa_location == loc_regs->end() || cfa_location->second.type != DWARF_LOCATION_REGISTER) {
    Log::Error("Attempt to set offset, but cfa is not set to a register.");
    last_error_.code = DWARF_ERROR_ILLEGAL_STATE;
    return false;
  }
  SignedType offset = static_cast<SignedType>(operands_[0]) * fde_->cie->data_alignment_factor;
  cfa_location->second.values[1] = static_cast<uint64_t>(offset);
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_val_offset(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  SignedType offset = static_cast<SignedType>(operands_[1]) * fde_->cie->data_alignment_factor;
  (*loc_regs)[reg] = {.type = DWARF_LOCATION_VAL_OFFSET, .values = {static_cast<uint64_t>(offset)}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_val_offset_sf(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  SignedType offset = static_cast<SignedType>(operands_[1]) * fde_->cie->data_alignment_factor;
  (*loc_regs)[reg] = {.type = DWARF_LOCATION_VAL_OFFSET, .values = {static_cast<uint64_t>(offset)}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_val_expression(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  (*loc_regs)[reg] = {.type = DWARF_LOCATION_VAL_EXPRESSION,
                      .values = {operands_[1], memory_->cur_offset()}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_gnu_negative_offset_extended(DwarfLocations* loc_regs) {
  AddressType reg = operands_[0];
  SignedType offset = -static_cast<SignedType>(operands_[1]);
  (*loc_regs)[reg] = {.type = DWARF_LOCATION_OFFSET, .values = {static_cast<uint64_t>(offset)}};
  return true;
}

template <typename AddressType>
bool DwarfCfa<AddressType>::cfa_aarch64_negate_ra_state(DwarfLocations* loc_regs) {
  // Only supported on aarch64.
  if (arch_ != ARCH_ARM64) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }

  auto cfa_location = loc_regs->find(Arm64Reg::ARM64_PREG_RA_SIGN_STATE);
  if (cfa_location == loc_regs->end()) {
    (*loc_regs)[Arm64Reg::ARM64_PREG_RA_SIGN_STATE] = {.type = DWARF_LOCATION_PSEUDO_REGISTER,
                                                       .values = {1}};
  } else {
    cfa_location->second.values[0] ^= 1;
  }
  return true;
}

const DwarfCfaInfo::Info DwarfCfaInfo::kTable[64] = {
    {
        // 0x00 DW_CFA_nop
        "DW_CFA_nop",
        2,
        0,
        {},
        {},
    },
    {
        "DW_CFA_set_loc",  // 0x01 DW_CFA_set_loc
        2,
        1,
        {DW_EH_PE_absptr},
        {DWARF_DISPLAY_SET_LOC},
    },
    {
        "DW_CFA_advance_loc1",  // 0x02 DW_CFA_advance_loc1
        2,
        1,
        {DW_EH_PE_udata1},
        {DWARF_DISPLAY_ADVANCE_LOC},
    },
    {
        "DW_CFA_advance_loc2",  // 0x03 DW_CFA_advance_loc2
        2,
        1,
        {DW_EH_PE_udata2},
        {DWARF_DISPLAY_ADVANCE_LOC},
    },
    {
        "DW_CFA_advance_loc4",  // 0x04 DW_CFA_advance_loc4
        2,
        1,
        {DW_EH_PE_udata4},
        {DWARF_DISPLAY_ADVANCE_LOC},
    },
    {
        "DW_CFA_offset_extended",  // 0x05 DW_CFA_offset_extended
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_uleb128},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_NUMBER},
    },
    {
        "DW_CFA_restore_extended",  // 0x06 DW_CFA_restore_extended
        2,
        1,
        {DW_EH_PE_uleb128},
        {DWARF_DISPLAY_REGISTER},
    },
    {
        "DW_CFA_undefined",  // 0x07 DW_CFA_undefined
        2,
        1,
        {DW_EH_PE_uleb128},
        {DWARF_DISPLAY_REGISTER},
    },
    {
        "DW_CFA_same_value",  // 0x08 DW_CFA_same_value
        2,
        1,
        {DW_EH_PE_uleb128},
        {DWARF_DISPLAY_REGISTER},
    },
    {
        "DW_CFA_register",  // 0x09 DW_CFA_register
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_uleb128},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_REGISTER},
    },
    {
        "DW_CFA_remember_state",  // 0x0a DW_CFA_remember_state
        2,
        0,
        {},
        {},
    },
    {
        "DW_CFA_restore_state",  // 0x0b DW_CFA_restore_state
        2,
        0,
        {},
        {},
    },
    {
        "DW_CFA_def_cfa",  // 0x0c DW_CFA_def_cfa
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_uleb128},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_NUMBER},
    },
    {
        "DW_CFA_def_cfa_register",  // 0x0d DW_CFA_def_cfa_register
        2,
        1,
        {DW_EH_PE_uleb128},
        {DWARF_DISPLAY_REGISTER},
    },
    {
        "DW_CFA_def_cfa_offset",  // 0x0e DW_CFA_def_cfa_offset
        2,
        1,
        {DW_EH_PE_uleb128},
        {DWARF_DISPLAY_NUMBER},
    },
    {
        "DW_CFA_def_cfa_expression",  // 0x0f DW_CFA_def_cfa_expression
        2,
        1,
        {DW_EH_PE_block},
        {DWARF_DISPLAY_EVAL_BLOCK},
    },
    {
        "DW_CFA_expression",  // 0x10 DW_CFA_expression
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_block},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_EVAL_BLOCK},
    },
    {
        "DW_CFA_offset_extended_sf",  // 0x11 DW_CFA_offset_extend_sf
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_sleb128},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_SIGNED_NUMBER},
    },
    {
        "DW_CFA_def_cfa_sf",  // 0x12 DW_CFA_def_cfa_sf
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_sleb128},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_SIGNED_NUMBER},
    },
    {
        "DW_CFA_def_cfa_offset_sf",  // 0x13 DW_CFA_def_cfa_offset_sf
        2,
        1,
        {DW_EH_PE_sleb128},
        {DWARF_DISPLAY_SIGNED_NUMBER},
    },
    {
        "DW_CFA_val_offset",  // 0x14 DW_CFA_val_offset
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_uleb128},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_NUMBER},
    },
    {
        "DW_CFA_val_offset_sf",  // 0x15 DW_CFA_val_offset_sf
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_sleb128},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_SIGNED_NUMBER},
    },
    {
        "DW_CFA_val_expression",  // 0x16 DW_CFA_val_expression
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_block},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_EVAL_BLOCK},
    },
    {"", 0, 0, {}, {}},  // 0x17 illegal cfa
    {"", 0, 0, {}, {}},  // 0x18 illegal cfa
    {"", 0, 0, {}, {}},  // 0x19 illegal cfa
    {"", 0, 0, {}, {}},  // 0x1a illegal cfa
    {"", 0, 0, {}, {}},  // 0x1b illegal cfa
    {"", 0, 0, {}, {}},  // 0x1c DW_CFA_lo_user (Treat as illegal)
    {"", 0, 0, {}, {}},  // 0x1d illegal cfa
    {"", 0, 0, {}, {}},  // 0x1e illegal cfa
    {"", 0, 0, {}, {}},  // 0x1f illegal cfa
    {"", 0, 0, {}, {}},  // 0x20 illegal cfa
    {"", 0, 0, {}, {}},  // 0x21 illegal cfa
    {"", 0, 0, {}, {}},  // 0x22 illegal cfa
    {"", 0, 0, {}, {}},  // 0x23 illegal cfa
    {"", 0, 0, {}, {}},  // 0x24 illegal cfa
    {"", 0, 0, {}, {}},  // 0x25 illegal cfa
    {"", 0, 0, {}, {}},  // 0x26 illegal cfa
    {"", 0, 0, {}, {}},  // 0x27 illegal cfa
    {"", 0, 0, {}, {}},  // 0x28 illegal cfa
    {"", 0, 0, {}, {}},  // 0x29 illegal cfa
    {"", 0, 0, {}, {}},  // 0x2a illegal cfa
    {"", 0, 0, {}, {}},  // 0x2b illegal cfa
    {"", 0, 0, {}, {}},  // 0x2c illegal cfa
    {
        "DW_CFA_AARCH64_negate_ra_state",  // 0x2d DW_CFA_AARCH64_negate_ra_state
        3,
        0,
        {},
        {},
    },
    {
        "DW_CFA_GNU_args_size",  // 0x2e DW_CFA_GNU_args_size
        2,
        1,
        {DW_EH_PE_uleb128},
        {DWARF_DISPLAY_NUMBER},
    },
    {
        "DW_CFA_GNU_negative_offset_extended",  // 0x2f DW_CFA_GNU_negative_offset_extended
        2,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_uleb128},
        {DWARF_DISPLAY_REGISTER, DWARF_DISPLAY_NUMBER},
    },
    {"", 0, 0, {}, {}},  // 0x31 illegal cfa
    {"", 0, 0, {}, {}},  // 0x32 illegal cfa
    {"", 0, 0, {}, {}},  // 0x33 illegal cfa
    {"", 0, 0, {}, {}},  // 0x34 illegal cfa
    {"", 0, 0, {}, {}},  // 0x35 illegal cfa
    {"", 0, 0, {}, {}},  // 0x36 illegal cfa
    {"", 0, 0, {}, {}},  // 0x37 illegal cfa
    {"", 0, 0, {}, {}},  // 0x38 illegal cfa
    {"", 0, 0, {}, {}},  // 0x39 illegal cfa
    {"", 0, 0, {}, {}},  // 0x3a illegal cfa
    {"", 0, 0, {}, {}},  // 0x3b illegal cfa
    {"", 0, 0, {}, {}},  // 0x3c illegal cfa
    {"", 0, 0, {}, {}},  // 0x3d illegal cfa
    {"", 0, 0, {}, {}},  // 0x3e illegal cfa
    {"", 0, 0, {}, {}},  // 0x3f DW_CFA_hi_user (Treat as illegal)
};

// Explicitly instantiate DwarfCfa.
template class DwarfCfa<uint32_t>;
template class DwarfCfa<uint64_t>;

}  // namespace unwindstack
