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

#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include <android-base/stringprintf.h>

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfMemory.h>
#include <unwindstack/Log.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>

#include "DwarfOp.h"

namespace unwindstack {

enum DwarfOpHandleFunc : uint8_t {
  OP_ILLEGAL = 0,
  OP_DEREF,
  OP_DEREF_SIZE,
  OP_PUSH,
  OP_DUP,
  OP_DROP,
  OP_OVER,
  OP_PICK,
  OP_SWAP,
  OP_ROT,
  OP_ABS,
  OP_AND,
  OP_DIV,
  OP_MINUS,
  OP_MOD,
  OP_MUL,
  OP_NEG,
  OP_NOT,
  OP_OR,
  OP_PLUS,
  OP_PLUS_UCONST,
  OP_SHL,
  OP_SHR,
  OP_SHRA,
  OP_XOR,
  OP_BRA,
  OP_EQ,
  OP_GE,
  OP_GT,
  OP_LE,
  OP_LT,
  OP_NE,
  OP_SKIP,
  OP_LIT,
  OP_REG,
  OP_REGX,
  OP_BREG,
  OP_BREGX,
  OP_NOP,
  OP_NOT_IMPLEMENTED,
};

struct OpCallback {
  // It may seem tempting to "clean this up" by replacing "const char[26]" with
  // "const char*", but doing so would place the entire callback table in
  // .data.rel.ro section, instead of .rodata section, and thus increase
  // dirty memory usage.  Libunwindstack is used by the linker and therefore
  // loaded for every running process, so every bit of memory counts.
  // Unlike C standard, C++ standard guarantees this array is big enough to
  // store the names, or else we would get a compilation error.
  const char name[26];

  // Similarily for this field, we do NOT want to directly store function
  // pointers here. Not only would that cause the callback table to be placed
  // in .data.rel.ro section, but it would be duplicated for each AddressType.
  // Instead, we use DwarfOpHandleFunc enum to decouple the callback table from
  // the function pointers.
  DwarfOpHandleFunc handle_func;

