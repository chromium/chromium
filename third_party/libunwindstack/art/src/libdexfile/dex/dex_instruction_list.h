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

#ifndef ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_LIST_H_
#define ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_LIST_H_

// V(opcode, instruction_code, name, format, index, flags, extended_flags, verifier_flags);
#define DEX_INSTRUCTION_LIST(V) \
  V(0x00, NOP, "nop", k10x, kIndexNone, kContinue, 0, kVerifyNothing) \
  V(0x01, MOVE, "move", k12x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB) \
  V(0x02, MOVE_FROM16, "move/from16", k22x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB) \
  V(0x03, MOVE_16, "move/16", k32x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB) \
  V(0x04, MOVE_WIDE, "move-wide", k12x, kIndexNone, kContinue, 0, kVerifyRegAWide | kVerifyRegBWide) \
  V(0x05, MOVE_WIDE_FROM16, "move-wide/from16", k22x, kIndexNone, kContinue, 0, kVerifyRegAWide | kVerifyRegBWide) \
  V(0x06, MOVE_WIDE_16, "move-wide/16", k32x, kIndexNone, kContinue, 0, kVerifyRegAWide | kVerifyRegBWide) \
  V(0x07, MOVE_OBJECT, "move-object", k12x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB) \
  V(0x08, MOVE_OBJECT_FROM16, "move-object/from16", k22x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB) \
  V(0x09, MOVE_OBJECT_16, "move-object/16", k32x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB) \
  V(0x0A, MOVE_RESULT, "move-result", k11x, kIndexNone, kContinue, 0, kVerifyRegA) \
  V(0x0B, MOVE_RESULT_WIDE, "move-result-wide", k11x, kIndexNone, kContinue, 0, kVerifyRegAWide) \
  V(0x0C, MOVE_RESULT_OBJECT, "move-result-object", k11x, kIndexNone, kContinue, 0, kVerifyRegA) \
  V(0x0D, MOVE_EXCEPTION, "move-exception", k11x, kIndexNone, kContinue, 0, kVerifyRegA) \
  V(0x0E, RETURN_VOID, "return-void", k10x, kIndexNone, kReturn, 0, kVerifyNothing) \
  V(0x0F, RETURN, "return", k11x, kIndexNone, kReturn, 0, kVerifyRegA) \
  V(0x10, RETURN_WIDE, "return-wide", k11x, kIndexNone, kReturn, 0, kVerifyRegAWide) \
  V(0x11, RETURN_OBJECT, "return-object", k11x, kIndexNone, kReturn, 0, kVerifyRegA) \
  V(0x12, CONST_4, "const/4", k11n, kIndexNone, kContinue, kRegBFieldOrConstant, kVerifyRegA) \
  V(0x13, CONST_16, "const/16", k21s, kIndexNone, kContinue, kRegBFieldOrConstant, kVerifyRegA) \
  V(0x14, CONST, "const", k31i, kIndexNone, kContinue, kRegBFieldOrConstant, kVerifyRegA) \
  V(0x15, CONST_HIGH16, "const/high16", k21h, kIndexNone, kContinue, kRegBFieldOrConstant, kVerifyRegA) \
  V(0x16, CONST_WIDE_16, "const-wide/16", k21s, kIndexNone, kContinue, kRegBFieldOrConstant, kVerifyRegAWide) \
  V(0x17, CONST_WIDE_32, "const-wide/32", k31i, kIndexNone, kContinue, kRegBFieldOrConstant, kVerifyRegAWide) \
  V(0x18, CONST_WIDE, "const-wide", k51l, kIndexNone, kContinue, kRegBFieldOrConstant, kVerifyRegAWide) \
  V(0x19, CONST_WIDE_HIGH16, "const-wide/high16", k21h, kIndexNone, kContinue, kRegBFieldOrConstant, kVerifyRegAWide) \
  V(0x1A, CONST_STRING, "const-string", k21c, kIndexStringRef, kContinue | kThrow, 0, kVerifyRegA | kVerifyRegBString) \
  V(0x1B, CONST_STRING_JUMBO, "const-string/jumbo", k31c, kIndexStringRef, kContinue | kThrow, 0, kVerifyRegA | kVerifyRegBString) \
  V(0x1C, CONST_CLASS, "const-class", k21c, kIndexTypeRef, kContinue | kThrow, 0, kVerifyRegA | kVerifyRegBType) \
  V(0x1D, MONITOR_ENTER, "monitor-enter", k11x, kIndexNone, kContinue | kThrow, kClobber, kVerifyRegA) \
  V(0x1E, MONITOR_EXIT, "monitor-exit", k11x, kIndexNone, kContinue | kThrow, kClobber, kVerifyRegA) \
  V(0x1F, CHECK_CAST, "check-cast", k21c, kIndexTypeRef, kContinue | kThrow, 0, kVerifyRegA | kVerifyRegBType) \
  V(0x20, INSTANCE_OF, "instance-of", k22c, kIndexTypeRef, kContinue | kThrow, 0, kVerifyRegA | kVerifyRegB | kVerifyRegCType) \
  V(0x21, ARRAY_LENGTH, "array-length", k12x, kIndexNone, kContinue | kThrow, 0, kVerifyRegA | kVerifyRegB) \
  V(0x22, NEW_INSTANCE, "new-instance", k21c, kIndexTypeRef, kContinue | kThrow, kClobber, kVerifyRegA | kVerifyRegBNewInstance) \
  V(0x23, NEW_ARRAY, "new-array", k22c, kIndexTypeRef, kContinue | kThrow, kClobber, kVerifyRegA | kVerifyRegB | kVerifyRegCNewArray) \
  V(0x24, FILLED_NEW_ARRAY, "filled-new-array", k35c, kIndexTypeRef, kContinue | kThrow, kClobber, kVerifyRegBType | kVerifyVarArg) \
  V(0x25, FILLED_NEW_ARRAY_RANGE, "filled-new-array/range", k3rc, kIndexTypeRef, kContinue | kThrow, kClobber, kVerifyRegBType | kVerifyVarArgRange) \
  V(0x26, FILL_ARRAY_DATA, "fill-array-data", k31t, kIndexNone, kContinue | kThrow, kClobber, kVerifyRegA | kVerifyArrayData) \
  V(0x27, THROW, "throw", k11x, kIndexNone, kThrow, 0, kVerifyRegA) \
  V(0x28, GOTO, "goto", k10t, kIndexNone, kBranch | kUnconditional, 0, kVerifyBranchTarget) \
  V(0x29, GOTO_16, "goto/16", k20t, kIndexNone, kBranch | kUnconditional, 0, kVerifyBranchTarget) \
  V(0x2A, GOTO_32, "goto/32", k30t, kIndexNone, kBranch | kUnconditional, 0, kVerifyBranchTarget) \
  V(0x2B, PACKED_SWITCH, "packed-switch", k31t, kIndexNone, kContinue | kSwitch, 0, kVerifyRegA | kVerifySwitchTargets) \
  V(0x2C, SPARSE_SWITCH, "sparse-switch", k31t, kIndexNone, kContinue | kSwitch, 0, kVerifyRegA | kVerifySwitchTargets) \
  V(0x2D, CMPL_FLOAT, "cmpl-float", k23x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x2E, CMPG_FLOAT, "cmpg-float", k23x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x2F, CMPL_DOUBLE, "cmpl-double", k23x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegBWide | kVerifyRegCWide) \
  V(0x30, CMPG_DOUBLE, "cmpg-double", k23x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegBWide | kVerifyRegCWide) \
  V(0x31, CMP_LONG, "cmp-long", k23x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegBWide | kVerifyRegCWide) \
  V(0x32, IF_EQ, "if-eq", k22t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyRegB | kVerifyBranchTarget) \
  V(0x33, IF_NE, "if-ne", k22t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyRegB | kVerifyBranchTarget) \
  V(0x34, IF_LT, "if-lt", k22t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyRegB | kVerifyBranchTarget) \
  V(0x35, IF_GE, "if-ge", k22t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyRegB | kVerifyBranchTarget) \
  V(0x36, IF_GT, "if-gt", k22t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyRegB | kVerifyBranchTarget) \
  V(0x37, IF_LE, "if-le", k22t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyRegB | kVerifyBranchTarget) \
  V(0x38, IF_EQZ, "if-eqz", k21t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyBranchTarget) \
  V(0x39, IF_NEZ, "if-nez", k21t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyBranchTarget) \
  V(0x3A, IF_LTZ, "if-ltz", k21t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyBranchTarget) \
  V(0x3B, IF_GEZ, "if-gez", k21t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyBranchTarget) \
  V(0x3C, IF_GTZ, "if-gtz", k21t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyBranchTarget) \
  V(0x3D, IF_LEZ, "if-lez", k21t, kIndexNone, kContinue | kBranch, 0, kVerifyRegA | kVerifyBranchTarget) \
  V(0x3E, UNUSED_3E, "unused-3e", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0x3F, UNUSED_3F, "unused-3f", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0x40, UNUSED_40, "unused-40", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0x41, UNUSED_41, "unused-41", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0x42, UNUSED_42, "unused-42", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0x43, UNUSED_43, "unused-43", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0x44, AGET, "aget", k23x, kIndexNone, kContinue | kThrow, kLoad, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x45, AGET_WIDE, "aget-wide", k23x, kIndexNone, kContinue | kThrow, kLoad, kVerifyRegAWide | kVerifyRegB | kVerifyRegC) \
  V(0x46, AGET_OBJECT, "aget-object", k23x, kIndexNone, kContinue | kThrow, kLoad, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x47, AGET_BOOLEAN, "aget-boolean", k23x, kIndexNone, kContinue | kThrow, kLoad, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x48, AGET_BYTE, "aget-byte", k23x, kIndexNone, kContinue | kThrow, kLoad, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x49, AGET_CHAR, "aget-char", k23x, kIndexNone, kContinue | kThrow, kLoad, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x4A, AGET_SHORT, "aget-short", k23x, kIndexNone, kContinue | kThrow, kLoad, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x4B, APUT, "aput", k23x, kIndexNone, kContinue | kThrow, kStore, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x4C, APUT_WIDE, "aput-wide", k23x, kIndexNone, kContinue | kThrow, kStore, kVerifyRegAWide | kVerifyRegB | kVerifyRegC) \
  V(0x4D, APUT_OBJECT, "aput-object", k23x, kIndexNone, kContinue | kThrow, kStore, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x4E, APUT_BOOLEAN, "aput-boolean", k23x, kIndexNone, kContinue | kThrow, kStore, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x4F, APUT_BYTE, "aput-byte", k23x, kIndexNone, kContinue | kThrow, kStore, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x50, APUT_CHAR, "aput-char", k23x, kIndexNone, kContinue | kThrow, kStore, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x51, APUT_SHORT, "aput-short", k23x, kIndexNone, kContinue | kThrow, kStore, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x52, IGET, "iget", k22c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x53, IGET_WIDE, "iget-wide", k22c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegCFieldOrConstant, kVerifyRegAWide | kVerifyRegB | kVerifyRegCField) \
  V(0x54, IGET_OBJECT, "iget-object", k22c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x55, IGET_BOOLEAN, "iget-boolean", k22c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x56, IGET_BYTE, "iget-byte", k22c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x57, IGET_CHAR, "iget-char", k22c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x58, IGET_SHORT, "iget-short", k22c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x59, IPUT, "iput", k22c, kIndexFieldRef, kContinue | kThrow, kStore | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x5A, IPUT_WIDE, "iput-wide", k22c, kIndexFieldRef, kContinue | kThrow, kStore | kRegCFieldOrConstant, kVerifyRegAWide | kVerifyRegB | kVerifyRegCField) \
  V(0x5B, IPUT_OBJECT, "iput-object", k22c, kIndexFieldRef, kContinue | kThrow, kStore | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x5C, IPUT_BOOLEAN, "iput-boolean", k22c, kIndexFieldRef, kContinue | kThrow, kStore | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x5D, IPUT_BYTE, "iput-byte", k22c, kIndexFieldRef, kContinue | kThrow, kStore | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x5E, IPUT_CHAR, "iput-char", k22c, kIndexFieldRef, kContinue | kThrow, kStore | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x5F, IPUT_SHORT, "iput-short", k22c, kIndexFieldRef, kContinue | kThrow, kStore | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB | kVerifyRegCField) \
  V(0x60, SGET, "sget", k21c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x61, SGET_WIDE, "sget-wide", k21c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegBFieldOrConstant, kVerifyRegAWide | kVerifyRegBField) \
  V(0x62, SGET_OBJECT, "sget-object", k21c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x63, SGET_BOOLEAN, "sget-boolean", k21c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x64, SGET_BYTE, "sget-byte", k21c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x65, SGET_CHAR, "sget-char", k21c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x66, SGET_SHORT, "sget-short", k21c, kIndexFieldRef, kContinue | kThrow, kLoad | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x67, SPUT, "sput", k21c, kIndexFieldRef, kContinue | kThrow, kStore | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x68, SPUT_WIDE, "sput-wide", k21c, kIndexFieldRef, kContinue | kThrow, kStore | kRegBFieldOrConstant, kVerifyRegAWide | kVerifyRegBField) \
  V(0x69, SPUT_OBJECT, "sput-object", k21c, kIndexFieldRef, kContinue | kThrow, kStore | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x6A, SPUT_BOOLEAN, "sput-boolean", k21c, kIndexFieldRef, kContinue | kThrow, kStore | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x6B, SPUT_BYTE, "sput-byte", k21c, kIndexFieldRef, kContinue | kThrow, kStore | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x6C, SPUT_CHAR, "sput-char", k21c, kIndexFieldRef, kContinue | kThrow, kStore | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x6D, SPUT_SHORT, "sput-short", k21c, kIndexFieldRef, kContinue | kThrow, kStore | kRegBFieldOrConstant, kVerifyRegA | kVerifyRegBField) \
  V(0x6E, INVOKE_VIRTUAL, "invoke-virtual", k35c, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgNonZero) \
  V(0x6F, INVOKE_SUPER, "invoke-super", k35c, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgNonZero) \
  V(0x70, INVOKE_DIRECT, "invoke-direct", k35c, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgNonZero) \
  V(0x71, INVOKE_STATIC, "invoke-static", k35c, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArg) \
  V(0x72, INVOKE_INTERFACE, "invoke-interface", k35c, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgNonZero) \
  V(0x73, UNUSED_73, "unused-73", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0x74, INVOKE_VIRTUAL_RANGE, "invoke-virtual/range", k3rc, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgRangeNonZero) \
  V(0x75, INVOKE_SUPER_RANGE, "invoke-super/range", k3rc, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgRangeNonZero) \
  V(0x76, INVOKE_DIRECT_RANGE, "invoke-direct/range", k3rc, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgRangeNonZero) \
  V(0x77, INVOKE_STATIC_RANGE, "invoke-static/range", k3rc, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgRange) \
  V(0x78, INVOKE_INTERFACE_RANGE, "invoke-interface/range", k3rc, kIndexMethodRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgRangeNonZero) \
  V(0x79, UNUSED_79, "unused-79", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0x7A, UNUSED_7A, "unused-7a", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0x7B, NEG_INT, "neg-int", k12x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB) \
  V(0x7C, NOT_INT, "not-int", k12x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB) \
  V(0x7D, NEG_LONG, "neg-long", k12x, kIndexNone, kContinue, 0, kVerifyRegAWide | kVerifyRegBWide) \
  V(0x7E, NOT_LONG, "not-long", k12x, kIndexNone, kContinue, 0, kVerifyRegAWide | kVerifyRegBWide) \
  V(0x7F, NEG_FLOAT, "neg-float", k12x, kIndexNone, kContinue, 0, kVerifyRegA | kVerifyRegB) \
  V(0x80, NEG_DOUBLE, "neg-double", k12x, kIndexNone, kContinue, 0, kVerifyRegAWide | kVerifyRegBWide) \
  V(0x81, INT_TO_LONG, "int-to-long", k12x, kIndexNone, kContinue, kCast, kVerifyRegAWide | kVerifyRegB) \
  V(0x82, INT_TO_FLOAT, "int-to-float", k12x, kIndexNone, kContinue, kCast, kVerifyRegA | kVerifyRegB) \
  V(0x83, INT_TO_DOUBLE, "int-to-double", k12x, kIndexNone, kContinue, kCast, kVerifyRegAWide | kVerifyRegB) \
  V(0x84, LONG_TO_INT, "long-to-int", k12x, kIndexNone, kContinue, kCast, kVerifyRegA | kVerifyRegBWide) \
  V(0x85, LONG_TO_FLOAT, "long-to-float", k12x, kIndexNone, kContinue, kCast, kVerifyRegA | kVerifyRegBWide) \
  V(0x86, LONG_TO_DOUBLE, "long-to-double", k12x, kIndexNone, kContinue, kCast, kVerifyRegAWide | kVerifyRegBWide) \
  V(0x87, FLOAT_TO_INT, "float-to-int", k12x, kIndexNone, kContinue, kCast, kVerifyRegA | kVerifyRegB) \
  V(0x88, FLOAT_TO_LONG, "float-to-long", k12x, kIndexNone, kContinue, kCast, kVerifyRegAWide | kVerifyRegB) \
  V(0x89, FLOAT_TO_DOUBLE, "float-to-double", k12x, kIndexNone, kContinue, kCast, kVerifyRegAWide | kVerifyRegB) \
  V(0x8A, DOUBLE_TO_INT, "double-to-int", k12x, kIndexNone, kContinue, kCast, kVerifyRegA | kVerifyRegBWide) \
  V(0x8B, DOUBLE_TO_LONG, "double-to-long", k12x, kIndexNone, kContinue, kCast, kVerifyRegAWide | kVerifyRegBWide) \
  V(0x8C, DOUBLE_TO_FLOAT, "double-to-float", k12x, kIndexNone, kContinue, kCast, kVerifyRegA | kVerifyRegBWide) \
  V(0x8D, INT_TO_BYTE, "int-to-byte", k12x, kIndexNone, kContinue, kCast, kVerifyRegA | kVerifyRegB) \
  V(0x8E, INT_TO_CHAR, "int-to-char", k12x, kIndexNone, kContinue, kCast, kVerifyRegA | kVerifyRegB) \
  V(0x8F, INT_TO_SHORT, "int-to-short", k12x, kIndexNone, kContinue, kCast, kVerifyRegA | kVerifyRegB) \
  V(0x90, ADD_INT, "add-int", k23x, kIndexNone, kContinue, kAdd, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x91, SUB_INT, "sub-int", k23x, kIndexNone, kContinue, kSubtract, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x92, MUL_INT, "mul-int", k23x, kIndexNone, kContinue, kMultiply, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x93, DIV_INT, "div-int", k23x, kIndexNone, kContinue | kThrow, kDivide, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x94, REM_INT, "rem-int", k23x, kIndexNone, kContinue | kThrow, kRemainder, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x95, AND_INT, "and-int", k23x, kIndexNone, kContinue, kAnd, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x96, OR_INT, "or-int", k23x, kIndexNone, kContinue, kOr, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x97, XOR_INT, "xor-int", k23x, kIndexNone, kContinue, kXor, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x98, SHL_INT, "shl-int", k23x, kIndexNone, kContinue, kShl, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x99, SHR_INT, "shr-int", k23x, kIndexNone, kContinue, kShr, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x9A, USHR_INT, "ushr-int", k23x, kIndexNone, kContinue, kUshr, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0x9B, ADD_LONG, "add-long", k23x, kIndexNone, kContinue, kAdd, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0x9C, SUB_LONG, "sub-long", k23x, kIndexNone, kContinue, kSubtract, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0x9D, MUL_LONG, "mul-long", k23x, kIndexNone, kContinue, kMultiply, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0x9E, DIV_LONG, "div-long", k23x, kIndexNone, kContinue | kThrow, kDivide, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0x9F, REM_LONG, "rem-long", k23x, kIndexNone, kContinue | kThrow, kRemainder, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0xA0, AND_LONG, "and-long", k23x, kIndexNone, kContinue, kAnd, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0xA1, OR_LONG, "or-long", k23x, kIndexNone, kContinue, kOr, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0xA2, XOR_LONG, "xor-long", k23x, kIndexNone, kContinue, kXor, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0xA3, SHL_LONG, "shl-long", k23x, kIndexNone, kContinue, kShl, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegC) \
  V(0xA4, SHR_LONG, "shr-long", k23x, kIndexNone, kContinue, kShr, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegC) \
  V(0xA5, USHR_LONG, "ushr-long", k23x, kIndexNone, kContinue, kUshr, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegC) \
  V(0xA6, ADD_FLOAT, "add-float", k23x, kIndexNone, kContinue, kAdd, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0xA7, SUB_FLOAT, "sub-float", k23x, kIndexNone, kContinue, kSubtract, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0xA8, MUL_FLOAT, "mul-float", k23x, kIndexNone, kContinue, kMultiply, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0xA9, DIV_FLOAT, "div-float", k23x, kIndexNone, kContinue, kDivide, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0xAA, REM_FLOAT, "rem-float", k23x, kIndexNone, kContinue, kRemainder, kVerifyRegA | kVerifyRegB | kVerifyRegC) \
  V(0xAB, ADD_DOUBLE, "add-double", k23x, kIndexNone, kContinue, kAdd, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0xAC, SUB_DOUBLE, "sub-double", k23x, kIndexNone, kContinue, kSubtract, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0xAD, MUL_DOUBLE, "mul-double", k23x, kIndexNone, kContinue, kMultiply, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0xAE, DIV_DOUBLE, "div-double", k23x, kIndexNone, kContinue, kDivide, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0xAF, REM_DOUBLE, "rem-double", k23x, kIndexNone, kContinue, kRemainder, kVerifyRegAWide | kVerifyRegBWide | kVerifyRegCWide) \
  V(0xB0, ADD_INT_2ADDR, "add-int/2addr", k12x, kIndexNone, kContinue, kAdd, kVerifyRegA | kVerifyRegB) \
  V(0xB1, SUB_INT_2ADDR, "sub-int/2addr", k12x, kIndexNone, kContinue, kSubtract, kVerifyRegA | kVerifyRegB) \
  V(0xB2, MUL_INT_2ADDR, "mul-int/2addr", k12x, kIndexNone, kContinue, kMultiply, kVerifyRegA | kVerifyRegB) \
  V(0xB3, DIV_INT_2ADDR, "div-int/2addr", k12x, kIndexNone, kContinue | kThrow, kDivide, kVerifyRegA | kVerifyRegB) \
  V(0xB4, REM_INT_2ADDR, "rem-int/2addr", k12x, kIndexNone, kContinue | kThrow, kRemainder, kVerifyRegA | kVerifyRegB) \
  V(0xB5, AND_INT_2ADDR, "and-int/2addr", k12x, kIndexNone, kContinue, kAnd, kVerifyRegA | kVerifyRegB) \
  V(0xB6, OR_INT_2ADDR, "or-int/2addr", k12x, kIndexNone, kContinue, kOr, kVerifyRegA | kVerifyRegB) \
  V(0xB7, XOR_INT_2ADDR, "xor-int/2addr", k12x, kIndexNone, kContinue, kXor, kVerifyRegA | kVerifyRegB) \
  V(0xB8, SHL_INT_2ADDR, "shl-int/2addr", k12x, kIndexNone, kContinue, kShl, kVerifyRegA | kVerifyRegB) \
  V(0xB9, SHR_INT_2ADDR, "shr-int/2addr", k12x, kIndexNone, kContinue, kShr, kVerifyRegA | kVerifyRegB) \
  V(0xBA, USHR_INT_2ADDR, "ushr-int/2addr", k12x, kIndexNone, kContinue, kUshr, kVerifyRegA | kVerifyRegB) \
  V(0xBB, ADD_LONG_2ADDR, "add-long/2addr", k12x, kIndexNone, kContinue, kAdd, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xBC, SUB_LONG_2ADDR, "sub-long/2addr", k12x, kIndexNone, kContinue, kSubtract, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xBD, MUL_LONG_2ADDR, "mul-long/2addr", k12x, kIndexNone, kContinue, kMultiply, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xBE, DIV_LONG_2ADDR, "div-long/2addr", k12x, kIndexNone, kContinue | kThrow, kDivide, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xBF, REM_LONG_2ADDR, "rem-long/2addr", k12x, kIndexNone, kContinue | kThrow, kRemainder, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xC0, AND_LONG_2ADDR, "and-long/2addr", k12x, kIndexNone, kContinue, kAnd, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xC1, OR_LONG_2ADDR, "or-long/2addr", k12x, kIndexNone, kContinue, kOr, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xC2, XOR_LONG_2ADDR, "xor-long/2addr", k12x, kIndexNone, kContinue, kXor, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xC3, SHL_LONG_2ADDR, "shl-long/2addr", k12x, kIndexNone, kContinue, kShl, kVerifyRegAWide | kVerifyRegB) \
  V(0xC4, SHR_LONG_2ADDR, "shr-long/2addr", k12x, kIndexNone, kContinue, kShr, kVerifyRegAWide | kVerifyRegB) \
  V(0xC5, USHR_LONG_2ADDR, "ushr-long/2addr", k12x, kIndexNone, kContinue, kUshr, kVerifyRegAWide | kVerifyRegB) \
  V(0xC6, ADD_FLOAT_2ADDR, "add-float/2addr", k12x, kIndexNone, kContinue, kAdd, kVerifyRegA | kVerifyRegB) \
  V(0xC7, SUB_FLOAT_2ADDR, "sub-float/2addr", k12x, kIndexNone, kContinue, kSubtract, kVerifyRegA | kVerifyRegB) \
  V(0xC8, MUL_FLOAT_2ADDR, "mul-float/2addr", k12x, kIndexNone, kContinue, kMultiply, kVerifyRegA | kVerifyRegB) \
  V(0xC9, DIV_FLOAT_2ADDR, "div-float/2addr", k12x, kIndexNone, kContinue, kDivide, kVerifyRegA | kVerifyRegB) \
  V(0xCA, REM_FLOAT_2ADDR, "rem-float/2addr", k12x, kIndexNone, kContinue, kRemainder, kVerifyRegA | kVerifyRegB) \
  V(0xCB, ADD_DOUBLE_2ADDR, "add-double/2addr", k12x, kIndexNone, kContinue, kAdd, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xCC, SUB_DOUBLE_2ADDR, "sub-double/2addr", k12x, kIndexNone, kContinue, kSubtract, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xCD, MUL_DOUBLE_2ADDR, "mul-double/2addr", k12x, kIndexNone, kContinue, kMultiply, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xCE, DIV_DOUBLE_2ADDR, "div-double/2addr", k12x, kIndexNone, kContinue, kDivide, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xCF, REM_DOUBLE_2ADDR, "rem-double/2addr", k12x, kIndexNone, kContinue, kRemainder, kVerifyRegAWide | kVerifyRegBWide) \
  V(0xD0, ADD_INT_LIT16, "add-int/lit16", k22s, kIndexNone, kContinue, kAdd | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xD1, RSUB_INT, "rsub-int", k22s, kIndexNone, kContinue, kSubtract | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xD2, MUL_INT_LIT16, "mul-int/lit16", k22s, kIndexNone, kContinue, kMultiply | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xD3, DIV_INT_LIT16, "div-int/lit16", k22s, kIndexNone, kContinue | kThrow, kDivide | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xD4, REM_INT_LIT16, "rem-int/lit16", k22s, kIndexNone, kContinue | kThrow, kRemainder | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xD5, AND_INT_LIT16, "and-int/lit16", k22s, kIndexNone, kContinue, kAnd | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xD6, OR_INT_LIT16, "or-int/lit16", k22s, kIndexNone, kContinue, kOr | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xD7, XOR_INT_LIT16, "xor-int/lit16", k22s, kIndexNone, kContinue, kXor | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xD8, ADD_INT_LIT8, "add-int/lit8", k22b, kIndexNone, kContinue, kAdd | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xD9, RSUB_INT_LIT8, "rsub-int/lit8", k22b, kIndexNone, kContinue, kSubtract | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xDA, MUL_INT_LIT8, "mul-int/lit8", k22b, kIndexNone, kContinue, kMultiply | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xDB, DIV_INT_LIT8, "div-int/lit8", k22b, kIndexNone, kContinue | kThrow, kDivide | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xDC, REM_INT_LIT8, "rem-int/lit8", k22b, kIndexNone, kContinue | kThrow, kRemainder | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xDD, AND_INT_LIT8, "and-int/lit8", k22b, kIndexNone, kContinue, kAnd | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xDE, OR_INT_LIT8, "or-int/lit8", k22b, kIndexNone, kContinue, kOr | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xDF, XOR_INT_LIT8, "xor-int/lit8", k22b, kIndexNone, kContinue, kXor | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xE0, SHL_INT_LIT8, "shl-int/lit8", k22b, kIndexNone, kContinue, kShl | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xE1, SHR_INT_LIT8, "shr-int/lit8", k22b, kIndexNone, kContinue, kShr | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xE2, USHR_INT_LIT8, "ushr-int/lit8", k22b, kIndexNone, kContinue, kUshr | kRegCFieldOrConstant, kVerifyRegA | kVerifyRegB) \
  V(0xE3, UNUSED_E3, "unused-e3", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xE4, UNUSED_E4, "unused-e4", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xE5, UNUSED_E5, "unused-e5", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xE6, UNUSED_E6, "unused-e6", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xE7, UNUSED_E7, "unused-e7", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xE8, UNUSED_E8, "unused-e8", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xE9, UNUSED_E9, "unused-e9", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xEA, UNUSED_EA, "unused-ea", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xEB, UNUSED_EB, "unused-eb", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xEC, UNUSED_EC, "unused-ec", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xED, UNUSED_ED, "unused-ed", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xEE, UNUSED_EE, "unused-ee", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xEF, UNUSED_EF, "unused-ef", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF0, UNUSED_F0, "unused-f0", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF1, UNUSED_F1, "unused-f1", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF2, UNUSED_F2, "unused-f2", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF3, UNUSED_F3, "unused-f3", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF4, UNUSED_F4, "unused-f4", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF5, UNUSED_F5, "unused-f5", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF6, UNUSED_F6, "unused-f6", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF7, UNUSED_F7, "unused-f7", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF8, UNUSED_F8, "unused-f8", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xF9, UNUSED_F9, "unused-f9", k10x, kIndexUnknown, 0, 0, kVerifyError) \
  V(0xFA, INVOKE_POLYMORPHIC, "invoke-polymorphic", k45cc, kIndexMethodAndProtoRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgNonZero | kVerifyRegHPrototype) \
  V(0xFB, INVOKE_POLYMORPHIC_RANGE, "invoke-polymorphic/range", k4rcc, kIndexMethodAndProtoRef, kContinue | kThrow | kInvoke, 0, kVerifyRegBMethod | kVerifyVarArgRangeNonZero | kVerifyRegHPrototype) \
  V(0xFC, INVOKE_CUSTOM, "invoke-custom", k35c, kIndexCallSiteRef, kContinue | kThrow, 0, kVerifyRegBCallSite | kVerifyVarArg) \
  V(0xFD, INVOKE_CUSTOM_RANGE, "invoke-custom/range", k3rc, kIndexCallSiteRef, kContinue | kThrow, 0, kVerifyRegBCallSite | kVerifyVarArgRange) \
  V(0xFE, CONST_METHOD_HANDLE, "const-method-handle", k21c, kIndexMethodHandleRef, kContinue | kThrow, 0, kVerifyRegA | kVerifyRegBMethodHandle) \
  V(0xFF, CONST_METHOD_TYPE, "const-method-type", k21c, kIndexProtoRef, kContinue | kThrow, 0, kVerifyRegA | kVerifyRegBPrototype)

#define DEX_INSTRUCTION_FORMAT_LIST(V) \
  V(k10x) \
  V(k12x) \
  V(k11n) \
  V(k11x) \
  V(k10t) \
  V(k20t) \
  V(k22x) \
  V(k21t) \
  V(k21s) \
  V(k21h) \
  V(k21c) \
  V(k23x) \
  V(k22b) \
  V(k22t) \
  V(k22s) \
  V(k22c) \
  V(k32x) \
  V(k30t) \
  V(k31t) \
  V(k31i) \
  V(k31c) \
  V(k35c) \
  V(k3rc) \
  V(k45cc) \
  V(k4rcc) \
  V(k51l)

#endif  // ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_LIST_H_
#undef ART_LIBDEXFILE_DEX_DEX_INSTRUCTION_LIST_H_  // the guard in this file is just for cpplint
