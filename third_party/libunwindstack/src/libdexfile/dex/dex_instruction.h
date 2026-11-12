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

#ifndef ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_H_
#define ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_H_

#include <android-base/logging.h>

#include "base/globals.h"
#include "base/macros.h"

using uint4_t = uint8_t;
using int4_t = int8_t;

namespace art {

class DexFile;

enum {
  kNumPackedOpcodes = 0x100
};

class Instruction {
 public:
  // NOP-encoded switch-statement signatures.
  enum Signatures {
    kPackedSwitchSignature = 0x0100,
    kSparseSwitchSignature = 0x0200,
    kArrayDataSignature = 0x0300,
  };

  struct PACKED(4) PackedSwitchPayload {
    const uint16_t ident;
    const uint16_t case_count;
    const int32_t first_key;
    const int32_t targets[];

   private:
    DISALLOW_COPY_AND_ASSIGN(PackedSwitchPayload);
  };

  struct PACKED(4) SparseSwitchPayload {
    const uint16_t ident;
    const uint16_t case_count;
    const int32_t keys_and_targets[];

   public:
    const int32_t* GetKeys() const {
      return keys_and_targets;
    }

    const int32_t* GetTargets() const {
      return keys_and_targets + case_count;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(SparseSwitchPayload);
  };

  struct PACKED(4) ArrayDataPayload {
    const uint16_t ident;
    const uint16_t element_width;
    const uint32_t element_count;
    const uint8_t data[];

   private:
    DISALLOW_COPY_AND_ASSIGN(ArrayDataPayload);
  };

  enum Code {  // private marker to avoid generate-operator-out.py from processing.
#define INSTRUCTION_ENUM(opcode, cname, p, f, i, a, e, v) cname = (opcode),
#include "dex_instruction_list.h"
    DEX_INSTRUCTION_LIST(INSTRUCTION_ENUM)
#undef DEX_INSTRUCTION_LIST
#undef INSTRUCTION_ENUM
    RSUB_INT_LIT16 = RSUB_INT,
  };

  enum Format : uint8_t {
    k10x,  // op
    k12x,  // op vA, vB
    k11n,  // op vA, #+B
    k11x,  // op vAA
    k10t,  // op +AA
    k20t,  // op +AAAA
    k22x,  // op vAA, vBBBB
    k21t,  // op vAA, +BBBB
    k21s,  // op vAA, #+BBBB
    k21h,  // op vAA, #+BBBB00000[00000000]
    k21c,  // op vAA, thing@BBBB
    k23x,  // op vAA, vBB, vCC
    k22b,  // op vAA, vBB, #+CC
    k22t,  // op vA, vB, +CCCC
    k22s,  // op vA, vB, #+CCCC
    k22c,  // op vA, vB, thing@CCCC
    k32x,  // op vAAAA, vBBBB
    k30t,  // op +AAAAAAAA
    k31t,  // op vAA, +BBBBBBBB
    k31i,  // op vAA, #+BBBBBBBB
    k31c,  // op vAA, thing@BBBBBBBB
    k35c,  // op {vC, vD, vE, vF, vG}, thing@BBBB (B: count, A: vG)
    k3rc,  // op {vCCCC .. v(CCCC+AA-1)}, meth@BBBB

    // op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH (A: count)
    // format: AG op BBBB FEDC HHHH
    k45cc,

    // op {VCCCC .. v(CCCC+AA-1)}, meth@BBBB, proto@HHHH (AA: count)
    // format: AA op BBBB CCCC HHHH
    k4rcc,  // op {VCCCC .. v(CCCC+AA-1)}, meth@BBBB, proto@HHHH (AA: count)

    k51l,  // op vAA, #+BBBBBBBBBBBBBBBB
    kInvalidFormat,
  };

