// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/window_frame_provider_gtk.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/native_theme/native_theme.h"

namespace gtk {

namespace {

// The maximum reasonable size of the frame edges in DIPs.  If a GTK theme draws
// window decorations larger than this, they will be clipped.
constexpr int kMaxFrameSizeDip = 64;

// The maximum reasonable radius of the frame top corners in DIPs.  If this
// limit is exceeded, the corners will be drawn correctly, but the compositor
// will get an incorrect hint as to which pixels are fully opaque.
constexpr int kMaxCornerRadiusDip = 32;

std::string GetThemeName() {
  gchar* theme = nullptr;
  g_object_get(gtk_settings_get_default(), "gtk-theme-name", &theme, nullptr);
  std::string theme_string;
  if (theme) {
    theme_string = theme;
    g_free(theme);
  }
  return theme_string;
}

GtkCssContext WindowContext(bool solid_frame, bool focused) {
  std::string selector = "#window.background.";
  selector += solid_frame ? "solid-csd" : "csd";
  if (!focused)
    selector += ":inactive";
  return AppendCssNodeToStyleContext({}, selector);
}

GtkCssContext DecorationContext(bool solid_frame, bool focused) {
  auto context = WindowContext(solid_frame, focused);
  context = AppendCssNodeToStyleContext(context, "#decoration");
  if (!focused)
    gtk_style_context_set_state(context, GTK_STATE_FLAG_BACKDROP);

  // The web contents is rendered after the frame border, so remove bottom
  // rounded corners otherwise their borders would get covered up.
  ApplyCssToContext(context, R"(* {
    border-bottom-left-radius: 0;
    border-bottom-right-radius: 0;
  })");

  return context;
}

GtkCssContext HeaderContext(bool solid_frame, bool focused) {
  auto context = WindowContext(solid_frame, focused);
  context =
      AppendCssNodeToStyleContext(context, "#headerbar.header-bar.titlebar");
  if (!focused)
    gtk_style_context_set_state(context, GTK_STATE_FLAG_BACKDROP);
  return context;
}

SkBitmap PaintBitmap(const gfx::Size& bitmap_size,
                     const gfx::Rect& render_bounds,
                     GtkCssContext context,
                     float scale) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(bitmap_size.width(), bitmap_size.height());
  bitmap.eraseColor(SK_ColorTRANSPARENT);

  CairoSurface surface(bitmap);
  cairo_t* cr = surface.cairo();

  auto bounds = render_bounds;

  cairo_scale(cr, scale, scale);
  gtk_render_background(context, cr, bounds.x(), bounds.y(), bounds.width(),
                        bounds.height());
  gtk_render_frame(context, cr, bounds.x(), bounds.y(), bounds.width(),
                   bounds.height());

  bitmap.notifyPixelsChanged();
  bitmap.setImmutable();
  return bitmap;
}

SkBitmap PaintHeaderbar(const gfx::Size& size,
                        GtkCssContext context,
                        float scale) {
  gfx::Rect tabstrip_bounds_dip(0, 0, size.width() / scale,
                                size.height() / scale);
  return PaintBitmap(size, tabstrip_bounds_dip, context, scale);
}

int ComputeTopCornerRadius(float scale) {
  // In GTK4, there's no way to directly obtain CSS values for a context, so we
  // need to experimentally determine the corner radius by rendering a sample.
  auto context = HeaderContext(false, false);
  ApplyCssToContext(context, R"(headerbar {
    background-image: none;
    background-color: black;
    box-shadow: none;
    border: none;
    border-bottom-left-radius: 0;
    border-bottom-right-radius: 0;
    border-top-right-radius: 0;
  })");
  auto bitmap = PaintHeaderbar({kMaxCornerRadiusDip, kMaxCornerRadiusDip},
                               context, scale);
  DCHECK_EQ(bitmap.width(), bitmap.height());
  for (int i = 0; i < bitmap.width(); ++i) {
    if (SkColorGetA(bitmap.getColor(0, i)) == 255 &&
        SkColorGetA(bitmap.getColor(i, 0)) == 255) {
      return i;
    }
  }
  return bitmap.width();
}

}  // namespace

WindowFrameProviderGtk::WindowFrameProviderGtk(bool solid_frame)
    : solid_frame_(solid_frame) {}

WindowFrameProviderGtk::~WindowFrameProviderGtk() = default;

int WindowFrameProviderGtk::GetTopCornerRadius() {
  MaybeUpdateBitmaps();
  return top_corner_radius_;
}

gfx::Insets WindowFrameProviderGtk::GetFrameThickness() {
  MaybeUpdateBitmaps();
  return frame_thickness_dip_;
}

