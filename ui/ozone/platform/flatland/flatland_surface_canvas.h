// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SURFACE_CANVAS_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SURFACE_CANVAS_H_

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/flatland/flatland_connection.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace mojo {
class PlatformHandle;
}

namespace ui {

// SurfaceOzoneCanvas implementation for FlatlandWindow.
class FlatlandSurfaceCanvas : public SurfaceOzoneCanvas {
 public:
  FlatlandSurfaceCanvas(
      fuchsia::sysmem2::Allocator_Sync* sysmem_allocator,
      fuchsia::ui::composition::Allocator* flatland_allocator);

  FlatlandSurfaceCanvas(const FlatlandSurfaceCanvas&) = delete;
  FlatlandSurfaceCanvas& operator=(const FlatlandSurfaceCanvas&) = delete;

  ~FlatlandSurfaceCanvas() override;

  mojo::PlatformHandle CreateView();

  // SurfaceOzoneCanvas implementation.
  void ResizeCanvas(const gfx::Size& viewport_size, float scale) override;
  SkCanvas* GetCanvas() override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  // Use 3 buffers for optimal performance. One for the current visible frame,
  // one for the next frame to be displayed, one to render the following frame.
  static const size_t kNumBuffers = 3;

  struct Frame {
    Frame();
    ~Frame();

    // Sets the `vmo` to use for this frame.
    void InitializeBuffer(fuchsia::sysmem2::VmoBuffer vmo,
                          gfx::Size size,
                          size_t stride);

    // Reset the state and release the buffer.
    void Reset();

    // Copies pixels covered by |dirty_region| from another |frame|.
    void CopyDirtyRegionFrom(const Frame& frame);

    bool is_empty() { return !surface; }

    // Shared memory for the buffer.
    base::WritableSharedMemoryMapping memory_mapping;

    // SkSurface that wraps |memory_mapping|.
    sk_sp<SkSurface> surface;

    fuchsia::ui::composition::ContentId image_id = {};

    // Fence that will be released by Scenic when it stops using this frame.
    zx::event release_fence;

    // The region of the frame that's not up-to-date.
    SkRegion dirty_region;
  };

  class VSyncProviderImpl;

  void FinalizeBufferAllocation();
  void OnFramePresented(base::TimeTicks actual_presentation_time,
                        base::TimeDelta future_presentation_interval);

  void OnFlatlandError(fuchsia::ui::composition::FlatlandError error);

  fuchsia::sysmem2::Allocator_Sync* const sysmem_allocator_;
  fuchsia::ui::composition::Allocator* const flatland_allocator_;

  FlatlandConnection flatland_;

  Frame frames_[kNumBuffers];

  // Buffer index in |frames_| for the frame that's currently being rendered.
  size_t current_frame_ = 0;

  // View size in device pixels.
  gfx::Size viewport_size_;

  fuchsia::ui::composition::ParentViewportWatcherPtr parent_viewport_watcher_;
  fuchsia::ui::composition::TransformId root_transform_id_;

  // Pending buffer collection for the buffers.
  fuchsia::sysmem2::BufferCollectionSyncPtr buffer_collection_;
  fuchsia::ui::composition::BufferCollectionImportToken import_token_;

  base::WeakPtr<VSyncProviderImpl> vsync_provider_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SURFACE_CANVAS_H_
