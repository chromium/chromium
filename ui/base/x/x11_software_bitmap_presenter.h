// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_SOFTWARE_BITMAP_PRESENTER_H_
#define UI_BASE_X_X11_SOFTWARE_BITMAP_PRESENTER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/xproto.h"

class SkCanvas;

namespace x11 {
class Connection;
}

namespace ui {

class XShmImagePool;

class COMPONENT_EXPORT(UI_BASE_X) X11SoftwareBitmapPresenter {
 public:
  // Corresponds to SwapBuffersCallback alias in SoftwareOutputDevice.
  using SwapBuffersCallback = base::OnceCallback<void(const gfx::Size&)>;

  X11SoftwareBitmapPresenter(x11::Connection& connection,
                             gfx::AcceleratedWidget widget,
                             bool enable_multibuffering);

  X11SoftwareBitmapPresenter(const X11SoftwareBitmapPresenter&) = delete;
  X11SoftwareBitmapPresenter& operator=(const X11SoftwareBitmapPresenter&) =
      delete;

  ~X11SoftwareBitmapPresenter();

  void Resize(const gfx::Size& pixel_size);
  SkCanvas* GetSkCanvas();
  void EndPaint(const gfx::Rect& damage_rect);
  void OnSwapBuffers(SwapBuffersCallback swap_ack_callback);
  int MaxFramesPending() const;

 private:
  // Draw |data| over |widget|'s parent-relative background, and write the
  // resulting image to |widget|.  Returns true on success.
  static bool CompositeBitmap(x11::Connection* connection,
                              x11::Drawable widget,
                              int x,
                              int y,
                              int width,
                              int height,
                              int depth,
                              x11::GraphicsContext gc,
                              const void* data);

  bool ShmPoolReady() const;

  x11::Window widget_;
  raw_ref<x11::Connection> connection_;
  x11::GraphicsContext gc_{};
  x11::VisualId visual_{};
  int depth_ = 0;

  const bool enable_multibuffering_;

  // If nonzero, indicates that the widget should be drawn over its
  // parent-relative background.
  uint8_t composite_ = 0;

  std::unique_ptr<ui::XShmImagePool> shm_pool_;
  bool needs_swap_ = false;

  sk_sp<SkSurface> surface_;

  gfx::Size viewport_pixel_size_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_SOFTWARE_BITMAP_PRESENTER_H_
