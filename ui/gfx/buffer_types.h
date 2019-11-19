// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BUFFER_TYPES_H_
#define UI_GFX_BUFFER_TYPES_H_

#include <tuple>

namespace gfx {

// The format needs to be taken into account when mapping a buffer into the
// client's address space.
enum class BufferFormat {
  R_8,
  R_16,
  RG_88,
  BGR_565,
  RGBA_4444,
  RGBX_8888,
  RGBA_8888,
  BGRX_8888,
  BGRX_1010102,
  RGBX_1010102,
  BGRA_8888,
  RGBA_F16,
  YVU_420,
  YUV_420_BIPLANAR,
  P010,

  LAST = P010
};

// The usage mode affects how a buffer can be used. Only buffers created with
// *_CPU_READ_WRITE_* can be mapped into the client's address space and accessed
// by the CPU. SCANOUT implies GPU_READ_WRITE.
// *_VDA_WRITE is for cases where a video decode accellerator writes into
// the buffers.

// TODO(reveman): Add GPU_READ_WRITE for use-cases where SCANOUT is not
// required.
enum class BufferUsage {
  GPU_READ,
  SCANOUT,
  // SCANOUT_CAMERA_READ_WRITE implies CPU_READ_WRITE.
  SCANOUT_CAMERA_READ_WRITE,
  CAMERA_AND_CPU_READ_WRITE,
  SCANOUT_CPU_READ_WRITE,
  SCANOUT_VDA_WRITE,
  GPU_READ_CPU_READ_WRITE,
  SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE,

  LAST = SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE
};

struct BufferUsageAndFormat {
  BufferUsageAndFormat()
      : usage(BufferUsage::GPU_READ), format(BufferFormat::RGBA_8888) {}
  BufferUsageAndFormat(BufferUsage usage, BufferFormat format)
      : usage(usage), format(format) {}

  bool operator==(const BufferUsageAndFormat& other) const {
    return std::tie(usage, format) == std::tie(other.usage, other.format);
  }

  BufferUsage usage;
  BufferFormat format;
};

}  // namespace gfx

#endif  // UI_GFX_BUFFER_TYPES_H_
