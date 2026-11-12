/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "dex_instruction-inl.h"

#include <inttypes.h>

#include <iomanip>
#include <sstream>

#include "android-base/stringprintf.h"

#include "dex_file-inl.h"
#include "utf.h"

namespace art {

using android::base::StringPrintf;

const char* const Instruction::kInstructionNames[] = {
#define INSTRUCTION_NAME(o, c, pname, f, i, a, e, v) pname,
#include "dex_instruction_list.h"
  DEX_INSTRUCTION_LIST(INSTRUCTION_NAME)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_NAME
};

static_assert(sizeof(Instruction::InstructionDescriptor) == 8u, "Unexpected descriptor size");

static constexpr int8_t InstructionSizeInCodeUnitsByOpcode(Instruction::Code opcode,
                                                           Instruction::Format format) {
  if (opcode == Instruction::Code::NOP) {
    return -1;
  } else if ((format >= Instruction::Format::k10x) && (format <= Instruction::Format::k10t)) {
    return 1;
  } else if ((format >= Instruction::Format::k20t) && (format <= Instruction::Format::k22c)) {
    return 2;
  } else if ((format >= Instruction::Format::k32x) && (format <= Instruction::Format::k3rc)) {
    return 3;
  } else if ((format >= Instruction::Format::k45cc) && (format <= Instruction::Format::k4rcc)) {
    return 4;
  } else if (format == Instruction::Format::k51l) {
    return 5;
  } else {
    return -1;
  }
}

Instruction::InstructionDescriptor const Instruction::kInstructionDescriptors[] = {
#define INSTRUCTION_DESCR(opcode, c, p, format, index, flags, eflags, vflags) \
    { vflags, \
      format, \
      index, \
      flags, \
      InstructionSizeInCodeUnitsByOpcode((c), (format)), \
    },
#include "dex_instruction_list.h"
  DEX_INSTRUCTION_LIST(INSTRUCTION_DESCR)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_DESCR
};

int32_t Instruction::GetTargetOffset() const {
  switch (FormatOf(Opcode())) {
    // Cases for conditional branches follow.
    case k22t: return VRegC_22t();
    case k21t: return VRegB_21t();
    // Cases for unconditional branches follow.
    case k10t: return VRegA_10t();
    case k20t: return VRegA_20t();
    case k30t: return VRegA_30t();
    default: LOG(FATAL) << "Tried to access the branch offset of an instruction " << Name() <<
        " which does not have a target operand.";
  }
  UNREACHABLE();
}

bool Instruction::CanFlowThrough() const {
  const uint16_t* insns = reinterpret_cast<const uint16_t*>(this);
  uint16_t insn = *insns;
  Code opcode = static_cast<Code>(insn & 0xFF);
  return  FlagsOf(opcode) & Instruction::kContinue;
}

size_t Instruction::SizeInCodeUnitsComplexOpcode() const {
  const uint16_t* insns = reinterpret_cast<const uint16_t*>(this);
  // Handle special NOP encoded variable length sequences.
  switch (*insns) {
    case kPackedSwitchSignature:
      return (4 + insns[1] * 2);
    case kSparseSwitchSignature:
      return (2 + insns[1] * 4);
    case kArrayDataSignature: {
      uint16_t element_size = insns[1];
      uint32_t length = insns[2] | (((uint32_t)insns[3]) << 16);
      // The plus 1 is to round up for odd size and width.
      return (4 + (element_size * length + 1) / 2);
    }
    default:
      if ((*insns & 0xFF) == 0) {
        return 1;  // NOP.
      } else {
        LOG(FATAL) << "Unreachable: " << DumpString(nullptr);
        UNREACHABLE();
      }
  }
}

size_t Instruction::CodeUnitsRequiredForSizeOfComplexOpcode() const {
  const uint16_t* insns = reinterpret_cast<const uint16_t*>(this);
  // Handle special NOP encoded variable length sequences.
  switch (*insns) {
    case kPackedSwitchSignature:
      FALLTHROUGH_INTENDED;
    case kSparseSwitchSignature:
      return 2;
    case kArrayDataSignature:
      return 4;
    default:
      if ((*insns & 0xFF) == 0) {
        return 1;  // NOP.
      } else {
        LOG(FATAL) << "Unreachable: " << DumpString(nullptr);
        UNREACHABLE();
      }
  }
}

std::string Instruction::DumpHex(size_t code_units) const {
  size_t inst_length = SizeInCodeUnits();
  if (inst_length > code_units) {
    inst_length = code_units;
  }
  std::ostringstream os;
  const uint16_t* insn = reinterpret_cast<const uint16_t*>(this);
  for (size_t i = 0; i < inst_length; i++) {
    os << StringPrintf("0x%04x", insn[i]) << " ";
  }
  for (size_t i = inst_length; i < code_units; i++) {
    os << "       ";
  }
  return os.str();
}

std::string Instruction::DumpHexLE(size_t instr_code_units) const {
  size_t inst_length = SizeInCodeUnits();
  if (inst_length > instr_code_units) {
    inst_length = instr_code_units;
  }
  std::ostringstream os;
  const uint16_t* insn = reinterpret_cast<const uint16_t*>(this);
  for (size_t i = 0; i < inst_length; i++) {
    os << StringPrintf("%02x%02x", static_cast<uint8_t>(insn[i] & 0x00FF),
                       static_cast<uint8_t>((insn[i] & 0xFF00) >> 8)) << " ";
  }
  for (size_t i = inst_length; i < instr_code_units; i++) {
    os << "     ";
  }
  return os.str();
}

std::string Instruction::DumpString(const DexFile* file) const {
  std::ostringstream os;
  const char* opcode = kInstructionNames[Opcode()];
  switch (FormatOf(Opcode())) {
    case k10x:  os << opcode; break;
    case k12x:  os << StringPrintf("%s v%d, v%d", opcode, VRegA_12x(), VRegB_12x()); break;
    case k11n:  os << StringPrintf("%s v%d, #%+d", opcode, VRegA_11n(), VRegB_11n()); break;
    case k11x:  os << StringPrintf("%s v%d", opcode, VRegA_11x()); break;
    case k10t:  os << StringPrintf("%s %+d", opcode, VRegA_10t()); break;
    case k20t:  os << StringPrintf("%s %+d", opcode, VRegA_20t()); break;
    case k22x:  os << StringPrintf("%s v%d, v%d", opcode, VRegA_22x(), VRegB_22x()); break;
    case k21t:  os << StringPrintf("%s v%d, %+d", opcode, VRegA_21t(), VRegB_21t()); break;
    case k21s:  os << StringPrintf("%s v%d, #%+d", opcode, VRegA_21s(), VRegB_21s()); break;
    case k21h: {
        // op vAA, #+BBBB0000[00000000]
        if (Opcode() == CONST_HIGH16) {
          uint32_t value = VRegB_21h() << 16;
          os << StringPrintf("%s v%d, #int %+d // 0x%x", opcode, VRegA_21h(), value, value);
        } else {
          uint64_t value = static_cast<uint64_t>(VRegB_21h()) << 48;
          os << StringPrintf("%s v%d, #long %+" PRId64 " // 0x%" PRIx64, opcode, VRegA_21h(),
                             value, value);
        }
      }
      break;
    case k21c: {
      switch (Opcode()) {
        case CONST_STRING:
          if (file != nullptr) {
            uint32_t string_idx = VRegB_21c();
            if (string_idx < file->NumStringIds()) {
              os << StringPrintf(
                  "const-string v%d, %s // string@%d",
                  VRegA_21c(),
                  PrintableString(file->StringDataByIdx(dex::StringIndex(string_idx))).c_str(),
                  string_idx);
            } else {
              os << StringPrintf("const-string v%d, <<invalid-string-idx-%d>> // string@%d",
                                 VRegA_21c(),
                                 string_idx,
                                 string_idx);
            }
            break;
          }
          FALLTHROUGH_INTENDED;
        case CHECK_CAST:
        case CONST_CLASS:
        case NEW_INSTANCE:
          if (file != nullptr) {
            dex::TypeIndex type_idx(VRegB_21c());
            os << opcode << " v" << static_cast<int>(VRegA_21c()) << ", "
               << file->PrettyType(type_idx) << " // type@" << type_idx;
            break;
          }
          FALLTHROUGH_INTENDED;
        case SGET:
        case SGET_WIDE:
        case SGET_OBJECT:
        case SGET_BOOLEAN:
        case SGET_BYTE:
        case SGET_CHAR:
        case SGET_SHORT:
          if (file != nullptr) {
            uint32_t field_idx = VRegB_21c();
            os << opcode << "  v" << static_cast<int>(VRegA_21c()) << ", " << file->PrettyField(field_idx, true)
               << " // field@" << field_idx;
            break;
          }
          FALLTHROUGH_INTENDED;
        case SPUT:
        case SPUT_WIDE:
        case SPUT_OBJECT:
        case SPUT_BOOLEAN:
        case SPUT_BYTE:
        case SPUT_CHAR:
        case SPUT_SHORT:
          if (file != nullptr) {
            uint32_t field_idx = VRegB_21c();
            os << opcode << " v" << static_cast<int>(VRegA_21c()) << ", " << file->PrettyField(field_idx, true)
               << " // field@" << field_idx;
            break;
          }
          FALLTHROUGH_INTENDED;
        default:
          os << StringPrintf("%s v%d, thing@%d", opcode, VRegA_21c(), VRegB_21c());
          break;
      }
      break;
    }
    case k23x:  os << StringPrintf("%s v%d, v%d, v%d", opcode, VRegA_23x(), VRegB_23x(), VRegC_23x()); break;
    case k22b:  os << StringPrintf("%s v%d, v%d, #%+d", opcode, VRegA_22b(), VRegB_22b(), VRegC_22b()); break;
    case k22t:  os << StringPrintf("%s v%d, v%d, %+d", opcode, VRegA_22t(), VRegB_22t(), VRegC_22t()); break;
    case k22s:  os << StringPrintf("%s v%d, v%d, #%+d", opcode, VRegA_22s(), VRegB_22s(), VRegC_22s()); break;
    case k22c: {
      switch (Opcode()) {
        case IGET:
        case IGET_WIDE:
        case IGET_OBJECT:
        case IGET_BOOLEAN:
        case IGET_BYTE:
        case IGET_CHAR:
        case IGET_SHORT:
          if (file != nullptr) {
            uint32_t field_idx = VRegC_22c();
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v" << static_cast<int>(VRegB_22c()) << ", "
               << file->PrettyField(field_idx, true) << " // field@" << field_idx;
            break;
          }
          FALLTHROUGH_INTENDED;
        case IPUT:
        case IPUT_WIDE:
        case IPUT_OBJECT:
        case IPUT_BOOLEAN:
        case IPUT_BYTE:
        case IPUT_CHAR:
        case IPUT_SHORT:
          if (file != nullptr) {
            uint32_t field_idx = VRegC_22c();
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v" << static_cast<int>(VRegB_22c()) << ", "
               << file->PrettyField(field_idx, true) << " // field@" << field_idx;
            break;
          }
          FALLTHROUGH_INTENDED;
        case INSTANCE_OF:
          if (file != nullptr) {
            dex::TypeIndex type_idx(VRegC_22c());
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v"
               << static_cast<int>(VRegB_22c()) << ", " << file->PrettyType(type_idx)
               << " // type@" << type_idx.index_;
            break;
          }
          FALLTHROUGH_INTENDED;
        case NEW_ARRAY:
          if (file != nullptr) {
            dex::TypeIndex type_idx(VRegC_22c());
            os << opcode << " v" << static_cast<int>(VRegA_22c()) << ", v"
               << static_cast<int>(VRegB_22c()) << ", " << file->PrettyType(type_idx)
               << " // type@" << type_idx.index_;
            break;
          }
          FALLTHROUGH_INTENDED;
        default:
          os << StringPrintf("%s v%d, v%d, thing@%d", opcode, VRegA_22c(), VRegB_22c(), VRegC_22c());
          break;
      }
      break;
    }
    case k32x:  os << StringPrintf("%s v%d, v%d", opcode, VRegA_32x(), VRegB_32x()); break;
    case k30t:  os << StringPrintf("%s %+d", opcode, VRegA_30t()); break;
    case k31t:  os << StringPrintf("%s v%d, %+d", opcode, VRegA_31t(), VRegB_31t()); break;
    case k31i:  os << StringPrintf("%s v%d, #%+d", opcode, VRegA_31i(), VRegB_31i()); break;
    case k31c:
      if (Opcode() == CONST_STRING_JUMBO) {
        uint32_t string_idx = VRegB_31c();
        if (file != nullptr) {
          if (string_idx < file->NumStringIds()) {
            os << StringPrintf(
                "%s v%d, %s // string@%d",
                opcode,
                VRegA_31c(),
                PrintableString(file->StringDataByIdx(dex::StringIndex(string_idx))).c_str(),
                string_idx);
          } else {
            os << StringPrintf("%s v%d, <<invalid-string-idx-%d>> // string@%d",
                               opcode,
                               VRegA_31c(),
                               string_idx,
                               string_idx);
          }
        } else {
          os << StringPrintf("%s v%d, string@%d", opcode, VRegA_31c(), string_idx);
        }
      } else {
        os << StringPrintf("%s v%d, thing@%d", opcode, VRegA_31c(), VRegB_31c()); break;
      }
      break;
    case k35c: {
      uint32_t arg[kMaxVarArgRegs];
      GetVarArgs(arg);
      auto DumpArgs = [&](size_t count) {
        for (size_t i = 0; i < count; ++i) {
          if (i != 0) {
            os << ", ";
          }
          os << "v" << arg[i];
        }
      };
      switch (Opcode()) {
        case FILLED_NEW_ARRAY:
        {
          os << opcode << " {";
          DumpArgs(VRegA_35c());
          os << "}, type@" << VRegB_35c();
        }
        break;

        case INVOKE_VIRTUAL:
        case INVOKE_SUPER:
        case INVOKE_DIRECT:
        case INVOKE_STATIC:
        case INVOKE_INTERFACE:
          if (file != nullptr) {
            os << opcode << " {";
            uint32_t method_idx = VRegB_35c();
            DumpArgs(VRegA_35c());
            os << "}, " << file->PrettyMethod(method_idx) << " // method@" << method_idx;
            break;
          }
          FALLTHROUGH_INTENDED;
        case INVOKE_CUSTOM:
          if (file != nullptr) {
            os << opcode << " {";
            uint32_t call_site_idx = VRegB_35c();
            DumpArgs(VRegA_35c());
            os << "},  // call_site@" << call_site_idx;
            break;
          }
          FALLTHROUGH_INTENDED;
        default:
          os << opcode << " {";
          DumpArgs(VRegA_35c());
          os << "}, thing@" << VRegB_35c();
          break;
      }
      break;
    }
    case k3rc: {
      uint16_t first_reg = VRegC_3rc();
      uint16_t last_reg =  VRegC_3rc() + VRegA_3rc() - 1;
      switch (Opcode()) {
        case INVOKE_VIRTUAL_RANGE:
        case INVOKE_SUPER_RANGE:
        case INVOKE_DIRECT_RANGE:
        case INVOKE_STATIC_RANGE:
        case INVOKE_INTERFACE_RANGE:
          if (file != nullptr) {
            uint32_t method_idx = VRegB_3rc();
            os << StringPrintf("%s, {v%d .. v%d}, ", opcode, first_reg, last_reg)
               << file->PrettyMethod(method_idx) << " // method@" << method_idx;
            break;
          }
          FALLTHROUGH_INTENDED;
        case INVOKE_CUSTOM_RANGE:
          if (file != nullptr) {
            uint32_t call_site_idx = VRegB_3rc();
            os << StringPrintf("%s, {v%d .. v%d}, ", opcode, first_reg, last_reg)
               << "// call_site@" << call_site_idx;
            break;
          }
          FALLTHROUGH_INTENDED;
        default:
          os << StringPrintf("%s, {v%d .. v%d}, ", opcode, first_reg, last_reg)
             << "thing@" << VRegB_3rc();
          break;
      }
      break;
    }
    case k45cc: {
      uint32_t arg[kMaxVarArgRegs];
      GetVarArgs(arg);
      uint16_t method_idx = VRegB_45cc();
      dex::ProtoIndex proto_idx(VRegH_45cc());
      os << opcode << " {";
      for (uint32_t i = 0; i < VRegA_45cc(); ++i) {
        if (i != 0) {
          os << ", ";
        }
        os << "v" << arg[i];
      }
      os << "}";
      if (file != nullptr) {
        os << ", " << file->PrettyMethod(method_idx)
           << ", " << file->GetShorty(proto_idx)
           << " // ";
      } else {
        os << ", ";
      }
      os << "method@" << method_idx << ", proto@" << proto_idx;
      break;
    }
    case k4rcc:
      switch (Opcode()) {
        case INVOKE_POLYMORPHIC_RANGE: {
          if (file != nullptr) {
            uint16_t method_idx = VRegB_4rcc();
            dex::ProtoIndex proto_idx(VRegH_4rcc());
            os << opcode << ", {v" << VRegC_4rcc() << " .. v" << (VRegC_4rcc() + VRegA_4rcc())
               << "}, " << file->PrettyMethod(method_idx)
               << ", " << file->GetShorty(dex::ProtoIndex(proto_idx))
               << " // method@" << method_idx << ", proto@" << proto_idx;
            break;
          }
        }
        FALLTHROUGH_INTENDED;
        default: {
          uint16_t method_idx = VRegB_4rcc();
          dex::ProtoIndex proto_idx(VRegH_4rcc());
          os << opcode << ", {v" << VRegC_4rcc() << " .. v" << (VRegC_4rcc() + VRegA_4rcc())
             << "}, method@" << method_idx << ", proto@" << proto_idx;
        }
      }
      break;
    case k51l: os << StringPrintf("%s v%d, #%+" PRId64, opcode, VRegA_51l(), VRegB_51l()); break;
    case kInvalidFormat: os << "<invalid-opcode-format>";
  }
  return os.str();
}

// Add some checks that ensure the flags make sense. We need a subclass to be in the context of
// Instruction. Otherwise the flags from the instruction list don't work.
struct InstructionStaticAsserts : private Instruction {
  #define IMPLIES(a, b) (!(a) || (b))

