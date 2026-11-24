/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <stddef.h>
#include <stdint.h>

namespace unwindstack {

enum ArchEnum : uint8_t {
  ARCH_UNKNOWN = 0,
  ARCH_ARM,
  ARCH_ARM64,
  ARCH_X86,
  ARCH_X86_64,
  ARCH_RISCV64,
};

static inline bool ArchIs32Bit(ArchEnum arch) {
  switch (arch) {
    case ARCH_ARM:
    case ARCH_X86:
      return true;
    default:
      return false;
  }
}

}  // namespace unwindstack
