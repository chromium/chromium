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

namespace unwindstack {

enum DwarfErrorCode : uint8_t {
  DWARF_ERROR_NONE,
  DWARF_ERROR_MEMORY_INVALID,
  DWARF_ERROR_ILLEGAL_VALUE,
  DWARF_ERROR_ILLEGAL_STATE,
  DWARF_ERROR_STACK_INDEX_NOT_VALID,
  DWARF_ERROR_NOT_IMPLEMENTED,
  DWARF_ERROR_TOO_MANY_ITERATIONS,
  DWARF_ERROR_CFA_NOT_DEFINED,
  DWARF_ERROR_UNSUPPORTED_VERSION,
  DWARF_ERROR_NO_FDES,
};

struct DwarfErrorData {
  DwarfErrorCode code;
  uint64_t address;
};

}  // namespace unwindstack
