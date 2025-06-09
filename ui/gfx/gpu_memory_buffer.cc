// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

#include "base/functional/callback.h"

namespace gfx {

void GpuMemoryBuffer::MapAsync(base::OnceCallback<void(bool)> result_cb) {
  std::move(result_cb).Run(Map());
}

bool GpuMemoryBuffer::AsyncMappingIsNonBlocking() const {
  return false;
}

}  // namespace gfx
