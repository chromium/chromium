// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DUMB_BUFFER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DUMB_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"

class SkCanvas;
struct SkImageInfo;
class SkSurface;

namespace ui {

class DrmDevice;

// Wrapper for a DRM allocated buffer. Keeps track of the native properties of
// the buffer and wraps the pixel memory into a SkSurface which can be used to
// draw into using Skia.
class DrmDumbBuffer {
 public:
  enum class HandleCloser {
    DESTROY_DUMB,
    GEM_CLOSE,
  };

  DrmDumbBuffer(const scoped_refptr<DrmDevice>& drm);
  ~DrmDumbBuffer();

  // Allocates a new dumb buffer, maps it, and wraps it in an SkSurface.
  // |info| determines the buffer characteristics (size, color format).
  bool Initialize(const SkImageInfo& info);

  // Imports an existing framebuffer, maps it, and wraps it in an SkSurface.
  bool InitializeFromFramebuffer(uint32_t framebuffer_id);

  SkCanvas* GetCanvas() const;
  SkSurface* surface() const { return surface_.get(); }

  uint32_t GetHandle() const;
  gfx::Size GetSize() const;
  uint32_t stride() const { return stride_; }

 private:
  bool MapDumbBuffer(const SkImageInfo& info);

  const scoped_refptr<DrmDevice> drm_;

  // Length of a row of pixels.
  uint32_t stride_ = 0;

  // Buffer handle used by the DRM allocator.
  uint32_t handle_ = 0;

  // Method of closing |handle_|.
  HandleCloser handle_closer_ = HandleCloser::DESTROY_DUMB;

  // Base address for memory mapping.
  void* mmap_base_ = 0;

  // Size for memory mapping.
  size_t mmap_size_ = 0;

  // Wrapper around the native pixel memory.
  sk_sp<SkSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(DrmDumbBuffer);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DUMB_BUFFER_H_