  #define VAR_ARGS_CHECK(o, c, pname, f, i, a, e, v) \
    static_assert(IMPLIES((f) == k35c || (f) == k45cc, \
                          ((v) & (kVerifyVarArg | kVerifyVarArgNonZero)) != 0), \
                  "Missing var-arg verification");
  #include "dex_instruction_list.h"
    DEX_INSTRUCTION_LIST(VAR_ARGS_CHECK)
  #undef DEX_INSTRUCTION_LIST
  #undef VAR_ARGS_CHECK

  #define VAR_ARGS_RANGE_CHECK(o, c, pname, f, i, a, e, v) \
    static_assert(IMPLIES((f) == k3rc || (f) == k4rcc, \
                          ((v) & (kVerifyVarArgRange | kVerifyVarArgRangeNonZero)) != 0), \
                  "Missing var-arg verification");
  #include "dex_instruction_list.h"
    DEX_INSTRUCTION_LIST(VAR_ARGS_RANGE_CHECK)
  #undef DEX_INSTRUCTION_LIST
  #undef VAR_ARGS_RANGE_CHECK

  #define EXPERIMENTAL_CHECK(o, c, pname, f, i, a, e, v) \
    static_assert(kHaveExperimentalInstructions || (((a) & kExperimental) == 0), \
                  "Unexpected experimental instruction.");
    #include "dex_instruction_list.h"
  DEX_INSTRUCTION_LIST(EXPERIMENTAL_CHECK)
  #undef DEX_INSTRUCTION_LIST
  #undef EXPERIMENTAL_CHECK
};

std::ostream& operator<<(std::ostream& os, Instruction::Code code) {
  return os << Instruction::Name(code);
}

uint32_t RangeInstructionOperands::GetOperand(size_t operand_index) const {
  DCHECK_LT(operand_index, GetNumberOfOperands());
  return first_operand_ + operand_index;
}

uint32_t VarArgsInstructionOperands::GetOperand(size_t operand_index) const {
  DCHECK_LT(operand_index, GetNumberOfOperands());
  return operands_[operand_index];
}

uint32_t NoReceiverInstructionOperands::GetOperand(size_t operand_index) const {
  DCHECK_LT(GetNumberOfOperands(), inner_->GetNumberOfOperands());
  // The receiver is the first operand and since we're skipping it, we need to
  // add 1 to the operand_index.
  return inner_->GetOperand(operand_index + 1);
}

}  // namespace art
