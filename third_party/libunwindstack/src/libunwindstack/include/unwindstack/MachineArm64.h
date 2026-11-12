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

#pragma once

#include <stdint.h>

namespace unwindstack {

enum Arm64Reg : uint16_t {
  ARM64_REG_R0 = 0,
  ARM64_REG_R1,
  ARM64_REG_R2,
  ARM64_REG_R3,
  ARM64_REG_R4,
  ARM64_REG_R5,
  ARM64_REG_R6,
  ARM64_REG_R7,
  ARM64_REG_R8,
  ARM64_REG_R9,
  ARM64_REG_R10,
  ARM64_REG_R11,
  ARM64_REG_R12,
  ARM64_REG_R13,
  ARM64_REG_R14,
  ARM64_REG_R15,
  ARM64_REG_R16,
  ARM64_REG_R17,
  ARM64_REG_R18,
  ARM64_REG_R19,
  ARM64_REG_R20,
  ARM64_REG_R21,
  ARM64_REG_R22,
  ARM64_REG_R23,
  ARM64_REG_R24,
  ARM64_REG_R25,
  ARM64_REG_R26,
  ARM64_REG_R27,
  ARM64_REG_R28,
  ARM64_REG_R29,
  ARM64_REG_R30,
  ARM64_REG_R31,
  ARM64_REG_PC,
  ARM64_REG_PSTATE,
  ARM64_REG_LAST,

  ARM64_REG_SP = ARM64_REG_R31,
  ARM64_REG_LR = ARM64_REG_R30,

  // Pseudo registers. These are not machine registers.

  // AARCH64 Return address signed state pseudo-register
  ARM64_PREG_RA_SIGN_STATE = 34,
  ARM64_PREG_FIRST = ARM64_PREG_RA_SIGN_STATE,
  ARM64_PREG_LAST,
};

}  // namespace unwindstack
