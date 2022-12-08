// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/window_frame_provider_gtk.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
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
  // GTK4 renders the decoration directly on the window.
  if (!GtkCheckVersion(4))
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

int ComputeTopCornerRadius() {
  // In GTK4, there's no way to directly obtain CSS values for a context, so we
  // need to experimentally determine the corner radius by rendering a sample.
  // Additionally, in GTK4, the headerbar corners get clipped by the window
  // rather than the headerbar having its own rounded corners.
  auto context = GtkCheckVersion(4) ? DecorationContext(false, false)
                                    : HeaderContext(false, false);
  ApplyCssToContext(context, R"(window, headerbar {
    background-image: none;
    background-color: black;
    box-shadow: none;
    border: none;
    border-bottom-left-radius: 0;
    border-bottom-right-radius: 0;
    border-top-right-radius: 0;
  })");
  gfx::Size size_dip{kMaxCornerRadiusDip, kMaxCornerRadiusDip};
  auto bitmap = GtkCheckVersion(4)
                    ? PaintBitmap(size_dip, {{0, 0}, size_dip}, context, 1)
                    : PaintHeaderbar(size_dip, context, 1);
  DCHECK_EQ(bitmap.width(), bitmap.height());
  for (int i = 0; i < bitmap.width(); ++i) {
    if (SkColorGetA(bitmap.getColor(0, i)) == 255 &&
        SkColorGetA(bitmap.getColor(i, 0)) == 255) {
      return i;
    }
  }
  return bitmap.width();
}

// Returns int(scale * 100), which essentially limits the scale to fractions of
// 100 and secures from rounding errors.
int ToRoundedScale(float scale) {
  return round(scale * 100);
}

}  // namespace

WindowFrameProviderGtk::Asset::Asset() = default;

WindowFrameProviderGtk::Asset::Asset(const WindowFrameProviderGtk::Asset& src) {
  CloneFrom(src);
}

WindowFrameProviderGtk::Asset& WindowFrameProviderGtk::Asset::operator=(
    const WindowFrameProviderGtk::Asset& src) {
  CloneFrom(src);
  return *this;
}

WindowFrameProviderGtk::Asset::~Asset() = default;

void WindowFrameProviderGtk::Asset::CloneFrom(
    const WindowFrameProviderGtk::Asset& src) {
  valid = src.valid;
  if (!valid)
    return;

  frame_size_px = src.frame_size_px;
  frame_thickness_px = src.frame_thickness_px;
  focused_bitmap = src.focused_bitmap;
  unfocused_bitmap = src.unfocused_bitmap;
}

WindowFrameProviderGtk::WindowFrameProviderGtk(bool solid_frame)
    : solid_frame_(solid_frame) {}

WindowFrameProviderGtk::~WindowFrameProviderGtk() = default;

int WindowFrameProviderGtk::GetTopCornerRadiusDip() {
  MaybeUpdateBitmaps(GetDeviceScaleFactor());
  return top_corner_radius_dip_;
}

gfx::Insets WindowFrameProviderGtk::GetFrameThicknessDip() {
  MaybeUpdateBitmaps(GetDeviceScaleFactor());
  return frame_thickness_dip_;
}

