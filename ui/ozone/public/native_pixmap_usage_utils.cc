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
      return NativePixmapBufferUsage::kGpuRead;
    case gfx::BufferUsage::SCANOUT:
      return NativePixmapBufferUsage::kScanout;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return NativePixmapBufferUsage::kScanoutCameraCpuReadWrite;
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      return NativePixmapBufferUsage::kCameraCpuReadWrite;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return NativePixmapBufferUsage::kScanoutCpuReadWrite;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      return NativePixmapBufferUsage::kScanoutVDAWrite;
    case gfx::BufferUsage::PROTECTED_SCANOUT:
      return NativePixmapBufferUsage::kProtectedScanout;
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
      return NativePixmapBufferUsage::kProtectedScanoutVDAWrite;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      return NativePixmapBufferUsage::kGpuReadCpuReadWrite;
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
      return NativePixmapBufferUsage::kScanoutVEACpuRead;
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return NativePixmapBufferUsage::kVEAReadCameraCpuReadWrite;
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return NativePixmapBufferUsage::kScanoutFrontRendering;
  }
}

}  // namespace ui
