// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_canvas_surface.h"

#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_xrandr_interval_only_vsync_provider.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gfx/x/connection.h"

namespace ui {

X11CanvasSurface::X11CanvasSurface(gfx::AcceleratedWidget widget)
    : x11_software_bitmap_presenter_(*x11::Connection::Get(), widget, true) {}

X11CanvasSurface::~X11CanvasSurface() = default;

SkCanvas* X11CanvasSurface::GetCanvas() {
  return x11_software_bitmap_presenter_.GetSkCanvas();
}

void X11CanvasSurface::ResizeCanvas(const gfx::Size& viewport_size,
                                    float scale) {
  x11_software_bitmap_presenter_.Resize(viewport_size);
}

void X11CanvasSurface::PresentCanvas(const gfx::Rect& damage) {
  x11_software_bitmap_presenter_.EndPaint(damage);
}

std::unique_ptr<gfx::VSyncProvider> X11CanvasSurface::CreateVSyncProvider() {
  return std::make_unique<XrandrIntervalOnlyVSyncProvider>();
}

bool X11CanvasSurface::SupportsAsyncBufferSwap() const {
  return true;
}

void X11CanvasSurface::OnSwapBuffers(SwapBuffersCallback swap_ack_callback,
                                     gfx::FrameData data) {
  x11_software_bitmap_presenter_.OnSwapBuffers(std::move(swap_ack_callback));
}

int X11CanvasSurface::MaxFramesPending() const {
  return x11_software_bitmap_presenter_.MaxFramesPending();
}

}  // namespace ui