void WindowFrameProviderGtk::PaintWindowFrame(
    gfx::Canvas* canvas,
    const gfx::Rect& rect_dip,
    int top_area_height_dip,
    bool focused,
    ui::WindowTiledEdges tiled_edges) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();

  MaybeUpdateBitmaps(scale);

  const auto& asset = assets_[ToRoundedScale(scale)];
  DCHECK(asset.valid);

  auto client_bounds_px = gfx::ScaleToRoundedRect(rect_dip, scale);
  const auto effective_frame_thickness_px = gfx::Insets::TLBR(
      tiled_edges.top ? 0 : asset.frame_thickness_px.top(),
      tiled_edges.left ? 0 : asset.frame_thickness_px.left(),
      tiled_edges.bottom ? 0 : asset.frame_thickness_px.bottom(),
      tiled_edges.right ? 0 : asset.frame_thickness_px.right());
  client_bounds_px.Inset(effective_frame_thickness_px);

  gfx::Rect src_rect(gfx::Size(BitmapSizePx(asset), BitmapSizePx(asset)));
  src_rect.Inset(gfx::Insets(asset.frame_size_px) -
                 effective_frame_thickness_px);

  auto corner_w = std::min(asset.frame_size_px, client_bounds_px.width() / 2);
  auto corner_h = std::min(asset.frame_size_px, client_bounds_px.height() / 2);
  auto edge_w = client_bounds_px.width() - 2 * corner_w;
  auto edge_h = client_bounds_px.height() - 2 * corner_h;

  auto corner_insets =
      effective_frame_thickness_px + gfx::Insets::VH(corner_h, corner_w);

  auto image = gfx::ImageSkia::CreateFrom1xBitmap(
      focused ? asset.focused_bitmap : asset.unfocused_bitmap);

  auto draw_image = [&](int src_x, int src_y, int src_w, int src_h, int dst_x,
                        int dst_y, int dst_w, int dst_h) {
    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0)
      return;
    canvas->DrawImageInt(image, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w,
                         dst_h, false);
  };

  // Top left corner
  draw_image(src_rect.x(), src_rect.y(), corner_insets.left(),
             corner_insets.top(), 0, 0, corner_insets.left(),
             corner_insets.top());
  // Top right corner
  draw_image(BitmapSizePx(asset) - asset.frame_size_px - corner_w, src_rect.y(),
             corner_insets.right(), corner_insets.top(),
             client_bounds_px.right() - corner_w, 0, corner_insets.right(),
             corner_insets.top());
  // Bottom left corner
  draw_image(src_rect.x(), BitmapSizePx(asset) - asset.frame_size_px - corner_h,
             corner_insets.left(), corner_insets.bottom(), 0,
             client_bounds_px.bottom() - corner_h, corner_insets.left(),
             corner_insets.bottom());
  // Bottom right corner
  draw_image(BitmapSizePx(asset) - asset.frame_size_px - corner_w,
             BitmapSizePx(asset) - asset.frame_size_px - corner_h,
             corner_insets.right(), corner_insets.bottom(),
             client_bounds_px.right() - corner_w,
             client_bounds_px.bottom() - corner_h, corner_insets.right(),
             corner_insets.bottom());
  // Top edge
  draw_image(2 * asset.frame_size_px, src_rect.y(), 1,
             effective_frame_thickness_px.top(), corner_insets.left(), 0,
             edge_w, effective_frame_thickness_px.top());
  // Left edge
  draw_image(src_rect.x(), 2 * asset.frame_size_px,
             effective_frame_thickness_px.left(), 1, 0, corner_insets.top(),
             effective_frame_thickness_px.left(), edge_h);
  // Bottom edge
  draw_image(2 * asset.frame_size_px, BitmapSizePx(asset) - asset.frame_size_px,
             1, effective_frame_thickness_px.bottom(), corner_insets.left(),
             client_bounds_px.bottom(), edge_w,
             effective_frame_thickness_px.bottom());
  // Right edge
  draw_image(BitmapSizePx(asset) - asset.frame_size_px, 2 * asset.frame_size_px,
             effective_frame_thickness_px.right(), 1, client_bounds_px.right(),
             corner_insets.top(), effective_frame_thickness_px.right(), edge_h);

  int top_area_height_px =
      top_area_height_dip * scale - effective_frame_thickness_px.top();

  auto header = PaintHeaderbar({client_bounds_px.width(), top_area_height_px},
                               HeaderContext(solid_frame_, focused), scale);
  image = gfx::ImageSkia::CreateFrom1xBitmap(header);
  // In GTK4, the headerbar gets clipped by the window.
  if (GtkCheckVersion(4)) {
    gfx::RectF bounds_px =
        gfx::RectF(client_bounds_px.x(), client_bounds_px.y(), header.width(),
                   header.height());
    float radius_px = scale * top_corner_radius_dip_;
    SkVector radii[4]{{radius_px, radius_px}, {radius_px, radius_px}, {}, {}};
    SkRRect clip;
    clip.setRectRadii(gfx::RectFToSkRect(bounds_px), radii);
    canvas->sk_canvas()->clipRRect(clip, SkClipOp::kIntersect, true);
  }
  draw_image(0, 0, header.width(), header.height(), client_bounds_px.x(),
             client_bounds_px.y(), header.width(), header.height());
}

