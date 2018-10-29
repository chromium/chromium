// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

#include "ui/gfx/generic_shared_memory_id.h"

namespace gfx {

GpuMemoryBufferHandle::GpuMemoryBufferHandle()
    : type(EMPTY_BUFFER), id(0), offset(0), stride(0) {}

// TODO(crbug.com/863011): Reset |type| and possibly the handles on the
// moved-from object.
GpuMemoryBufferHandle::GpuMemoryBufferHandle(GpuMemoryBufferHandle&& other) =
    default;

GpuMemoryBufferHandle& GpuMemoryBufferHandle::operator=(
    GpuMemoryBufferHandle&& other) = default;

GpuMemoryBufferHandle::~GpuMemoryBufferHandle() = default;

void GpuMemoryBuffer::SetColorSpace(const gfx::ColorSpace& color_space) {}

}  // namespace gfx