  enum IndexType : uint8_t {
    kIndexUnknown = 0,
    kIndexNone,               // has no index
    kIndexTypeRef,            // type reference index
    kIndexStringRef,          // string reference index
    kIndexMethodRef,          // method reference index
    kIndexFieldRef,           // field reference index
    kIndexFieldOffset,        // field offset (for static linked fields)
    kIndexVtableOffset,       // vtable offset (for static linked methods)
    kIndexMethodAndProtoRef,  // method and a proto reference index (for invoke-polymorphic)
    kIndexCallSiteRef,        // call site reference index
    kIndexMethodHandleRef,    // constant method handle reference index
    kIndexProtoRef,           // prototype reference index
  };

  enum Flags : uint8_t {  // private marker to avoid generate-operator-out.py from processing.
    kBranch              = 0x01,  // conditional or unconditional branch
    kContinue            = 0x02,  // flow can continue to next statement
    kSwitch              = 0x04,  // switch statement
    kThrow               = 0x08,  // could cause an exception to be thrown
    kReturn              = 0x10,  // returns, no additional statements
    kInvoke              = 0x20,  // a flavor of invoke
    kUnconditional       = 0x40,  // unconditional branch
    kExperimental        = 0x80,  // is an experimental opcode
  };

  // Old flags. Keeping them around in case we might need them again some day.
  enum ExtendedFlags : uint32_t {
    kAdd                 = 0x0000080,  // addition
    kSubtract            = 0x0000100,  // subtract
    kMultiply            = 0x0000200,  // multiply
    kDivide              = 0x0000400,  // division
    kRemainder           = 0x0000800,  // remainder
    kAnd                 = 0x0001000,  // and
    kOr                  = 0x0002000,  // or
    kXor                 = 0x0004000,  // xor
    kShl                 = 0x0008000,  // shl
    kShr                 = 0x0010000,  // shr
    kUshr                = 0x0020000,  // ushr
    kCast                = 0x0040000,  // cast
    kStore               = 0x0080000,  // store opcode
    kLoad                = 0x0100000,  // load opcode
    kClobber             = 0x0200000,  // clobbers memory in a big way (not just a write)
    kRegCFieldOrConstant = 0x0400000,  // is the third virtual register a field or literal constant (vC)
    kRegBFieldOrConstant = 0x0800000,  // is the second virtual register a field or literal constant (vB)
  };

  enum VerifyFlag : uint32_t {  // private marker to avoid generate-operator-out.py from processing.
    kVerifyNothing            = 0x0000000,
    kVerifyRegA               = 0x0000001,
    kVerifyRegAWide           = 0x0000002,
    kVerifyRegB               = 0x0000004,
    kVerifyRegBField          = 0x0000008,
    kVerifyRegBMethod         = 0x0000010,
    kVerifyRegBNewInstance    = 0x0000020,
    kVerifyRegBString         = 0x0000040,
    kVerifyRegBType           = 0x0000080,
    kVerifyRegBWide           = 0x0000100,
    kVerifyRegC               = 0x0000200,
    kVerifyRegCField          = 0x0000400,
    kVerifyRegCNewArray       = 0x0000800,
    kVerifyRegCType           = 0x0001000,
    kVerifyRegCWide           = 0x0002000,
    kVerifyArrayData          = 0x0004000,
    kVerifyBranchTarget       = 0x0008000,
    kVerifySwitchTargets      = 0x0010000,
    kVerifyVarArg             = 0x0020000,
    kVerifyVarArgNonZero      = 0x0040000,
    kVerifyVarArgRange        = 0x0080000,
    kVerifyVarArgRangeNonZero = 0x0100000,
    kVerifyRuntimeOnly        = 0x0200000,
    kVerifyError              = 0x0400000,
    kVerifyRegHPrototype      = 0x0800000,
    kVerifyRegBCallSite       = 0x1000000,
    kVerifyRegBMethodHandle   = 0x2000000,
    kVerifyRegBPrototype      = 0x4000000,
  };

  // Collect the enums in a struct for better locality.
  struct InstructionDescriptor {
    uint32_t verify_flags;         // Set of VerifyFlag.
    Format format;
    IndexType index_type;
    uint8_t flags;                 // Set of Flags.
    int8_t size_in_code_units;
  };

  static constexpr uint32_t kMaxVarArgRegs = 5;

  static constexpr bool kHaveExperimentalInstructions = false;

