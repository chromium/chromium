// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_CANVAS_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_CANVAS_H_

#include <lib/ui/scenic/cpp/resources.h>
#include <memory>

#include "base/macros.h"
#include "base/memory/shared_memory_mapping.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace scenic {
class Session;
}  // namespace scenic

namespace ui {

class ScenicWindow;

// SurfaceOzoneCanvas implementation for ScenicWindow. It allows to draw on a
// ScenicWindow.
class ScenicWindowCanvas : public SurfaceOzoneCanvas {
 public:
  // |scenic_surface| must outlive the canvas. ScenicSurface owns the
  // scenic::Session used in this class for all drawing operations.
  explicit ScenicWindowCanvas(ScenicSurface* scenic_surface);
  ~ScenicWindowCanvas() override;

  // SurfaceOzoneCanvas implementation.
  void ResizeCanvas(const gfx::Size& viewport_size) override;
  sk_sp<SkSurface> GetSurface() override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  // Use 2 buffers: one is shown on the screen while the other is used to render
  // the next frame.
  static const int kNumBuffers = 2;

  struct Frame {
    Frame();
    ~Frame();

    // Allocates and maps memory for a frame of |size| (in physical in pixels)
    // and then registers it with |scenic|.
    void Initialize(gfx::Size size, scenic::Session* scenic);

    // Copies pixels covered by |dirty_region| from another |frame|.
    void CopyDirtyRegionFrom(const Frame& frame);

    bool is_empty() { return !surface; }

    // Shared memory for the buffer.
    base::WritableSharedMemoryMapping memory_mapping;

    // Scenic Memory resource for |memory_region|.
    std::unique_ptr<scenic::Memory> scenic_memory;

    // SkSurface that wraps |memory_mapping|.
    sk_sp<SkSurface> surface;

    // Fence that will be released by Scenic when it stops using this frame.
    zx::event release_fence;

    // The region of the frame that's not up-to-date.
    SkRegion dirty_region;
  };

  Frame frames_[kNumBuffers];

  // Buffer index in |frames_| for the frame that's currently being rendered.
  int current_frame_ = 0;

  // View size in device pixels.
  gfx::Size viewport_size_;

  ScenicSurface* const scenic_surface_;

  DISALLOW_COPY_AND_ASSIGN(ScenicWindowCanvas);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_CANVAS_H_