void WindowFrameProviderGtk::MaybeUpdateBitmaps(float scale) {
  std::string theme_name = GetThemeName();
  if (theme_name_ != theme_name) {
    assets_.clear();
    theme_name_ = theme_name;
  }

  auto& asset = assets_[ToRoundedScale(scale)];
  if (asset.valid)
    return;

  asset.frame_size_px = std::ceil(kMaxFrameSizeDip * scale);

  gfx::Rect frame_bounds_dip(kMaxFrameSizeDip, kMaxFrameSizeDip,
                             2 * kMaxFrameSizeDip, 2 * kMaxFrameSizeDip);
  auto focused_context = DecorationContext(solid_frame_, true);
  frame_bounds_dip.Inset(-GtkStyleContextGetPadding(focused_context));
  frame_bounds_dip.Inset(-GtkStyleContextGetBorder(focused_context));
  gfx::Size bitmap_size(BitmapSizePx(asset), BitmapSizePx(asset));
  asset.focused_bitmap =
      PaintBitmap(bitmap_size, frame_bounds_dip, focused_context, scale);
  asset.unfocused_bitmap =
      PaintBitmap(bitmap_size, frame_bounds_dip,
                  DecorationContext(solid_frame_, false), scale);

  // In GTK4, there's no way to obtain the frame thickness from CSS values
  // directly, so we must determine it experimentally based on the drawn
  // bitmaps.
  auto get_inset = [&](auto&& pixel_iterator) -> int {
    for (int i = 0; i < asset.frame_size_px; ++i) {
      if (SkColorGetA(pixel_iterator(i))) {
        int inset_px = asset.frame_size_px - i;
        return std::ceil(inset_px / scale);
      }
    }
    return 0;
  };

  top_corner_radius_dip_ = ComputeTopCornerRadius();

  const auto previous_frame_thickness_dip_ = frame_thickness_dip_;
  frame_thickness_dip_ = gfx::Insets::TLBR(
      get_inset([&](int i) {
        return asset.focused_bitmap.getColor(2 * asset.frame_size_px, i);
      }),
      get_inset([&](int i) {
        return asset.focused_bitmap.getColor(i, 2 * asset.frame_size_px);
      }),
      get_inset([&](int i) {
        return asset.focused_bitmap.getColor(2 * asset.frame_size_px,
                                             BitmapSizePx(asset) - i - 1);
      }),
      get_inset([&](int i) {
        return asset.focused_bitmap.getColor(BitmapSizePx(asset) - i - 1,
                                             2 * asset.frame_size_px);
      }));
  if (!previous_frame_thickness_dip_.IsEmpty() &&
      frame_thickness_dip_ != previous_frame_thickness_dip_) {
    // The possibility of the mismatch is quite low because this logic affects
    // only mixed DPI setups on Linux, which itself is a rare configuration
    // already, and there the user needs to use some unusual scale that would
    // cause the mismatch.  So in theory, this is possible, but in practice, it
    // should never happen.
    LOG(ERROR) << "Frame thickness mismatch!  Old: ["
               << previous_frame_thickness_dip_.ToString() << "], new: ["
               << frame_thickness_dip_.ToString() << "].  Current scale is "
               << scale << ".  Please report to crbug.com/1240905.";
  }
  asset.frame_thickness_px =
      gfx::ScaleToRoundedInsets(frame_thickness_dip_, scale);

  asset.valid = true;
}

int WindowFrameProviderGtk::BitmapSizePx(const Asset& asset) const {
  // The window decoration will be rendered into a square with this side length.
  // The left and right sides of the decoration add 2 * kMaxDecorationThickness,
  // and the window itself has size 2 * kMaxDecorationThickness.
  return 4 * asset.frame_size_px;
}

}  // namespace gtk