  uint8_t num_required_stack_values;
  uint8_t num_operands;
  uint8_t operands[2];
};

constexpr static OpCallback kCallbackTable[256] = {
    {"", OP_ILLEGAL, 0, 0, {}},  // 0x00 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0x01 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0x02 illegal op
    {
        // 0x03 DW_OP_addr
        "DW_OP_addr",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_absptr},
    },
    {"", OP_ILLEGAL, 0, 0, {}},  // 0x04 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0x05 illegal op
    {
        // 0x06 DW_OP_deref
        "DW_OP_deref",
        OP_DEREF,
        1,
        0,
        {},
    },
    {"", OP_ILLEGAL, 0, 0, {}},  // 0x07 illegal op
    {
        // 0x08 DW_OP_const1u
        "DW_OP_const1u",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_udata1},
    },
    {
        // 0x09 DW_OP_const1s
        "DW_OP_const1s",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_sdata1},
    },
    {
        // 0x0a DW_OP_const2u
        "DW_OP_const2u",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_udata2},
    },
    {
        // 0x0b DW_OP_const2s
        "DW_OP_const2s",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_sdata2},
    },
    {
        // 0x0c DW_OP_const4u
        "DW_OP_const4u",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_udata4},
    },
    {
        // 0x0d DW_OP_const4s
        "DW_OP_const4s",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_sdata4},
    },
    {
        // 0x0e DW_OP_const8u
        "DW_OP_const8u",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_udata8},
    },
    {
        // 0x0f DW_OP_const8s
        "DW_OP_const8s",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_sdata8},
    },
    {
        // 0x10 DW_OP_constu
        "DW_OP_constu",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_uleb128},
    },
    {
        // 0x11 DW_OP_consts
        "DW_OP_consts",
        OP_PUSH,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x12 DW_OP_dup
        "DW_OP_dup",
        OP_DUP,
        1,
        0,
        {},
    },
    {
        // 0x13 DW_OP_drop
        "DW_OP_drop",
        OP_DROP,
        1,
        0,
        {},
    },
    {
        // 0x14 DW_OP_over
        "DW_OP_over",
        OP_OVER,
        2,
        0,
        {},
    },
    {
        // 0x15 DW_OP_pick
        "DW_OP_pick",
        OP_PICK,
        0,
        1,
        {DW_EH_PE_udata1},
    },
    {
        // 0x16 DW_OP_swap
        "DW_OP_swap",
        OP_SWAP,
        2,
        0,
        {},
    },
    {
        // 0x17 DW_OP_rot
        "DW_OP_rot",
        OP_ROT,
        3,
        0,
        {},
    },
    {
        // 0x18 DW_OP_xderef
        "DW_OP_xderef",
        OP_NOT_IMPLEMENTED,
        2,
        0,
        {},
    },
    {
        // 0x19 DW_OP_abs
        "DW_OP_abs",
        OP_ABS,
        1,
        0,
        {},
    },
    {
        // 0x1a DW_OP_and
        "DW_OP_and",
        OP_AND,
        2,
        0,
        {},
    },
    {
        // 0x1b DW_OP_div
        "DW_OP_div",
        OP_DIV,
        2,
        0,
        {},
    },
    {
        // 0x1c DW_OP_minus
        "DW_OP_minus",
        OP_MINUS,
        2,
        0,
        {},
    },
    {
        // 0x1d DW_OP_mod
        "DW_OP_mod",
        OP_MOD,
        2,
        0,
        {},
    },
    {
        // 0x1e DW_OP_mul
        "DW_OP_mul",
        OP_MUL,
        2,
        0,
        {},
    },
    {
        // 0x1f DW_OP_neg
        "DW_OP_neg",
        OP_NEG,
        1,
        0,
        {},
    },
    {
        // 0x20 DW_OP_not
        "DW_OP_not",
        OP_NOT,
        1,
        0,
        {},
    },
    {
        // 0x21 DW_OP_or
        "DW_OP_or",
        OP_OR,
        2,
        0,
        {},
    },
    {
        // 0x22 DW_OP_plus
        "DW_OP_plus",
        OP_PLUS,
        2,
        0,
        {},
    },
    {
        // 0x23 DW_OP_plus_uconst
        "DW_OP_plus_uconst",
        OP_PLUS_UCONST,
        1,
        1,
        {DW_EH_PE_uleb128},
    },
    {
        // 0x24 DW_OP_shl
        "DW_OP_shl",
        OP_SHL,
        2,
        0,
        {},
    },
    {
        // 0x25 DW_OP_shr
        "DW_OP_shr",
        OP_SHR,
        2,
        0,
        {},
    },
    {
        // 0x26 DW_OP_shra
        "DW_OP_shra",
        OP_SHRA,
        2,
        0,
        {},
    },
    {
        // 0x27 DW_OP_xor
        "DW_OP_xor",
        OP_XOR,
        2,
        0,
        {},
    },
    {
        // 0x28 DW_OP_bra
        "DW_OP_bra",
        OP_BRA,
        1,
        1,
        {DW_EH_PE_sdata2},
    },
    {
        // 0x29 DW_OP_eq
        "DW_OP_eq",
        OP_EQ,
        2,
        0,
        {},
    },
    {
        // 0x2a DW_OP_ge
        "DW_OP_ge",
        OP_GE,
        2,
        0,
        {},
    },
    {
        // 0x2b DW_OP_gt
        "DW_OP_gt",
        OP_GT,
        2,
        0,
        {},
    },
    {
        // 0x2c DW_OP_le
        "DW_OP_le",
        OP_LE,
        2,
        0,
        {},
    },
    {
        // 0x2d DW_OP_lt
        "DW_OP_lt",
        OP_LT,
        2,
        0,
        {},
    },
    {
        // 0x2e DW_OP_ne
        "DW_OP_ne",
        OP_NE,
        2,
        0,
        {},
    },
    {
        // 0x2f DW_OP_skip
        "DW_OP_skip",
        OP_SKIP,
        0,
        1,
        {DW_EH_PE_sdata2},
    },
    {
        // 0x30 DW_OP_lit0
        "DW_OP_lit0",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x31 DW_OP_lit1
        "DW_OP_lit1",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x32 DW_OP_lit2
        "DW_OP_lit2",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x33 DW_OP_lit3
        "DW_OP_lit3",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x34 DW_OP_lit4
        "DW_OP_lit4",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x35 DW_OP_lit5
        "DW_OP_lit5",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x36 DW_OP_lit6
        "DW_OP_lit6",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x37 DW_OP_lit7
        "DW_OP_lit7",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x38 DW_OP_lit8
        "DW_OP_lit8",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x39 DW_OP_lit9
        "DW_OP_lit9",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x3a DW_OP_lit10
        "DW_OP_lit10",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x3b DW_OP_lit11
        "DW_OP_lit11",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x3c DW_OP_lit12
        "DW_OP_lit12",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x3d DW_OP_lit13
        "DW_OP_lit13",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x3e DW_OP_lit14
        "DW_OP_lit14",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x3f DW_OP_lit15
        "DW_OP_lit15",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x40 DW_OP_lit16
        "DW_OP_lit16",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x41 DW_OP_lit17
        "DW_OP_lit17",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x42 DW_OP_lit18
        "DW_OP_lit18",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x43 DW_OP_lit19
        "DW_OP_lit19",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x44 DW_OP_lit20
        "DW_OP_lit20",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x45 DW_OP_lit21
        "DW_OP_lit21",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x46 DW_OP_lit22
        "DW_OP_lit22",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x47 DW_OP_lit23
        "DW_OP_lit23",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x48 DW_OP_lit24
        "DW_OP_lit24",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x49 DW_OP_lit25
        "DW_OP_lit25",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x4a DW_OP_lit26
        "DW_OP_lit26",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x4b DW_OP_lit27
        "DW_OP_lit27",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x4c DW_OP_lit28
        "DW_OP_lit28",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x4d DW_OP_lit29
        "DW_OP_lit29",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x4e DW_OP_lit30
        "DW_OP_lit30",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x4f DW_OP_lit31
        "DW_OP_lit31",
        OP_LIT,
        0,
        0,
        {},
    },
    {
        // 0x50 DW_OP_reg0
        "DW_OP_reg0",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x51 DW_OP_reg1
        "DW_OP_reg1",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x52 DW_OP_reg2
        "DW_OP_reg2",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x53 DW_OP_reg3
        "DW_OP_reg3",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x54 DW_OP_reg4
        "DW_OP_reg4",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x55 DW_OP_reg5
        "DW_OP_reg5",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x56 DW_OP_reg6
        "DW_OP_reg6",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x57 DW_OP_reg7
        "DW_OP_reg7",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x58 DW_OP_reg8
        "DW_OP_reg8",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x59 DW_OP_reg9
        "DW_OP_reg9",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x5a DW_OP_reg10
        "DW_OP_reg10",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x5b DW_OP_reg11
        "DW_OP_reg11",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x5c DW_OP_reg12
        "DW_OP_reg12",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x5d DW_OP_reg13
        "DW_OP_reg13",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x5e DW_OP_reg14
        "DW_OP_reg14",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x5f DW_OP_reg15
        "DW_OP_reg15",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x60 DW_OP_reg16
        "DW_OP_reg16",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x61 DW_OP_reg17
        "DW_OP_reg17",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x62 DW_OP_reg18
        "DW_OP_reg18",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x63 DW_OP_reg19
        "DW_OP_reg19",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x64 DW_OP_reg20
        "DW_OP_reg20",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x65 DW_OP_reg21
        "DW_OP_reg21",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x66 DW_OP_reg22
        "DW_OP_reg22",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x67 DW_OP_reg23
        "DW_OP_reg23",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x68 DW_OP_reg24
        "DW_OP_reg24",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x69 DW_OP_reg25
        "DW_OP_reg25",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x6a DW_OP_reg26
        "DW_OP_reg26",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x6b DW_OP_reg27
        "DW_OP_reg27",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x6c DW_OP_reg28
        "DW_OP_reg28",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x6d DW_OP_reg29
        "DW_OP_reg29",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x6e DW_OP_reg30
        "DW_OP_reg30",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x6f DW_OP_reg31
        "DW_OP_reg31",
        OP_REG,
        0,
        0,
        {},
    },
    {
        // 0x70 DW_OP_breg0
        "DW_OP_breg0",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x71 DW_OP_breg1
        "DW_OP_breg1",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x72 DW_OP_breg2
        "DW_OP_breg2",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x73 DW_OP_breg3
        "DW_OP_breg3",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x74 DW_OP_breg4
        "DW_OP_breg4",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x75 DW_OP_breg5
        "DW_OP_breg5",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x76 DW_OP_breg6
        "DW_OP_breg6",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x77 DW_OP_breg7
        "DW_OP_breg7",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x78 DW_OP_breg8
        "DW_OP_breg8",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x79 DW_OP_breg9
        "DW_OP_breg9",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x7a DW_OP_breg10
        "DW_OP_breg10",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x7b DW_OP_breg11
        "DW_OP_breg11",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x7c DW_OP_breg12
        "DW_OP_breg12",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x7d DW_OP_breg13
        "DW_OP_breg13",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x7e DW_OP_breg14
        "DW_OP_breg14",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x7f DW_OP_breg15
        "DW_OP_breg15",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x80 DW_OP_breg16
        "DW_OP_breg16",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x81 DW_OP_breg17
        "DW_OP_breg17",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x82 DW_OP_breg18
        "DW_OP_breg18",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x83 DW_OP_breg19
        "DW_OP_breg19",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x84 DW_OP_breg20
        "DW_OP_breg20",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x85 DW_OP_breg21
        "DW_OP_breg21",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x86 DW_OP_breg22
        "DW_OP_breg22",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x87 DW_OP_breg23
        "DW_OP_breg23",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x88 DW_OP_breg24
        "DW_OP_breg24",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x89 DW_OP_breg25
        "DW_OP_breg25",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x8a DW_OP_breg26
        "DW_OP_breg26",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x8b DW_OP_breg27
        "DW_OP_breg27",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x8c DW_OP_breg28
        "DW_OP_breg28",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x8d DW_OP_breg29
        "DW_OP_breg29",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x8e DW_OP_breg30
        "DW_OP_breg30",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x8f DW_OP_breg31
        "DW_OP_breg31",
        OP_BREG,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x90 DW_OP_regx
        "DW_OP_regx",
        OP_REGX,
        0,
        1,
        {DW_EH_PE_uleb128},
    },
    {
        // 0x91 DW_OP_fbreg
        "DW_OP_fbreg",
        OP_NOT_IMPLEMENTED,
        0,
        1,
        {DW_EH_PE_sleb128},
    },
    {
        // 0x92 DW_OP_bregx
        "DW_OP_bregx",
        OP_BREGX,
        0,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_sleb128},
    },
    {
        // 0x93 DW_OP_piece
        "DW_OP_piece",
        OP_NOT_IMPLEMENTED,
        0,
        1,
        {DW_EH_PE_uleb128},
    },
    {
        // 0x94 DW_OP_deref_size
        "DW_OP_deref_size",
        OP_DEREF_SIZE,
        1,
        1,
        {DW_EH_PE_udata1},
    },
    {
        // 0x95 DW_OP_xderef_size
        "DW_OP_xderef_size",
        OP_NOT_IMPLEMENTED,
        0,
        1,
        {DW_EH_PE_udata1},
    },
    {
        // 0x96 DW_OP_nop
        "DW_OP_nop",
        OP_NOP,
        0,
        0,
        {},
    },
    {
        // 0x97 DW_OP_push_object_address
        "DW_OP_push_object_address",
        OP_NOT_IMPLEMENTED,
        0,
        0,
        {},
    },
    {
        // 0x98 DW_OP_call2
        "DW_OP_call2",
        OP_NOT_IMPLEMENTED,
        0,
        1,
        {DW_EH_PE_udata2},
    },
    {
        // 0x99 DW_OP_call4
        "DW_OP_call4",
        OP_NOT_IMPLEMENTED,
        0,
        1,
        {DW_EH_PE_udata4},
    },
    {
        // 0x9a DW_OP_call_ref
        "DW_OP_call_ref",
        OP_NOT_IMPLEMENTED,
        0,
        0,  // Has a different sized operand (4 bytes or 8 bytes).
        {},
    },
    {
        // 0x9b DW_OP_form_tls_address
        "DW_OP_form_tls_address",
        OP_NOT_IMPLEMENTED,
        0,
        0,
        {},
    },
    {
        // 0x9c DW_OP_call_frame_cfa
        "DW_OP_call_frame_cfa",
        OP_NOT_IMPLEMENTED,
        0,
        0,
        {},
    },
    {
        // 0x9d DW_OP_bit_piece
        "DW_OP_bit_piece",
        OP_NOT_IMPLEMENTED,
        0,
        2,
        {DW_EH_PE_uleb128, DW_EH_PE_uleb128},
    },
    {
        // 0x9e DW_OP_implicit_value
        "DW_OP_implicit_value",
        OP_NOT_IMPLEMENTED,
        0,
        1,
        {DW_EH_PE_uleb128},
    },
    {
        // 0x9f DW_OP_stack_value
        "DW_OP_stack_value",
        OP_NOT_IMPLEMENTED,
        1,
        0,
        {},
    },
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa0 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa1 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa2 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa3 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa4 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa5 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa6 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa7 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa8 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xa9 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xaa illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xab illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xac illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xad illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xae illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xaf illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb0 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb1 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb2 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb3 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb4 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb5 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb6 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb7 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb8 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xb9 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xba illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xbb illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xbc illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xbd illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xbe illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xbf illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc0 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc1 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc2 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc3 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc4 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc5 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc6 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc7 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc8 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xc9 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xca illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xcb illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xcc illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xcd illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xce illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xcf illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd0 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd1 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd2 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd3 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd4 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd5 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd6 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd7 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd8 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xd9 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xda illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xdb illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xdc illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xdd illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xde illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xdf illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe0 DW_OP_lo_user
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe1 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe2 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe3 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe4 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe5 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe6 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe7 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe8 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xe9 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xea illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xeb illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xec illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xed illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xee illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xef illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf0 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf1 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf2 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf3 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf4 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf5 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf6 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf7 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf8 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xf9 illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xfa illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xfb illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xfc illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xfd illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xfe illegal op
    {"", OP_ILLEGAL, 0, 0, {}},  // 0xff DW_OP_hi_user
};

