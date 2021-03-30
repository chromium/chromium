// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_software_bitmap_presenter.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cstring>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/x/x11_shm_image_pool.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_types.h"
#include "ui/gfx/x/xproto_util.h"

namespace ui {

namespace {

constexpr int kMaxFramesPending = 2;

class ScopedPixmap {
 public:
  ScopedPixmap(x11::Connection* connection, x11::Pixmap pixmap)
      : connection_(connection), pixmap_(pixmap) {}

  ~ScopedPixmap() {
    if (pixmap_ != x11::Pixmap::None)
      connection_->FreePixmap({pixmap_});
  }

 private:
  x11::Connection* const connection_;
  x11::Pixmap pixmap_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPixmap);
};

}  // namespace

// static
bool X11SoftwareBitmapPresenter::CompositeBitmap(x11::Connection* connection,
                                                 x11::Drawable widget,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 int depth,
                                                 x11::GraphicsContext gc,
                                                 const void* data) {
  connection->ClearArea({false, widget, x, y, width, height});

  constexpr auto kAllPlanes =
      std::numeric_limits<decltype(x11::GetImageRequest::plane_mask)>::max();

  scoped_refptr<base::RefCountedMemory> bg;
  auto req = connection->GetImage(
      {x11::ImageFormat::ZPixmap, widget, x, y, width, height, kAllPlanes});
  if (auto reply = req.Sync()) {
    bg = reply->data;
  } else {
    auto pixmap_id = connection->GenerateId<x11::Pixmap>();
    connection->CreatePixmap({depth, pixmap_id, widget, width, height});
    ScopedPixmap pixmap(connection, pixmap_id);

    connection->ChangeGC(x11::ChangeGCRequest{
        .gc = gc, .subwindow_mode = x11::SubwindowMode::IncludeInferiors});
    connection->CopyArea({widget, pixmap_id, gc, x, y, 0, 0, width, height});
    connection->ChangeGC(x11::ChangeGCRequest{
        .gc = gc, .subwindow_mode = x11::SubwindowMode::ClipByChildren});

    auto req = connection->GetImage({x11::ImageFormat::ZPixmap, pixmap_id, 0, 0,
                                     width, height, kAllPlanes});
    if (auto reply = req.Sync())
      bg = reply->data;
    else
      return false;
  }

  SkBitmap bg_bitmap;
  SkImageInfo image_info = SkImageInfo::Make(
      width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  if (!bg_bitmap.installPixels(image_info, const_cast<uint8_t*>(bg->data()),
                               image_info.minRowBytes())) {
    return false;
  }
  SkCanvas canvas(bg_bitmap);

  SkBitmap fg_bitmap;
  image_info = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType,
                                 kPremul_SkAlphaType);
  if (!fg_bitmap.installPixels(image_info, const_cast<void*>(data), 4 * width))
    return false;
  canvas.drawImage(fg_bitmap.asImage(), 0, 0);
  canvas.flush();

  connection->PutImage({x11::ImageFormat::ZPixmap, widget, gc, width, height, x,
                        y, 0, depth, bg});

  return true;
}

X11SoftwareBitmapPresenter::X11SoftwareBitmapPresenter(
    x11::Connection* connection,
    gfx::AcceleratedWidget widget,
    bool enable_multibuffering)
    : widget_(static_cast<x11::Window>(widget)),
      connection_(connection),
      enable_multibuffering_(enable_multibuffering) {
  DCHECK_NE(widget_, x11::Window::None);

  gc_ = connection_->GenerateId<x11::GraphicsContext>();
  connection_->CreateGC({gc_, widget_});

  if (auto response = connection_->GetWindowAttributes({widget_}).Sync()) {
    visual_ = response->visual;
    depth_ = connection_->GetVisualInfoFromId(visual_)->format->depth;
  } else {
    LOG(ERROR) << "XGetWindowAttributes failed for window "
               << static_cast<uint32_t>(widget_);
    return;
  }

  shm_pool_ = std::make_unique<ui::XShmImagePool>(connection_, widget_, visual_,
                                                  depth_, MaxFramesPending(),
                                                  enable_multibuffering_);

  // TODO(thomasanderson): Avoid going through the X11 server to plumb this
  // property in.
  GetProperty(widget_, x11::GetAtom("CHROMIUM_COMPOSITE_WINDOW"), &composite_);
}

