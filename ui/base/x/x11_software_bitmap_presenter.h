// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_SOFTWARE_BITMAP_PRESENTER_H_
#define UI_BASE_X_X11_SOFTWARE_BITMAP_PRESENTER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

class SkCanvas;

namespace base {
class TaskRunner;
}

namespace ui {

class XShmImagePoolBase;

class COMPONENT_EXPORT(UI_BASE_X) X11SoftwareBitmapPresenter {
 public:
  // Corresponds to SwapBuffersCallback alias in SoftwareOutputDevice.
  using SwapBuffersCallback = base::OnceCallback<void(const gfx::Size&)>;

  X11SoftwareBitmapPresenter(gfx::AcceleratedWidget widget,
                             base::TaskRunner* host_task_runner,
                             base::TaskRunner* event_task_runner);

  ~X11SoftwareBitmapPresenter();

  void Resize(const gfx::Size& pixel_size);
  SkCanvas* GetSkCanvas();
  void EndPaint(const gfx::Rect& damage_rect);
  void OnSwapBuffers(SwapBuffersCallback swap_ack_callback);
  int MaxFramesPending() const;

 private:
  // Draw |data| over |widget|'s parent-relative background, and write the
  // resulting image to |widget|.  Returns true on success.
  static bool CompositeBitmap(XDisplay* display,
                              XID widget,
                              int x,
                              int y,
                              int width,
                              int height,
                              int depth,
                              GC gc,
                              const void* data);

  bool ShmPoolReady() const;

  gfx::AcceleratedWidget widget_;
  XDisplay* display_;
  GC gc_;
  XWindowAttributes attributes_;

  // If nonzero, indicates that the widget should be drawn over its
  // parent-relative background.
  int composite_ = 0;

  scoped_refptr<ui::XShmImagePoolBase> shm_pool_;
  bool needs_swap_ = false;

  base::TaskRunner* host_task_runner_;
  sk_sp<SkSurface> surface_;

  gfx::Size viewport_pixel_size_;

  DISALLOW_COPY_AND_ASSIGN(X11SoftwareBitmapPresenter);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_SOFTWARE_BITMAP_PRESENTER_H_
