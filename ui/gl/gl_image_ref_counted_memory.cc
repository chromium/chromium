// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_ref_counted_memory.h"

#include <stddef.h>

#include "base/check.h"
#include "base/memory/ref_counted_memory.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "ui/gfx/buffer_format_util.h"

namespace gl {

GLImageRefCountedMemory::GLImageRefCountedMemory(const gfx::Size& size)
    : GLImageMemory(size) {}

GLImageRefCountedMemory::~GLImageRefCountedMemory() {}

bool GLImageRefCountedMemory::Initialize(
    base::RefCountedMemory* ref_counted_memory,
    gfx::BufferFormat format) {
  if (!GLImageMemory::Initialize(
          ref_counted_memory->front(), format,
          gfx::RowSizeForBufferFormat(GetSize().width(), format, 0))) {
    return false;
  }

  DCHECK(!ref_counted_memory_.get());
  ref_counted_memory_ = ref_counted_memory;
  return true;
}

void GLImageRefCountedMemory::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {
  // Log size 0 if |ref_counted_memory_| has been released.
  size_t size_in_bytes = ref_counted_memory_ ? ref_counted_memory_->size() : 0;

  // Dump under "/private_memory", as the base class may also dump to
  // "/texture_memory".
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name + "/private_memory");
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(size_in_bytes));

  pmd->AddSuballocation(dump->guid(),
                        base::trace_event::MemoryDumpManager::GetInstance()
                            ->system_allocator_pool_name());
}

}  // namespace gl
