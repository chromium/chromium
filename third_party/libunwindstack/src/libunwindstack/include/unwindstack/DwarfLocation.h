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

#include <unordered_map>

namespace unwindstack {

struct DwarfCie;

enum DwarfLocationEnum : uint8_t {
  DWARF_LOCATION_INVALID = 0,
  DWARF_LOCATION_UNDEFINED,
  DWARF_LOCATION_OFFSET,
  DWARF_LOCATION_VAL_OFFSET,
  DWARF_LOCATION_REGISTER,
  DWARF_LOCATION_EXPRESSION,
  DWARF_LOCATION_VAL_EXPRESSION,
  DWARF_LOCATION_PSEUDO_REGISTER,
};

struct DwarfLocation {
  DwarfLocationEnum type;
  uint64_t values[2];
};

struct DwarfLocations : public std::unordered_map<uint32_t, DwarfLocation> {
  const DwarfCie* cie;
  // The range of PCs where the locations are valid (end is exclusive).
  uint64_t pc_start = 0;
  uint64_t pc_end = 0;
};

}  // namespace unwindstack