  // Returns the size (in 2 byte code units) of this instruction.
  size_t SizeInCodeUnits() const {
    int8_t result = kInstructionDescriptors[Opcode()].size_in_code_units;
    if (UNLIKELY(result < 0)) {
      return SizeInCodeUnitsComplexOpcode();
    } else {
      return static_cast<size_t>(result);
    }
  }

  // Returns the size (in 2 byte code units) of the given instruction format.
  ALWAYS_INLINE static constexpr size_t SizeInCodeUnits(Format format);

  // Code units required to calculate the size of the instruction.
  size_t CodeUnitsRequiredForSizeComputation() const {
    const int8_t result = kInstructionDescriptors[Opcode()].size_in_code_units;
    return UNLIKELY(result < 0) ? CodeUnitsRequiredForSizeOfComplexOpcode() : 1;
  }

  // Reads an instruction out of the stream at the specified address.
  static const Instruction* At(const uint16_t* code) {
    DCHECK(code != nullptr);
    return reinterpret_cast<const Instruction*>(code);
  }

  // Reads an instruction out of the stream from the current address plus an offset.
  const Instruction* RelativeAt(int32_t offset) const WARN_UNUSED {
    return At(reinterpret_cast<const uint16_t*>(this) + offset);
  }

  // Returns a pointer to the next instruction in the stream.
  const Instruction* Next() const {
    return RelativeAt(SizeInCodeUnits());
  }

  // Returns a pointer to the instruction after this 1xx instruction in the stream.
  const Instruction* Next_1xx() const {
    DCHECK(FormatOf(Opcode()) >= k10x && FormatOf(Opcode()) <= k10t);
    return RelativeAt(1);
  }

  // Returns a pointer to the instruction after this 2xx instruction in the stream.
  const Instruction* Next_2xx() const {
    DCHECK(FormatOf(Opcode()) >= k20t && FormatOf(Opcode()) <= k22c);
    return RelativeAt(2);
  }

  // Returns a pointer to the instruction after this 3xx instruction in the stream.
  const Instruction* Next_3xx() const {
    DCHECK(FormatOf(Opcode()) >= k32x && FormatOf(Opcode()) <= k3rc);
    return RelativeAt(3);
  }

  // Returns a pointer to the instruction after this 4xx instruction in the stream.
  const Instruction* Next_4xx() const {
    DCHECK(FormatOf(Opcode()) >= k45cc && FormatOf(Opcode()) <= k4rcc);
    return RelativeAt(4);
  }

  // Returns a pointer to the instruction after this 51l instruction in the stream.
  const Instruction* Next_51l() const {
    DCHECK(FormatOf(Opcode()) == k51l);
    return RelativeAt(5);
  }

  // Returns the name of this instruction's opcode.
  const char* Name() const {
    return Instruction::Name(Opcode());
  }

  // Returns the name of the given opcode.
  static const char* Name(Code opcode) {
    return kInstructionNames[opcode];
  }

  // VRegA
  bool HasVRegA() const;
  ALWAYS_INLINE int32_t VRegA() const;
  ALWAYS_INLINE int32_t VRegA(Format format, uint16_t inst_data) const;

