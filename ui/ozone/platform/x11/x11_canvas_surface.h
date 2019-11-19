// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_CANVAS_SURFACE_H_
#define UI_OZONE_PLATFORM_X11_X11_CANVAS_SURFACE_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/x/x11_software_bitmap_presenter.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

// The platform-specific part of an software output. The class is intended
// for use when no EGL/GLES2 acceleration is possible.
// This class owns any bits that the ozone implementation needs freed when
// the software output is destroyed.
class X11CanvasSurface : public SurfaceOzoneCanvas {
 public:
  X11CanvasSurface(gfx::AcceleratedWidget widget,
                   base::TaskRunner* gpu_task_runner);
  ~X11CanvasSurface() override;

  // SurfaceOzoneCanvas overrides:
  sk_sp<SkSurface> GetSurface() override;
  void ResizeCanvas(const gfx::Size& viewport_size) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;
  bool SupportsAsyncBufferSwap() const override;
  void OnSwapBuffers(SwapBuffersCallback swap_ack_callback) override;
  int MaxFramesPending() const override;

 private:
  // Current surface we paint to.
  sk_sp<SkSurface> surface_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Helper X11 bitmap presenter that presents the contents.
  X11SoftwareBitmapPresenter x11_software_bitmap_presenter_;

  DISALLOW_COPY_AND_ASSIGN(X11CanvasSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_CANVAS_SURFACE_H_