template <typename AddressType>
const typename DwarfOp<AddressType>::OpHandleFuncPtr DwarfOp<AddressType>::kOpHandleFuncList[] = {
    [OP_ILLEGAL] = nullptr,
    [OP_DEREF] = &DwarfOp<AddressType>::op_deref,
    [OP_DEREF_SIZE] = &DwarfOp<AddressType>::op_deref_size,
    [OP_PUSH] = &DwarfOp<AddressType>::op_push,
    [OP_DUP] = &DwarfOp<AddressType>::op_dup,
    [OP_DROP] = &DwarfOp<AddressType>::op_drop,
    [OP_OVER] = &DwarfOp<AddressType>::op_over,
    [OP_PICK] = &DwarfOp<AddressType>::op_pick,
    [OP_SWAP] = &DwarfOp<AddressType>::op_swap,
    [OP_ROT] = &DwarfOp<AddressType>::op_rot,
    [OP_ABS] = &DwarfOp<AddressType>::op_abs,
    [OP_AND] = &DwarfOp<AddressType>::op_and,
    [OP_DIV] = &DwarfOp<AddressType>::op_div,
    [OP_MINUS] = &DwarfOp<AddressType>::op_minus,
    [OP_MOD] = &DwarfOp<AddressType>::op_mod,
    [OP_MUL] = &DwarfOp<AddressType>::op_mul,
    [OP_NEG] = &DwarfOp<AddressType>::op_neg,
    [OP_NOT] = &DwarfOp<AddressType>::op_not,
    [OP_OR] = &DwarfOp<AddressType>::op_or,
    [OP_PLUS] = &DwarfOp<AddressType>::op_plus,
    [OP_PLUS_UCONST] = &DwarfOp<AddressType>::op_plus_uconst,
    [OP_SHL] = &DwarfOp<AddressType>::op_shl,
    [OP_SHR] = &DwarfOp<AddressType>::op_shr,
    [OP_SHRA] = &DwarfOp<AddressType>::op_shra,
    [OP_XOR] = &DwarfOp<AddressType>::op_xor,
    [OP_BRA] = &DwarfOp<AddressType>::op_bra,
    [OP_EQ] = &DwarfOp<AddressType>::op_eq,
    [OP_GE] = &DwarfOp<AddressType>::op_ge,
    [OP_GT] = &DwarfOp<AddressType>::op_gt,
    [OP_LE] = &DwarfOp<AddressType>::op_le,
    [OP_LT] = &DwarfOp<AddressType>::op_lt,
    [OP_NE] = &DwarfOp<AddressType>::op_ne,
    [OP_SKIP] = &DwarfOp<AddressType>::op_skip,
    [OP_LIT] = &DwarfOp<AddressType>::op_lit,
    [OP_REG] = &DwarfOp<AddressType>::op_reg,
    [OP_REGX] = &DwarfOp<AddressType>::op_regx,
    [OP_BREG] = &DwarfOp<AddressType>::op_breg,
    [OP_BREGX] = &DwarfOp<AddressType>::op_bregx,
    [OP_NOP] = &DwarfOp<AddressType>::op_nop,
    [OP_NOT_IMPLEMENTED] = &DwarfOp<AddressType>::op_not_implemented,
};