  int8_t VRegA_10t() const {
    return VRegA_10t(Fetch16(0));
  }
  uint8_t VRegA_10x() const {
    return VRegA_10x(Fetch16(0));
  }
  uint4_t VRegA_11n() const {
    return VRegA_11n(Fetch16(0));
  }
  uint8_t VRegA_11x() const {
    return VRegA_11x(Fetch16(0));
  }
  uint4_t VRegA_12x() const {
    return VRegA_12x(Fetch16(0));
  }
  int16_t VRegA_20t() const;
  uint8_t VRegA_21c() const {
    return VRegA_21c(Fetch16(0));
  }
  uint8_t VRegA_21h() const {
    return VRegA_21h(Fetch16(0));
  }
  uint8_t VRegA_21s() const {
    return VRegA_21s(Fetch16(0));
  }
  uint8_t VRegA_21t() const {
    return VRegA_21t(Fetch16(0));
  }
  uint8_t VRegA_22b() const {
    return VRegA_22b(Fetch16(0));
  }
  uint4_t VRegA_22c() const {
    return VRegA_22c(Fetch16(0));
  }
  uint4_t VRegA_22s() const {
    return VRegA_22s(Fetch16(0));
  }
  uint4_t VRegA_22t() const {
    return VRegA_22t(Fetch16(0));
  }
  uint8_t VRegA_22x() const {
    return VRegA_22x(Fetch16(0));
  }
  uint8_t VRegA_23x() const {
    return VRegA_23x(Fetch16(0));
  }
  int32_t VRegA_30t() const;
  uint8_t VRegA_31c() const {
    return VRegA_31c(Fetch16(0));
  }
  uint8_t VRegA_31i() const {
    return VRegA_31i(Fetch16(0));
  }
  uint8_t VRegA_31t() const {
    return VRegA_31t(Fetch16(0));
  }
  uint16_t VRegA_32x() const;
  uint4_t VRegA_35c() const {
    return VRegA_35c(Fetch16(0));
  }
  uint8_t VRegA_3rc() const {
    return VRegA_3rc(Fetch16(0));
  }
  uint8_t VRegA_51l() const {
    return VRegA_51l(Fetch16(0));
  }
  uint4_t VRegA_45cc() const {
    return VRegA_45cc(Fetch16(0));
  }
  uint8_t VRegA_4rcc() const {
    return VRegA_4rcc(Fetch16(0));
  }

  // The following methods return the vA operand for various instruction formats. The "inst_data"
  // parameter holds the first 16 bits of instruction which the returned value is decoded from.
  int8_t VRegA_10t(uint16_t inst_data) const;
  uint8_t VRegA_10x(uint16_t inst_data) const;
  uint4_t VRegA_11n(uint16_t inst_data) const;
  uint8_t VRegA_11x(uint16_t inst_data) const;
  uint4_t VRegA_12x(uint16_t inst_data) const;
  uint8_t VRegA_21c(uint16_t inst_data) const;
  uint8_t VRegA_21h(uint16_t inst_data) const;
  uint8_t VRegA_21s(uint16_t inst_data) const;
  uint8_t VRegA_21t(uint16_t inst_data) const;
  uint8_t VRegA_22b(uint16_t inst_data) const;
  uint4_t VRegA_22c(uint16_t inst_data) const;
  uint4_t VRegA_22s(uint16_t inst_data) const;
  uint4_t VRegA_22t(uint16_t inst_data) const;
  uint8_t VRegA_22x(uint16_t inst_data) const;
  uint8_t VRegA_23x(uint16_t inst_data) const;
  uint8_t VRegA_31c(uint16_t inst_data) const;
  uint8_t VRegA_31i(uint16_t inst_data) const;
  uint8_t VRegA_31t(uint16_t inst_data) const;
  uint4_t VRegA_35c(uint16_t inst_data) const;
  uint8_t VRegA_3rc(uint16_t inst_data) const;
  uint8_t VRegA_51l(uint16_t inst_data) const;
  uint4_t VRegA_45cc(uint16_t inst_data) const;
  uint8_t VRegA_4rcc(uint16_t inst_data) const;

  // VRegB
  bool HasVRegB() const;
  ALWAYS_INLINE int32_t VRegB() const;
  ALWAYS_INLINE int32_t VRegB(Format format, uint16_t inst_data) const;

  bool HasWideVRegB() const;
  uint64_t WideVRegB() const;

  int4_t VRegB_11n() const {
    return VRegB_11n(Fetch16(0));
  }
  uint4_t VRegB_12x() const {
    return VRegB_12x(Fetch16(0));
  }
  uint16_t VRegB_21c() const;
  uint16_t VRegB_21h() const;
  int16_t VRegB_21s() const;
  int16_t VRegB_21t() const;
  uint8_t VRegB_22b() const;
  uint4_t VRegB_22c() const {
    return VRegB_22c(Fetch16(0));
  }
  uint4_t VRegB_22s() const {
    return VRegB_22s(Fetch16(0));
  }
  uint4_t VRegB_22t() const {
    return VRegB_22t(Fetch16(0));
  }
  uint16_t VRegB_22x() const;
  uint8_t VRegB_23x() const;
  uint32_t VRegB_31c() const;
  int32_t VRegB_31i() const;
  int32_t VRegB_31t() const;
  uint16_t VRegB_32x() const;
  uint16_t VRegB_35c() const;
  uint16_t VRegB_3rc() const;
  uint64_t VRegB_51l() const;  // vB_wide
  uint16_t VRegB_45cc() const;
  uint16_t VRegB_4rcc() const;

