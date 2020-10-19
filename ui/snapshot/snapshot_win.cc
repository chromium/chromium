// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot_win.h"

#include <memory>

#include "base/callback.h"
#include "base/win/windows_version.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"
#include "ui/snapshot/snapshot_aura.h"

namespace {

// Windows 8.1 is the first version that supports PW_RENDERFULLCONTENT.
// Without that flag PrintWindow may not correctly capture what's actually
// onscreen.
bool UseAuraSnapshot() {
  return (base::win::GetVersion() < base::win::Version::WIN8_1);
}

}  // namespace

namespace ui {

namespace internal {

bool GrabHwndSnapshot(HWND window_handle,
                      const gfx::Rect& snapshot_bounds_in_pixels,
                      const gfx::Rect& clip_rect_in_pixels,
                      gfx::Image* image) {
  gfx::Rect snapshot_bounds_in_window =
      snapshot_bounds_in_pixels + clip_rect_in_pixels.OffsetFromOrigin();
  gfx::Size bitmap_size(snapshot_bounds_in_window.right(),
                        snapshot_bounds_in_window.bottom());

  std::unique_ptr<SkCanvas> canvas = skia::CreatePlatformCanvas(
      bitmap_size.width(), bitmap_size.height(), false);
  HDC mem_hdc = skia::GetNativeDrawingContext(canvas.get());

  // Grab a copy of the window. Use PrintWindow because it works even when the
  // window's partially occluded. The PW_RENDERFULLCONTENT flag is undocumented,
  // but works starting in Windows 8.1. It allows for capturing the contents of
  // the window that are drawn using DirectComposition.
  UINT flags = PW_CLIENTONLY | PW_RENDERFULLCONTENT;

  BOOL result = PrintWindow(window_handle, mem_hdc, flags);
  if (!result) {
    PLOG(ERROR) << "Failed to print window";
    return false;
  }

  SkBitmap bitmap;
  bitmap.allocN32Pixels(snapshot_bounds_in_window.width(),
                        snapshot_bounds_in_window.height());
  canvas->readPixels(bitmap, snapshot_bounds_in_window.x(),
                     snapshot_bounds_in_window.y());

  // Clear the region of the bitmap outside the clip rect to white.
  SkCanvas image_canvas(bitmap);
  SkPaint paint;
  paint.setColor(SK_ColorWHITE);

  SkRegion region;
  gfx::Rect clip_in_bitmap(clip_rect_in_pixels.size());
  clip_in_bitmap.Offset(-snapshot_bounds_in_pixels.OffsetFromOrigin());
  region.setRect(
      gfx::RectToSkIRect(gfx::Rect(snapshot_bounds_in_pixels.size())));
  region.op(gfx::RectToSkIRect(clip_in_bitmap), SkRegion::kDifference_Op);
  image_canvas.drawRegion(region, paint);

  *image = gfx::Image::CreateFrom1xBitmap(bitmap);

  return true;
}

}  // namespace internal

bool GrabViewSnapshot(gfx::NativeView view_handle,
                      const gfx::Rect& snapshot_bounds,
                      gfx::Image* image) {
  return GrabWindowSnapshot(view_handle, snapshot_bounds, image);
}

bool GrabWindowSnapshot(gfx::NativeWindow window_handle,
                        const gfx::Rect& snapshot_bounds,
                        gfx::Image* image) {
  if (UseAuraSnapshot()) {
    // Not supported in Aura.  Callers should fall back to the async version.
    return false;
  }

  DCHECK(window_handle);
  gfx::Rect window_bounds = window_handle->GetBoundsInRootWindow();
  aura::WindowTreeHost* host = window_handle->GetHost();
  DCHECK(host);
  HWND hwnd = host->GetAcceleratedWidget();

  gfx::RectF window_bounds_in_pixels(window_bounds);
  host->GetRootTransform().TransformRect(&window_bounds_in_pixels);
  gfx::RectF snapshot_bounds_in_pixels(snapshot_bounds);
  host->GetRootTransform().TransformRect(&snapshot_bounds_in_pixels);

  gfx::Rect expanded_window_bounds_in_pixels =
      gfx::ToEnclosingRect(window_bounds_in_pixels);
  RECT client_area;
  ::GetClientRect(hwnd, &client_area);
  gfx::Rect client_area_rect(client_area);
  client_area_rect.set_origin(gfx::Point());

  expanded_window_bounds_in_pixels.Intersect(client_area_rect);

  return internal::GrabHwndSnapshot(
      hwnd, gfx::ToEnclosingRect(snapshot_bounds_in_pixels),
      expanded_window_bounds_in_pixels, image);
}

void GrabWindowSnapshotAsync(gfx::NativeWindow window,
                             const gfx::Rect& source_rect,
                             GrabWindowSnapshotAsyncCallback callback) {
  if (UseAuraSnapshot()) {
    GrabWindowSnapshotAsyncAura(window, source_rect, std::move(callback));
    return;
  }
  gfx::Image image;
  GrabWindowSnapshot(window, source_rect, &image);
  std::move(callback).Run(image);
}

void GrabViewSnapshotAsync(gfx::NativeView view,
                           const gfx::Rect& source_rect,
                           GrabWindowSnapshotAsyncCallback callback) {
  if (UseAuraSnapshot()) {
    GrabWindowSnapshotAsyncAura(view, source_rect, std::move(callback));
    return;
  }
  NOTIMPLEMENTED();
  std::move(callback).Run(gfx::Image());
}

void GrabWindowSnapshotAndScaleAsync(gfx::NativeWindow window,
                                     const gfx::Rect& source_rect,
                                     const gfx::Size& target_size,
                                     GrabWindowSnapshotAsyncCallback callback) {
  if (UseAuraSnapshot()) {
    GrabWindowSnapshotAndScaleAsyncAura(window, source_rect, target_size,
                                        std::move(callback));
    return;
  }
  NOTIMPLEMENTED();
  std::move(callback).Run(gfx::Image());
}

}  // namespace ui
