// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/gbm_util.h"

#include "base/notreached.h"
#include "ui/gfx/linux/gbm_defines.h"

namespace ui {

uint32_t BufferUsageToGbmFlags(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      return GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT:
      return GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_WRITE | GBM_BO_USE_SCANOUT |
             GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_WRITE;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      return GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING |
             GBM_BO_USE_HW_VIDEO_DECODER;
    case gfx::BufferUsage::PROTECTED_SCANOUT:
      return GBM_BO_USE_SCANOUT | GBM_BO_USE_PROTECTED;
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
      return GBM_BO_USE_SCANOUT | GBM_BO_USE_PROTECTED |
             GBM_BO_USE_HW_VIDEO_DECODER;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_TEXTURING;
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING |
             GBM_BO_USE_HW_VIDEO_ENCODER;
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_WRITE |
             GBM_BO_USE_TEXTURING | GBM_BO_USE_HW_VIDEO_ENCODER |
             GBM_BO_USE_SW_READ_OFTEN;
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING |
             GBM_BO_USE_FRONT_RENDERING;
  }
}

uint32_t NativePixmapUsageToGbmFlags(NativePixmapUsageSet usage) {
  uint32_t flags = 0;
  if (usage.Has(NativePixmapUsage::kScanout)) {
    flags |= GBM_BO_USE_SCANOUT;
  }
  if (usage.Has(NativePixmapUsage::kRendering)) {
    flags |= GBM_BO_USE_RENDERING;
  }
  if (usage.Has(NativePixmapUsage::kTexturing)) {
    flags |= GBM_BO_USE_TEXTURING;
  }
  if (usage.Has(NativePixmapUsage::kCpuRead)) {
    flags |= GBM_BO_USE_LINEAR;
  }
  if (usage.Has(NativePixmapUsage::kCamera)) {
    flags |= GBM_BO_USE_CAMERA_WRITE;
  }
  if (usage.Has(NativePixmapUsage::kProtected)) {
    flags |= GBM_BO_USE_PROTECTED;
  }
  if (usage.Has(NativePixmapUsage::kHWVideoDecoder)) {
    flags |= GBM_BO_USE_HW_VIDEO_DECODER;
  }
  if (usage.Has(NativePixmapUsage::kHWVideoEncoder)) {
    flags |= GBM_BO_USE_HW_VIDEO_ENCODER;
  }
  if (usage.Has(NativePixmapUsage::kSWReadOften)) {
    flags |= GBM_BO_USE_SW_READ_OFTEN;
  }
  if (usage.Has(NativePixmapUsage::kFrontRendering)) {
    flags |= GBM_BO_USE_FRONT_RENDERING;
  }
  return flags;
}

}  // namespace ui