void WindowFrameProviderGtk::PaintWindowFrame(gfx::Canvas* canvas,
                                              const gfx::Rect& rect_dip,
                                              int top_area_height_dip,
                                              bool focused) {
  MaybeUpdateBitmaps();

  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();

  auto client_bounds_px = gfx::ScaleToRoundedRect(rect_dip, scale);
  client_bounds_px.Inset(frame_thickness_px_);

  gfx::Rect src_rect(gfx::Size(BitmapSizePx(), BitmapSizePx()));
  src_rect.Inset(gfx::Insets(frame_size_px_) - frame_thickness_px_);

  auto corner_w = std::min(frame_size_px_, client_bounds_px.width() / 2);
  auto corner_h = std::min(frame_size_px_, client_bounds_px.height() / 2);
  auto edge_w = client_bounds_px.width() - 2 * corner_w;
  auto edge_h = client_bounds_px.height() - 2 * corner_h;

  auto corner_insets = frame_thickness_px_ + gfx::Insets(corner_h, corner_w);

  auto image = gfx::ImageSkia::CreateFrom1xBitmap(focused ? focused_bitmap_
                                                          : unfocused_bitmap_);

  // Top left corner
  canvas->DrawImageInt(image, src_rect.x(), src_rect.y(), corner_insets.left(),
                       corner_insets.top(), 0, 0, corner_insets.left(),
                       corner_insets.top(), false);
  // Top right corner
  canvas->DrawImageInt(image, BitmapSizePx() - frame_size_px_ - corner_w,
                       src_rect.y(), corner_insets.right(), corner_insets.top(),
                       client_bounds_px.right() - corner_w, 0,
                       corner_insets.right(), corner_insets.top(), false);
  // Bottom left corner
  canvas->DrawImageInt(image, src_rect.x(),
                       BitmapSizePx() - frame_size_px_ - corner_h,
                       corner_insets.left(), corner_insets.bottom(), 0,
                       client_bounds_px.bottom() - corner_h,
                       corner_insets.left(), corner_insets.bottom(), false);
  // Bottom right corner
  canvas->DrawImageInt(image, BitmapSizePx() - frame_size_px_ - corner_w,
                       BitmapSizePx() - frame_size_px_ - corner_h,
                       corner_insets.right(), corner_insets.bottom(),
                       client_bounds_px.right() - corner_w,
                       client_bounds_px.bottom() - corner_h,
                       corner_insets.right(), corner_insets.bottom(), false);
  // Top edge
  canvas->DrawImageInt(image, 2 * frame_size_px_, src_rect.y(), 1,
                       frame_thickness_px_.top(), corner_insets.left(), 0,
                       edge_w, frame_thickness_px_.top(), false);
  // Left edge
  canvas->DrawImageInt(image, src_rect.x(), 2 * frame_size_px_,
                       frame_thickness_px_.left(), 1, 0, corner_insets.top(),
                       frame_thickness_px_.left(), edge_h, false);
  // Bottom edge
  canvas->DrawImageInt(
      image, 2 * frame_size_px_, BitmapSizePx() - frame_size_px_, 1,
      frame_thickness_px_.bottom(), corner_insets.left(),
      client_bounds_px.bottom(), edge_w, frame_thickness_px_.bottom(), false);
  // Right edge
  canvas->DrawImageInt(image, BitmapSizePx() - frame_size_px_,
                       2 * frame_size_px_, frame_thickness_px_.right(), 1,
                       client_bounds_px.right(), corner_insets.top(),
                       frame_thickness_px_.right(), edge_h, false);

  int top_area_height_px =
      top_area_height_dip * scale - frame_thickness_px_.top();

  auto header = PaintHeaderbar({client_bounds_px.width(), top_area_height_px},
                               HeaderContext(solid_frame_, focused), scale);
  canvas->DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(header), 0, 0,
                       header.width(), header.height(), client_bounds_px.x(),
                       client_bounds_px.y(), header.width(), header.height(),
                       false);
}

void WindowFrameProviderGtk::MaybeUpdateBitmaps() {
  auto scale = GetDeviceScaleFactor();
  std::string theme_name = GetThemeName();
  if (scale_ == scale && theme_name_ == theme_name)
    return;
  scale_ = scale;
  theme_name_ = theme_name;

  frame_size_px_ = std::ceil(kMaxFrameSizeDip * scale);
  if (!solid_frame_)
    top_corner_radius_ = ComputeTopCornerRadius(scale);

  gfx::Rect frame_bounds_dip(kMaxFrameSizeDip, kMaxFrameSizeDip,
                             2 * kMaxFrameSizeDip, 2 * kMaxFrameSizeDip);
  auto focused_context = DecorationContext(solid_frame_, true);
  frame_bounds_dip.Inset(-GtkStyleContextGetPadding(focused_context));
  frame_bounds_dip.Inset(-GtkStyleContextGetBorder(focused_context));
  gfx::Size bitmap_size(BitmapSizePx(), BitmapSizePx());
  focused_bitmap_ =
      PaintBitmap(bitmap_size, frame_bounds_dip, focused_context, scale);
  unfocused_bitmap_ =
      PaintBitmap(bitmap_size, frame_bounds_dip,
                  DecorationContext(solid_frame_, false), scale);

  // In GTK4, there's no way to obtain the frame thickness from CSS values
  // directly, so we must determine it experimentally based on the drawn
  // bitmaps.
  auto get_inset = [&](auto&& pixel_iterator) -> int {
    for (int i = 0; i < frame_size_px_; ++i) {
      if (SkColorGetA(pixel_iterator(i))) {
        int inset_px = frame_size_px_ - i;
        return std::ceil(inset_px / scale);
      }
    }
    return 0;
  };
  frame_thickness_dip_ = gfx::Insets(
      // top
      get_inset([&](int i) {
        return focused_bitmap_.getColor(2 * frame_size_px_, i);
      }),
      // left
      get_inset([&](int i) {
        return focused_bitmap_.getColor(i, 2 * frame_size_px_);
      }),
      // bottom
      get_inset([&](int i) {
        return focused_bitmap_.getColor(2 * frame_size_px_,
                                        BitmapSizePx() - i - 1);
      }),
      // right
      get_inset([&](int i) {
        return focused_bitmap_.getColor(BitmapSizePx() - i - 1,
                                        2 * frame_size_px_);
      }));
  frame_thickness_px_ = gfx::ScaleToRoundedInsets(frame_thickness_dip_, scale);
}

int WindowFrameProviderGtk::BitmapSizePx() const {
  // The window decoration will be rendered into a square with this side length.
  // The left and right sides of the decoration add 2 * kMaxDecorationThickness,
  // and the window itself has size 2 * kMaxDecorationThickness.
  return 4 * frame_size_px_;
}

}  // namespace gtk
