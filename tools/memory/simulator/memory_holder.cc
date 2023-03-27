// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/simulator/memory_holder.h"

namespace memory_simulator {

MemoryHolder::MemoryHolder() = default;
MemoryHolder::~MemoryHolder() = default;

uint64_t MemoryHolder::Rand() {
  return random_generator_.RandUint64();
}

}  // namespace memory_simulator
