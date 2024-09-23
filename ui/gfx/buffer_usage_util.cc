// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/buffer_usage_util.h"

namespace gfx {

const char* BufferUsageToString(BufferUsage usage) {
  switch (usage) {
    case BufferUsage::GPU_READ:
      return "GPU_READ";
    case BufferUsage::SCANOUT:
      return "SCANOUT";
    case BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return "SCANOUT_CAMERA_READ_WRITE";
    case BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      return "CAMERA_AND_CPU_READ_WRITE";
    case BufferUsage::SCANOUT_CPU_READ_WRITE:
      return "SCANOUT_CPU_READ_WRITE";
    case BufferUsage::SCANOUT_VDA_WRITE:
      return "SCANOUT_VDA_WRITE";
    case BufferUsage::PROTECTED_SCANOUT:
      return "PROTECTED_SCANOUT";
    case BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
      return "PROTECTED_SCANOUT_VDA_WRITE";
    case BufferUsage::GPU_READ_CPU_READ_WRITE:
      return "GPU_READ_CPU_READ_WRITE";
    case BufferUsage::SCANOUT_VEA_CPU_READ:
      return "SCANOUT_VEA_CPU_READ";
    case BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return "VEA_READ_CAMERA_AND_CPU_READ_WRITE";
    case BufferUsage::SCANOUT_FRONT_RENDERING:
      return "SCANOUT_FRONT_RENDERING";
  }
  return "Invalid Usage";
}

}  // namespace gfx