template <typename AddressType>
bool DwarfOp<AddressType>::Eval(uint64_t start, uint64_t end) {
  is_register_ = false;
  stack_.clear();
  memory_->set_cur_offset(start);
  dex_pc_set_ = false;

  // Unroll the first Decode calls to be able to check for a special
  // sequence of ops and values that indicate this is the dex pc.
  // The pattern is:
  //   OP_const4u (0x0c)  'D' 'E' 'X' '1'
  //   OP_drop (0x13)
  if (memory_->cur_offset() < end) {
    if (!Decode()) {
      return false;
    }
  } else {
    return true;
  }
  bool check_for_drop;
  if (cur_op_ == 0x0c && operands_.back() == 0x31584544) {
    check_for_drop = true;
  } else {
    check_for_drop = false;
  }
  if (memory_->cur_offset() < end) {
    if (!Decode()) {
      return false;
    }
  } else {
    return true;
  }

  if (check_for_drop && cur_op_ == 0x13) {
    dex_pc_set_ = true;
  }

  uint32_t iterations = 2;
  while (memory_->cur_offset() < end) {
    if (!Decode()) {
      return false;
    }
    // To protect against a branch that creates an infinite loop,
    // terminate if the number of iterations gets too high.
    if (iterations++ == 1000) {
      last_error_.code = DWARF_ERROR_TOO_MANY_ITERATIONS;
      return false;
    }
  }
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::Decode() {
  last_error_.code = DWARF_ERROR_NONE;
  if (!memory_->ReadBytes(&cur_op_, 1)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_->cur_offset();
    return false;
  }

  const auto* op = &kCallbackTable[cur_op_];
  if (op->handle_func == OP_ILLEGAL) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }

  const auto handle_func = kOpHandleFuncList[op->handle_func];

  // Make sure that the required number of stack elements is available.
  if (stack_.size() < op->num_required_stack_values) {
    last_error_.code = DWARF_ERROR_STACK_INDEX_NOT_VALID;
    return false;
  }

  operands_.clear();
  for (size_t i = 0; i < op->num_operands; i++) {
    uint64_t value;
    if (!memory_->ReadEncodedValue<AddressType>(op->operands[i], &value)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_->cur_offset();
      return false;
    }
    operands_.push_back(value);
  }
  return (this->*handle_func)();
}

