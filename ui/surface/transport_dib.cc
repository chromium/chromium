// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/transport_dib.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "skia/ext/platform_canvas.h"

TransportDIB::TransportDIB(base::UnsafeSharedMemoryRegion region)
    : shm_region_(std::move(region)) {}

TransportDIB::~TransportDIB() = default;

// static
std::unique_ptr<TransportDIB> TransportDIB::Map(
    base::UnsafeSharedMemoryRegion region) {
  std::unique_ptr<TransportDIB> dib = CreateWithHandle(std::move(region));
  if (!dib->Map())
    return nullptr;
  return dib;
}

// static
std::unique_ptr<TransportDIB> TransportDIB::CreateWithHandle(
    base::UnsafeSharedMemoryRegion region) {
  return base::WrapUnique(new TransportDIB(std::move(region)));
}

std::unique_ptr<SkCanvas> TransportDIB::GetPlatformCanvas(int w,
                                                          int h,
                                                          bool opaque) {
  if (!shm_region_.IsValid())
    return nullptr;

#if defined(OS_WIN)
  // This DIB already mapped the file into this process, but PlatformCanvas
  // will map it again.
  DCHECK(!memory()) << "Mapped file twice in the same process.";

  // We can't check the canvas size before mapping, but it's safe because
  // Windows will fail to map the section if the dimensions of the canvas
  // are too large.
  std::unique_ptr<SkCanvas> canvas =
      skia::CreatePlatformCanvasWithSharedSection(
          w, h, opaque, shm_region_.GetPlatformHandle(),
          skia::RETURN_NULL_ON_FAILURE);

  // Calculate the size for the memory region backing the canvas.
  if (canvas)
    size_ = skia::PlatformCanvasStrideForWidth(w) * h;

  return canvas;
#else
  if ((!memory() && !Map()) || !VerifyCanvasSize(w, h))
    return nullptr;
  return skia::CreatePlatformCanvasWithPixels(w, h, opaque,
                                              static_cast<uint8_t*>(memory()),
                                              skia::RETURN_NULL_ON_FAILURE);
#endif
}

bool TransportDIB::Map() {
  if (!shm_region_.IsValid())
    return false;

  if (memory())
    return true;

  shm_mapping_ = shm_region_.Map();
  if (!shm_mapping_.IsValid()) {
    PLOG(ERROR) << "Failed to map transport DIB";
    return false;
  }

  size_ = shm_mapping_.size();
  return true;
}

void* TransportDIB::memory() const {
  return shm_mapping_.IsValid() ? shm_mapping_.memory() : nullptr;
}

base::UnsafeSharedMemoryRegion* TransportDIB::shared_memory_region() {
  return &shm_region_;
}

// static
bool TransportDIB::VerifyCanvasSize(int w, int h) {
  if (w <= 0 || h <= 0)
    return false;

  const size_t stride = skia::PlatformCanvasStrideForWidth(w);
  size_t canvas_size;
  if (!base::CheckMul(h, stride).AssignIfValid(&canvas_size))
    return false;

  return canvas_size <= size_;
}
