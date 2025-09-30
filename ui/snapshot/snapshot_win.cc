// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/win/windows_version.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

namespace ui {

namespace {

void GrabHwndSnapshot(HWND window_handle,
                      const gfx::Rect& snapshot_bounds_in_pixels,
                      const gfx::Rect& clip_rect_in_pixels,
                      gfx::Image* image) {
  BOOL result = false;
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
  
  if (base::win::GetVersion() >= base::win::Version::WIN8_1){
	result = PrintWindow(window_handle, mem_hdc, flags);
  }
  else {
	// PrintWindow does not work for pre-Windows 8.1. So we'll use BitBlt.
	// Copying from the window's actual HDC doesn't work so let's just use the full screen HDC.
	// When a snapshot is captured the focus should be on the browser window anyway.
	HDC window_hdc = GetDC(NULL);
	
	RECT window_rect;
	
	memset(&window_rect, 0, sizeof(RECT));
	
	result = GetWindowRect(window_handle, &window_rect);
	
	 if (!result) {
    PLOG(ERROR) << "Failed to get valid rect for snapshot area.";
    return;
    }
	// The left of the snapshot "window" rect is offset by 8 pixels to remove a bit of the dark grey showing through.
	result = BitBlt(mem_hdc, 0, 0, bitmap_size.width(), bitmap_size.height(), 
					window_hdc, window_rect.left + 8, window_rect.top, SRCCOPY);
	
	DeleteDC(window_hdc);
  }
  if (!result) {
    PLOG(ERROR) << "Failed to print window";
    return;
  }

  SkBitmap bitmap;
  bitmap.allocN32Pixels(snapshot_bounds_in_window.width(),
                        snapshot_bounds_in_window.height());
  canvas->readPixels(bitmap, snapshot_bounds_in_window.x(),
                     snapshot_bounds_in_window.y());

  // Clear the region of the bitmap outside the clip rect to white.
  SkCanvas image_canvas(bitmap, SkSurfaceProps{});
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

  return;
}

void GrabNativeWindowSnapshot(gfx::NativeWindow native_window,
                              const gfx::Rect& snapshot_bounds,
                              gfx::Image* image) {
  DCHECK(native_window);
  gfx::Rect window_bounds = native_window->GetBoundsInRootWindow();
  aura::WindowTreeHost* host = native_window->GetHost();
  DCHECK(host);
  HWND hwnd = host->GetAcceleratedWidget();

  gfx::Rect snapshot_bounds_in_pixels =
      host->GetRootTransform().MapRect(snapshot_bounds);
  gfx::Rect expanded_window_bounds_in_pixels =
      host->GetRootTransform().MapRect(window_bounds);
  RECT client_area;
  ::GetClientRect(hwnd, &client_area);
  gfx::Rect client_area_rect(client_area);
  client_area_rect.set_origin(gfx::Point());

  expanded_window_bounds_in_pixels.Intersect(client_area_rect);

  GrabHwndSnapshot(hwnd, snapshot_bounds_in_pixels,
                   expanded_window_bounds_in_pixels, image);
}

}  // namespace

void GrabWindowSnapshot(gfx::NativeWindow window,
                        const gfx::Rect& source_rect,
                        GrabSnapshotImageCallback callback) {
  gfx::Image image;
  GrabNativeWindowSnapshot(window, source_rect, &image);
  std::move(callback).Run(image);
}

void GrabViewSnapshot(gfx::NativeView view,
                      const gfx::Rect& source_rect,
                      GrabSnapshotImageCallback callback) {
  gfx::Image image;
  GrabNativeWindowSnapshot(view, source_rect, &image);
  std::move(callback).Run(image);
}

}  // namespace ui
