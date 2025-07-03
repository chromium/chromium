// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

#include "base/functional/callback.h"
#include "ui/gfx/buffer_format_util.h"

namespace gfx {

void GpuMemoryBuffer::MapAsync(base::OnceCallback<void(bool)> result_cb) {
  std::move(result_cb).Run(Map());
}

bool GpuMemoryBuffer::AsyncMappingIsNonBlocking() const {
  return false;
}

base::span<uint8_t> GpuMemoryBuffer::memory_span(size_t plane) {
  uint8_t* data = static_cast<uint8_t*>(memory(plane));
  if (!data) {
    return {};
  }
  size_t size = 0;
  if (!PlaneSizeForBufferFormatChecked(GetSize(), GetFormat(), plane, &size)) {
    return {};
  }

  // SAFETY: The safety is ensured by the contract of the `GpuMemoryBuffer`.
  // `data` is a pointer to memory that has been mapped by `Map()` and and
  // the `size` is calculated using the buffer utility method used by all
  // `GpuMemoryBuffer` clients already.
  return UNSAFE_BUFFERS(base::span<uint8_t>(data, size));
}

}  // namespace gfx