  // The following methods return the vB operand for all instruction formats where it is encoded in
  // the first 16 bits of instruction. The "inst_data" parameter holds these 16 bits. The returned
  // value is decoded from it.
  int4_t VRegB_11n(uint16_t inst_data) const;
  uint4_t VRegB_12x(uint16_t inst_data) const;
  uint4_t VRegB_22c(uint16_t inst_data) const;
  uint4_t VRegB_22s(uint16_t inst_data) const;
  uint4_t VRegB_22t(uint16_t inst_data) const;

  // VRegC
  bool HasVRegC() const;
  ALWAYS_INLINE int32_t VRegC() const;
  ALWAYS_INLINE int32_t VRegC(Format format) const;

  int8_t VRegC_22b() const;
  uint16_t VRegC_22c() const;
  int16_t VRegC_22s() const;
  int16_t VRegC_22t() const;
  uint8_t VRegC_23x() const;
  uint4_t VRegC_35c() const;
  uint16_t VRegC_3rc() const;
  uint4_t VRegC_45cc() const;
  uint16_t VRegC_4rcc() const;


  // VRegH
  bool HasVRegH() const;
  int32_t VRegH() const;
  uint16_t VRegH_45cc() const;
  uint16_t VRegH_4rcc() const;

  // Fills the given array with the 'arg' array of the instruction.
  bool HasVarArgs() const;
  uint32_t GetVarArgs(uint32_t args[kMaxVarArgRegs], uint16_t inst_data) const;
  uint32_t GetVarArgs(uint32_t args[kMaxVarArgRegs]) const {
    return GetVarArgs(args, Fetch16(0));
  }

  // Returns the opcode field of the instruction. The given "inst_data" parameter must be the first
  // 16 bits of instruction.
  Code Opcode(uint16_t inst_data) const {
    DCHECK_EQ(inst_data, Fetch16(0));
    return static_cast<Code>(inst_data & 0xFF);
  }

  // Returns the opcode field of the instruction from the first 16 bits of instruction.
  Code Opcode() const {
    return Opcode(Fetch16(0));
  }

  void SetOpcode(Code opcode) {
    DCHECK_LT(static_cast<uint16_t>(opcode), 256u);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[0] = (insns[0] & 0xff00) | static_cast<uint16_t>(opcode);
  }

  void SetVRegA_10x(uint8_t val) {
    DCHECK(FormatOf(Opcode()) == k10x);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[0] = (val << 8) | (insns[0] & 0x00ff);
  }

  void SetVRegB_3rc(uint16_t val) {
    DCHECK(FormatOf(Opcode()) == k3rc);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[1] = val;
  }

  void SetVRegB_35c(uint16_t val) {
    DCHECK(FormatOf(Opcode()) == k35c);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[1] = val;
  }

  void SetVRegC_22c(uint16_t val) {
    DCHECK(FormatOf(Opcode()) == k22c);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[1] = val;
  }

  void SetVRegA_21c(uint8_t val) {
    DCHECK(FormatOf(Opcode()) == k21c);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[0] = (val << 8) | (insns[0] & 0x00ff);
  }

  void SetVRegB_21c(uint16_t val) {
    DCHECK(FormatOf(Opcode()) == k21c);
    uint16_t* insns = reinterpret_cast<uint16_t*>(this);
    insns[1] = val;
  }

  // Returns the format of the given opcode.
  static Format FormatOf(Code opcode) {
    return kInstructionDescriptors[opcode].format;
  }

  // Returns the index type of the given opcode.
  static IndexType IndexTypeOf(Code opcode) {
    return kInstructionDescriptors[opcode].index_type;
  }

