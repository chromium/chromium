// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_CANVAS_SURFACE_H_
#define UI_OZONE_PLATFORM_X11_X11_CANVAS_SURFACE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/x/x11_software_bitmap_presenter.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

class SkSurface;

namespace ui {

// The platform-specific part of an software output. The class is intended
// for use when no EGL/GLES2 acceleration is possible.
// This class owns any bits that the ozone implementation needs freed when
// the software output is destroyed.
class X11CanvasSurface : public SurfaceOzoneCanvas {
 public:
  explicit X11CanvasSurface(gfx::AcceleratedWidget widget);

  X11CanvasSurface(const X11CanvasSurface&) = delete;
  X11CanvasSurface& operator=(const X11CanvasSurface&) = delete;

  ~X11CanvasSurface() override;

  // SurfaceOzoneCanvas overrides:
  SkCanvas* GetCanvas() override;
  void ResizeCanvas(const gfx::Size& viewport_size, float scale) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;
  bool SupportsAsyncBufferSwap() const override;
  void OnSwapBuffers(SwapBuffersCallback swap_ack_callback,
                     gfx::FrameData data) override;
  int MaxFramesPending() const override;

 private:
  // Current surface we paint to.
  sk_sp<SkSurface> surface_;

  // Helper X11 bitmap presenter that presents the contents.
  X11SoftwareBitmapPresenter x11_software_bitmap_presenter_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_CANVAS_SURFACE_H_
