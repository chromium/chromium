// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/partition_allocator/inspect_utils.h"

namespace partition_alloc::tools {

RemoteProcessMemoryReader::~RemoteProcessMemoryReader() = default;

bool RemoteProcessMemoryReader::IsValid() const {
  return is_valid_;
}

}  // namespace partition_alloc::tools
