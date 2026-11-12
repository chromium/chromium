/*
 * Copyright (C) 2022 The Android Open Source Project
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

enum Riscv64Reg : uint16_t {
  RISCV64_REG_PC,
  RISCV64_REG_RA,
  RISCV64_REG_SP,
  RISCV64_REG_GP,
  RISCV64_REG_TP,
  RISCV64_REG_T0,
  RISCV64_REG_T1,
  RISCV64_REG_T2,
  RISCV64_REG_S0,
  RISCV64_REG_S1,
  RISCV64_REG_A0,
  RISCV64_REG_A1,
  RISCV64_REG_A2,
  RISCV64_REG_A3,
  RISCV64_REG_A4,
  RISCV64_REG_A5,
  RISCV64_REG_A6,
  RISCV64_REG_A7,
  RISCV64_REG_S2,
  RISCV64_REG_S3,
  RISCV64_REG_S4,
  RISCV64_REG_S5,
  RISCV64_REG_S6,
  RISCV64_REG_S7,
  RISCV64_REG_S8,
  RISCV64_REG_S9,
  RISCV64_REG_S10,
  RISCV64_REG_S11,
  RISCV64_REG_T3,
  RISCV64_REG_T4,
  RISCV64_REG_T5,
  RISCV64_REG_T6,
  RISCV64_REG_MAX,
};

}  // namespace unwindstack
