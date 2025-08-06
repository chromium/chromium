// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_NATIVE_PIXMAP_USAGE_H_
#define UI_OZONE_PUBLIC_NATIVE_PIXMAP_USAGE_H_

namespace ui {

// Corresponds to how ui/ozone platforms "use" a native pixmap buffer.
enum class NativePixmapUsage : uint32_t {
  // Buffer will be used as scanout buffer.
  SCANOUT = 1 << 0,
  // Buffer will be used for rendering.
  RENDERING = 1 << 1,
  // Buffer will be used as a texture that will be sampled from.
  TEXTURING = 1 << 2,
  // Buffer is linear i.e. not tiled.
  LINEAR = 1 << 3,
  // Buffer will be written to by a camera subsystem.
  CAMERA = 1 << 4,
  // Buffer will be protected i.e. inaccessible to unprivileged users.
  PROTECTED = 1 << 5,
  // Buffer will be written by a video decode accelerator.
  HW_VIDEO_DECODER = 1 << 6,
  // Buffer will be read by a video encode accelerator.
  HW_VIDEO_ENCODER = 1 << 7,
  // Buffer will be read often by the CPU software.
  SW_READ_OFTEN = 1 << 8,
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_NATIVE_PIXMAP_USAGE_H_