template <typename AddressType>
void DwarfOp<AddressType>::GetLogInfo(uint64_t start, uint64_t end,
                                      std::vector<std::string>* lines) {
  memory_->set_cur_offset(start);
  while (memory_->cur_offset() < end) {
    uint8_t cur_op;
    if (!memory_->ReadBytes(&cur_op, 1)) {
      return;
    }

    std::string raw_string(android::base::StringPrintf("Raw Data: 0x%02x", cur_op));
    std::string log_string;
    const auto* op = &kCallbackTable[cur_op];
    if (op->handle_func == OP_ILLEGAL) {
      log_string = "Illegal";
    } else {
      log_string = op->name;
      uint64_t start_offset = memory_->cur_offset();
      for (size_t i = 0; i < op->num_operands; i++) {
        uint64_t value;
        if (!memory_->ReadEncodedValue<AddressType>(op->operands[i], &value)) {
          return;
        }
        log_string += ' ' + std::to_string(value);
      }
      uint64_t end_offset = memory_->cur_offset();

      memory_->set_cur_offset(start_offset);
      for (size_t i = start_offset; i < end_offset; i++) {
        uint8_t byte;
        if (!memory_->ReadBytes(&byte, 1)) {
          return;
        }
        raw_string += android::base::StringPrintf(" 0x%02x", byte);
      }
      memory_->set_cur_offset(end_offset);
    }
    lines->push_back(std::move(log_string));
    lines->push_back(std::move(raw_string));
  }
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_deref() {
  // Read the address and dereference it.
  AddressType addr = StackPop();
  AddressType value;
  if (!regular_memory()->ReadFully(addr, &value, sizeof(value))) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = addr;
    return false;
  }
  stack_.push_front(value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_deref_size() {
  AddressType bytes_to_read = OperandAt(0);
  if (bytes_to_read > sizeof(AddressType) || bytes_to_read == 0) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  // Read the address and dereference it.
  AddressType addr = StackPop();
  AddressType value = 0;
  if (!regular_memory()->ReadFully(addr, &value, bytes_to_read)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = addr;
    return false;
  }
  stack_.push_front(value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_push() {
  // Push all of the operands.
  for (auto operand : operands_) {
    stack_.push_front(operand);
  }
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_dup() {
  stack_.push_front(StackAt(0));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_drop() {
  StackPop();
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_over() {
  stack_.push_front(StackAt(1));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_pick() {
  AddressType index = OperandAt(0);
  if (index >= StackSize()) {
    last_error_.code = DWARF_ERROR_STACK_INDEX_NOT_VALID;
    return false;
  }
  stack_.push_front(StackAt(index));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_swap() {
  AddressType old_value = stack_[0];
  stack_[0] = stack_[1];
  stack_[1] = old_value;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_rot() {
  AddressType top = stack_[0];
  stack_[0] = stack_[1];
  stack_[1] = stack_[2];
  stack_[2] = top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_abs() {
  SignedType signed_value = static_cast<SignedType>(stack_[0]);
  if (signed_value < 0) {
    signed_value = -signed_value;
  }
  stack_[0] = static_cast<AddressType>(signed_value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_and() {
  AddressType top = StackPop();
  stack_[0] &= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_div() {
  AddressType top = StackPop();
  if (top == 0) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  SignedType signed_divisor = static_cast<SignedType>(top);
  SignedType signed_dividend = static_cast<SignedType>(stack_[0]);
  stack_[0] = static_cast<AddressType>(signed_dividend / signed_divisor);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_minus() {
  AddressType top = StackPop();
  stack_[0] -= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_mod() {
  AddressType top = StackPop();
  if (top == 0) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  stack_[0] %= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_mul() {
  AddressType top = StackPop();
  stack_[0] *= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_neg() {
  SignedType signed_value = static_cast<SignedType>(stack_[0]);
  stack_[0] = static_cast<AddressType>(-signed_value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_not() {
  stack_[0] = ~stack_[0];
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_or() {
  AddressType top = StackPop();
  stack_[0] |= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_plus() {
  AddressType top = StackPop();
  stack_[0] += top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_plus_uconst() {
  stack_[0] += OperandAt(0);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_shl() {
  AddressType top = StackPop();
  stack_[0] <<= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_shr() {
  AddressType top = StackPop();
  stack_[0] >>= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_shra() {
  AddressType top = StackPop();
  SignedType signed_value = static_cast<SignedType>(stack_[0]) >> top;
  stack_[0] = static_cast<AddressType>(signed_value);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_xor() {
  AddressType top = StackPop();
  stack_[0] ^= top;
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_bra() {
  // Requires one stack element.
  AddressType top = StackPop();
  if (top == 0) {
    return true;
  }

  int16_t offset = static_cast<int16_t>(OperandAt(0));
  uint64_t cur_offset = memory_->cur_offset() + offset;
  memory_->set_cur_offset(cur_offset);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_eq() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] == top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_ge() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] >= top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_gt() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] > top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_le() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] <= top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_lt() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] < top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_ne() {
  AddressType top = StackPop();
  stack_[0] = bool_to_dwarf_bool(stack_[0] != top);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_skip() {
  int16_t offset = static_cast<int16_t>(OperandAt(0));
  uint64_t cur_offset = memory_->cur_offset() + offset;
  memory_->set_cur_offset(cur_offset);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_lit() {
  stack_.push_front(cur_op() - 0x30);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_reg() {
  is_register_ = true;
  stack_.push_front(cur_op() - 0x50);
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_regx() {
  is_register_ = true;
  stack_.push_front(OperandAt(0));
  return true;
}

// It's not clear for breg/bregx, if this op should read the current
// value of the register, or where we think that register is located.
// For simplicity, the code will read the value before doing the unwind.
template <typename AddressType>
bool DwarfOp<AddressType>::op_breg() {
  uint16_t reg = cur_op() - 0x70;
  if (reg >= regs_info_->Total()) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  stack_.push_front(regs_info_->Get(reg) + OperandAt(0));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_bregx() {
  AddressType reg = OperandAt(0);
  if (reg >= regs_info_->Total()) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }
  stack_.push_front(regs_info_->Get(reg) + OperandAt(1));
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_nop() {
  return true;
}

template <typename AddressType>
bool DwarfOp<AddressType>::op_not_implemented() {
  last_error_.code = DWARF_ERROR_NOT_IMPLEMENTED;
  return false;
}

// Explicitly instantiate DwarfOp.
template class DwarfOp<uint32_t>;
template class DwarfOp<uint64_t>;

}  // namespace unwindstack
