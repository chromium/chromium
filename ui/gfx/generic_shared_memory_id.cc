// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/generic_shared_memory_id.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace gfx {

base::trace_event::MemoryAllocatorDumpGuid
GetGenericSharedGpuMemoryGUIDForTracing(
    uint64_t tracing_process_id,
    GenericSharedMemoryId generic_shared_memory_id) {
  return base::trace_event::MemoryAllocatorDumpGuid(
      base::StringPrintf("genericsharedmemory-x-process/%" PRIx64 "/%d",
                         tracing_process_id, generic_shared_memory_id.id));
}

}  // namespace gfx
