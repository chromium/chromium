// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/native_pixmap_usage_utils.h"

#include "ui/gfx/buffer_types.h"
#include "ui/ozone/public/native_pixmap_usage.h"

namespace ui {

NativePixmapUsageSet BufferUsageToNativePixmapUsage(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      return {NativePixmapUsage::kTexturing};
    case gfx::BufferUsage::SCANOUT:
      return {NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
              NativePixmapUsage::kRendering};
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return {NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
              NativePixmapUsage::kCpuRead, NativePixmapUsage::kCamera};
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      return {NativePixmapUsage::kCpuRead, NativePixmapUsage::kCamera};
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return {NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
              NativePixmapUsage::kCpuRead};
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      return {NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
              NativePixmapUsage::kHWVideoDecoder};
    case gfx::BufferUsage::PROTECTED_SCANOUT:
      return {NativePixmapUsage::kScanout, NativePixmapUsage::kProtected};
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
      return {NativePixmapUsage::kScanout, NativePixmapUsage::kProtected,
              NativePixmapUsage::kHWVideoDecoder};
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      return {NativePixmapUsage::kTexturing, NativePixmapUsage::kCpuRead};
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
      return {NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
              NativePixmapUsage::kCpuRead, NativePixmapUsage::kHWVideoEncoder};
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return {NativePixmapUsage::kTexturing, NativePixmapUsage::kCpuRead,
              NativePixmapUsage::kCamera, NativePixmapUsage::kHWVideoEncoder,
              NativePixmapUsage::kSWReadOften};
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return {NativePixmapUsage::kScanout, NativePixmapUsage::kTexturing,
              NativePixmapUsage::kFrontRendering};
  }
}

}  // namespace ui
