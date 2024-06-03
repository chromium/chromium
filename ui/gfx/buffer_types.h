// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BUFFER_TYPES_H_
#define UI_GFX_BUFFER_TYPES_H_

#include <stdint.h>

#include <tuple>

namespace gfx {

// The format needs to be taken into account when mapping a buffer into the
// client's address space.
enum class BufferFormat : uint8_t {
  // Used as an enum for metrics. DO NOT reorder or delete values. Rather,
  // add them at the end and increment kMaxValue.
  R_8,
  R_16,
  RG_88,
  RG_1616,
  BGR_565,
  RGBA_4444,
  RGBX_8888,
  RGBA_8888,
  BGRX_8888,
  BGRA_1010102,
  RGBA_1010102,
  BGRA_8888,
  RGBA_F16,
  YVU_420,
  YUV_420_BIPLANAR,
  YUVA_420_TRIPLANAR,
  P010,

  LAST = P010,
  kMaxValue = LAST
};

// The usage mode affects how a buffer can be used. Only buffers created with
// *_CPU_READ_WRITE_* can be mapped into the client's address space and accessed
// by the CPU.
// *_VDA_WRITE is for cases where a video decode accelerator writes into the
// buffers.
// PROTECTED_* are for HW protected buffers that cannot be read by the CPU and
// can only be read in protected GPU contexts or scanned out to overlays.
// At present, SCANOUT implies GPU_READ_WRITE. This doesn't apply to other
// SCANOUT_* values.

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
  PROTECTED_SCANOUT,
  PROTECTED_SCANOUT_VDA_WRITE,
  GPU_READ_CPU_READ_WRITE,
  SCANOUT_VEA_CPU_READ,
  SCANOUT_FRONT_RENDERING,
  VEA_READ_CAMERA_AND_CPU_READ_WRITE,

  LAST = VEA_READ_CAMERA_AND_CPU_READ_WRITE
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

// Used to identify the plane of a GpuMemoryBuffer to use when creating a
// SharedImage.
enum class BufferPlane {
  // For single-plane GpuMemoryBuffer, this refers to that single plane. For
  // YUV_420, YUV_420_BIPLANAR, YUVA_420_TRIPLANAR, and P010 GpuMemoryBuffers,
  // this refers to an RGB representation of the planes (either bound directly
  // as a texture or created through an extra copy).
  DEFAULT,
  // The Y plane for YUV_420, YUV_420_BIPLANAR, YUVA_420_TRIPLANAR, and P010.
  Y,
  // The UV plane for YUV_420_BIPLANAR, YUVA_420_TRIPLANAR and P010.
  UV,
  // The U plane for YUV_420.
  U,
  // The V plane for YUV_420.
  V,
  // The A plane for YUVA_420_TRIPLANAR.
  A,

  LAST = A
};

}  // namespace gfx

#endif  // UI_GFX_BUFFER_TYPES_H_