X11SoftwareBitmapPresenter::~X11SoftwareBitmapPresenter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gc_ != x11::GraphicsContext{})
    connection_->FreeGC({gc_});
}

bool X11SoftwareBitmapPresenter::ShmPoolReady() const {
  return shm_pool_ && shm_pool_->Ready();
}

void X11SoftwareBitmapPresenter::Resize(const gfx::Size& pixel_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pixel_size == viewport_pixel_size_)
    return;
  viewport_pixel_size_ = pixel_size;
  // Fallback to the non-shm codepath when |composite_| is true, which only
  // happens for status icon windows that are typically 16x16px.  It's possible
  // to add a shm codepath, but it wouldn't be buying much since it would only
  // affect windows that are tiny and infrequently updated.
  if (!composite_ && shm_pool_ && shm_pool_->Resize(pixel_size)) {
    needs_swap_ = false;
    surface_ = nullptr;
  } else {
    SkColorType color_type = ColorTypeForVisual(visual_);
    if (color_type == kUnknown_SkColorType)
      return;
    SkImageInfo info = SkImageInfo::Make(viewport_pixel_size_.width(),
                                         viewport_pixel_size_.height(),
                                         color_type, kOpaque_SkAlphaType);
    SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
    surface_ = SkSurface::MakeRaster(info, &props);
  }
}

SkCanvas* X11SoftwareBitmapPresenter::GetSkCanvas() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShmPoolReady())
    return shm_pool_->CurrentCanvas();
  else if (surface_)
    return surface_->getCanvas();
  return nullptr;
}

void X11SoftwareBitmapPresenter::EndPaint(const gfx::Rect& damage_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gfx::Rect rect = damage_rect;
  rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (rect.IsEmpty())
    return;

  SkPixmap skia_pixmap;

  if (ShmPoolReady()) {
    // TODO(thomasanderson): Investigate direct rendering with DRI3 to avoid any
    // unnecessary X11 IPC or buffer copying.
    x11::Shm::PutImageRequest put_image_request{
        .drawable = widget_,
        .gc = gc_,
        .total_width = shm_pool_->CurrentBitmap().width(),
        .total_height = shm_pool_->CurrentBitmap().height(),
        .src_x = rect.x(),
        .src_y = rect.y(),
        .src_width = rect.width(),
        .src_height = rect.height(),
        .dst_x = rect.x(),
        .dst_y = rect.y(),
        .depth = depth_,
        .format = x11::ImageFormat::ZPixmap,
        .send_event = enable_multibuffering_,
        .shmseg = shm_pool_->CurrentSegment(),
        .offset = 0,
    };
    connection_->shm().PutImage(put_image_request);
    needs_swap_ = true;
    // Flush now to ensure the X server gets the request as early as
    // possible to reduce frame-to-frame latency.
    connection_->Flush();
    return;
  }
  if (surface_)
    surface_->peekPixels(&skia_pixmap);

  if (!skia_pixmap.addr())
    return;

  if (composite_ &&
      CompositeBitmap(connection_, widget_, rect.x(), rect.y(), rect.width(),
                      rect.height(), depth_, gc_, skia_pixmap.addr())) {
    // Flush now to ensure the X server gets the request as early as
    // possible to reduce frame-to-frame latency.

    connection_->Flush();
    return;
  }

  auto* connection = x11::Connection::Get();
  DrawPixmap(connection, visual_, widget_, gc_, skia_pixmap, rect.x(), rect.y(),
             rect.x(), rect.y(), rect.width(), rect.height());

  // Flush now to ensure the X server gets the request as early as
  // possible to reduce frame-to-frame latency.
  connection_->Flush();
}

void X11SoftwareBitmapPresenter::OnSwapBuffers(
    SwapBuffersCallback swap_ack_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (enable_multibuffering_ && ShmPoolReady() && needs_swap_)
    shm_pool_->SwapBuffers(std::move(swap_ack_callback));
  else
    std::move(swap_ack_callback).Run(viewport_pixel_size_);
  needs_swap_ = false;
}

int X11SoftwareBitmapPresenter::MaxFramesPending() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enable_multibuffering_ ? kMaxFramesPending : 1;
}

}  // namespace ui