  // Returns the flags for the given opcode.
  static uint8_t FlagsOf(Code opcode) {
    return kInstructionDescriptors[opcode].flags;
  }

  // Return the verify flags for the given opcode.
  static uint32_t VerifyFlagsOf(Code opcode) {
    return kInstructionDescriptors[opcode].verify_flags;
  }

  // Returns true if this instruction is a branch.
  bool IsBranch() const {
    return (kInstructionDescriptors[Opcode()].flags & kBranch) != 0;
  }

  // Returns true if this instruction is a unconditional branch.
  bool IsUnconditional() const {
    return (kInstructionDescriptors[Opcode()].flags & kUnconditional) != 0;
  }

  // Returns the branch offset if this instruction is a branch.
  int32_t GetTargetOffset() const;

  // Returns true if the instruction allows control flow to go to the following instruction.
  bool CanFlowThrough() const;

  // Returns true if the instruction is a quickened instruction.
  bool IsQuickened() const {
    return (kInstructionDescriptors[Opcode()].index_type == kIndexFieldOffset) ||
        (kInstructionDescriptors[Opcode()].index_type == kIndexVtableOffset);
  }

  // Returns true if this instruction is a switch.
  bool IsSwitch() const {
    return (kInstructionDescriptors[Opcode()].flags & kSwitch) != 0;
  }

  // Returns true if this instruction can throw.
  bool IsThrow() const {
    return (kInstructionDescriptors[Opcode()].flags & kThrow) != 0;
  }

  // Determine if the instruction is any of 'return' instructions.
  bool IsReturn() const {
    return (kInstructionDescriptors[Opcode()].flags & kReturn) != 0;
  }

  // Determine if this instruction ends execution of its basic block.
  bool IsBasicBlockEnd() const {
    return IsBranch() || IsReturn() || Opcode() == THROW;
  }

  // Determine if this instruction is an invoke.
  bool IsInvoke() const {
    return (kInstructionDescriptors[Opcode()].flags & kInvoke) != 0;
  }

  // Determine if this instruction is experimental.
  bool IsExperimental() const {
    return (kInstructionDescriptors[Opcode()].flags & kExperimental) != 0;
  }

  int GetVerifyTypeArgumentA() const {
    return (kInstructionDescriptors[Opcode()].verify_flags & (kVerifyRegA | kVerifyRegAWide));
  }

  int GetVerifyTypeArgumentB() const {
    return (kInstructionDescriptors[Opcode()].verify_flags & (kVerifyRegB | kVerifyRegBField |
        kVerifyRegBMethod | kVerifyRegBNewInstance | kVerifyRegBString | kVerifyRegBType |
        kVerifyRegBWide));
  }

  int GetVerifyTypeArgumentC() const {
    return (kInstructionDescriptors[Opcode()].verify_flags & (kVerifyRegC | kVerifyRegCField |
        kVerifyRegCNewArray | kVerifyRegCType | kVerifyRegCWide));
  }

  int GetVerifyTypeArgumentH() const {
    return (kInstructionDescriptors[Opcode()].verify_flags & kVerifyRegHPrototype);
  }

  int GetVerifyExtraFlags() const {
    return (kInstructionDescriptors[Opcode()].verify_flags & (kVerifyArrayData |
        kVerifyBranchTarget | kVerifySwitchTargets | kVerifyVarArg | kVerifyVarArgNonZero |
        kVerifyVarArgRange | kVerifyVarArgRangeNonZero | kVerifyError));
  }

  bool GetVerifyIsRuntimeOnly() const {
    return (kInstructionDescriptors[Opcode()].verify_flags & kVerifyRuntimeOnly) != 0;
  }

  // Get the dex PC of this instruction as a offset in code units from the beginning of insns.
  uint32_t GetDexPc(const uint16_t* insns) const {
    return (reinterpret_cast<const uint16_t*>(this) - insns);
  }

  // Dump decoded version of instruction
  std::string DumpString(const DexFile*) const;

  // Dump code_units worth of this instruction, padding to code_units for shorter instructions
  std::string DumpHex(size_t code_units) const;

  // Little-endian dump code_units worth of this instruction, padding to code_units for
  // shorter instructions
  std::string DumpHexLE(size_t instr_code_units) const;

  uint16_t Fetch16(size_t offset) const {
    const uint16_t* insns = reinterpret_cast<const uint16_t*>(this);
    return insns[offset];
  }

 private:
  size_t SizeInCodeUnitsComplexOpcode() const;

  // Return how many code unit words are required to compute the size of the opcode.
  size_t CodeUnitsRequiredForSizeOfComplexOpcode() const;

  uint32_t Fetch32(size_t offset) const {
    return (Fetch16(offset) | ((uint32_t) Fetch16(offset + 1) << 16));
  }

  uint4_t InstA() const {
    return InstA(Fetch16(0));
  }

  uint4_t InstB() const {
    return InstB(Fetch16(0));
  }

  uint8_t InstAA() const {
    return InstAA(Fetch16(0));
  }

  uint4_t InstA(uint16_t inst_data) const {
    DCHECK_EQ(inst_data, Fetch16(0));
    return static_cast<uint4_t>((inst_data >> 8) & 0x0f);
  }

  uint4_t InstB(uint16_t inst_data) const {
    DCHECK_EQ(inst_data, Fetch16(0));
    return static_cast<uint4_t>(inst_data >> 12);
  }

  uint8_t InstAA(uint16_t inst_data) const {
    DCHECK_EQ(inst_data, Fetch16(0));
    return static_cast<uint8_t>(inst_data >> 8);
  }

  static const char* const kInstructionNames[];

  static const InstructionDescriptor kInstructionDescriptors[];

  DISALLOW_IMPLICIT_CONSTRUCTORS(Instruction);
};
std::ostream& operator<<(std::ostream& os, Instruction::Code code);
std::ostream& operator<<(std::ostream& os, Instruction::Format format);

// Base class for accessing instruction operands. Unifies operand
// access for instructions that have range and varargs forms
// (e.g. invoke-polymoprhic/range and invoke-polymorphic).
class InstructionOperands {
 public:
  explicit InstructionOperands(size_t num_operands) : num_operands_(num_operands) {}
  virtual ~InstructionOperands() {}
  virtual uint32_t GetOperand(size_t index) const = 0;
  size_t GetNumberOfOperands() const { return num_operands_; }

 private:
  const size_t num_operands_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(InstructionOperands);
};

// Class for accessing operands for instructions with a range format
// (e.g. 3rc and 4rcc).
class RangeInstructionOperands final : public InstructionOperands {
 public:
  RangeInstructionOperands(uint32_t first_operand, size_t num_operands)
      : InstructionOperands(num_operands), first_operand_(first_operand) {}
  ~RangeInstructionOperands() {}
  uint32_t GetOperand(size_t operand_index) const override;

 private:
  const uint32_t first_operand_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RangeInstructionOperands);
};

// Class for accessing operands for instructions with a variable
// number of arguments format (e.g. 35c and 45cc).
class VarArgsInstructionOperands final : public InstructionOperands {
 public:
  VarArgsInstructionOperands(const uint32_t (&operands)[Instruction::kMaxVarArgRegs],
                             size_t num_operands)
      : InstructionOperands(num_operands), operands_(operands) {}
  ~VarArgsInstructionOperands() {}
  uint32_t GetOperand(size_t operand_index) const override;

 private:
  const uint32_t (&operands_)[Instruction::kMaxVarArgRegs];

  DISALLOW_IMPLICIT_CONSTRUCTORS(VarArgsInstructionOperands);
};

// Class for accessing operands without the receiver by wrapping an
// existing InstructionOperands instance.
class NoReceiverInstructionOperands final : public InstructionOperands {
 public:
  explicit NoReceiverInstructionOperands(const InstructionOperands* const inner)
      : InstructionOperands(inner->GetNumberOfOperands() - 1), inner_(inner) {}
  ~NoReceiverInstructionOperands() {}
  uint32_t GetOperand(size_t operand_index) const override;

 private:
  const InstructionOperands* const inner_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(NoReceiverInstructionOperands);
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_H_
