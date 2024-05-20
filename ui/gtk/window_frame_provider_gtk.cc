// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/window_frame_provider_gtk.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
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

GtkCssContext WindowContext(bool solid_frame, bool tiled, bool focused) {
  std::string selector = "window.background.";
  selector += solid_frame ? "solid-csd" : "csd";
  if (tiled) {
    selector += ".tiled";
  }
  if (!focused) {
    selector += ":inactive";
  }
  return AppendCssNodeToStyleContext({}, selector);
}

GtkCssContext DecorationContext(bool solid_frame, bool tiled, bool focused) {
  auto context = WindowContext(solid_frame, tiled, focused);
  // GTK4 renders the decoration directly on the window.
  if (!GtkCheckVersion(4)) {
    context = AppendCssNodeToStyleContext(context, "decoration");
  }
  if (!focused) {
    gtk_style_context_set_state(context, GTK_STATE_FLAG_BACKDROP);
  }

  // The web contents is rendered after the frame border, so remove bottom
  // rounded corners otherwise their borders would get covered up.
  ApplyCssToContext(context, R"(* {
    border-bottom-left-radius: 0;
    border-bottom-right-radius: 0;
  })");

  return context;
}

GtkCssContext HeaderContext(bool solid_frame, bool tiled, bool focused) {
  auto context = WindowContext(solid_frame, tiled, focused);
  context =
      AppendCssNodeToStyleContext(context, "headerbar.header-bar.titlebar");
  if (!focused) {
    gtk_style_context_set_state(context, GTK_STATE_FLAG_BACKDROP);
  }
  ApplyCssToContext(context, "* { border-bottom-style: none; }");
  return context;
}

SkBitmap PaintBitmap(const gfx::Size& bitmap_size,
                     const gfx::RectF& render_bounds,
                     GtkCssContext context,
                     float scale) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(bitmap_size.width(), bitmap_size.height());
  bitmap.eraseColor(SK_ColorTRANSPARENT);

  CairoSurface surface(bitmap);
  cairo_t* cr = surface.cairo();

  double opacity = GetOpacityFromContext(context);
  if (opacity < 1) {
    cairo_push_group(cr);
  }

  cairo_scale(cr, scale, scale);
  gtk_render_background(context, cr, render_bounds.x(), render_bounds.y(),
                        render_bounds.width(), render_bounds.height());
  gtk_render_frame(context, cr, render_bounds.x(), render_bounds.y(),
                   render_bounds.width(), render_bounds.height());

  if (opacity < 1) {
    cairo_pop_group_to_source(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_paint_with_alpha(cr, opacity);
  }

  bitmap.setImmutable();
  return bitmap;
}

SkBitmap PaintHeaderbar(const gfx::Size& size,
                        GtkCssContext context,
                        float scale) {
  gfx::RectF tabstrip_bounds_dip(0, 0, size.width() / scale,
                                 size.height() / scale);
  return PaintBitmap(size, tabstrip_bounds_dip, context, scale);
}

int ComputeTopCornerRadius() {
  // In GTK4, there's no way to directly obtain CSS values for a context, so we
  // need to experimentally determine the corner radius by rendering a sample.
  // Additionally, in GTK4, the headerbar corners get clipped by the window
  // rather than the headerbar having its own rounded corners.
  auto context = GtkCheckVersion(4) ? DecorationContext(false, false, false)
                                    : HeaderContext(false, false, false);
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
  auto bitmap =
      GtkCheckVersion(4)
          ? PaintBitmap(size_dip, {{0, 0}, gfx::SizeF(size_dip)}, context, 1)
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

// Returns true iff any part of the header is transparent (even a single pixel).
// This is used as an optimization hint to the compositor so that it doesn't
// have to composite behind opaque regions.  The consequence of a false-negative
// is rendering artifacts, but the consequence of a false-positive is only a
// slight performance penalty, so this function is intentionally conservative
// in deciding if the header is translucent.
bool HeaderIsTranslucent() {
  // The arbitrary square size to render a sample header.
  constexpr int kHeaderSize = 32;
  auto context = HeaderContext(false, false, false);
  double opacity = GetOpacityFromContext(context);
  if (opacity < 1.0) {
    return true;
  }
  ApplyCssToContext(context, R"(window, headerbar {
    box-shadow: none;
    border: none;
    border-radius: 0;
  })");
  gfx::Size size_dip{kHeaderSize, kHeaderSize};
  auto bitmap = PaintHeaderbar(size_dip, context, 1);
  for (int x = 0; x < kHeaderSize; x++) {
    for (int y = 0; y < kHeaderSize; y++) {
      if (SkColorGetA(bitmap.getColor(x, y)) != SK_AlphaOPAQUE) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

WindowFrameProviderGtk::Asset::Asset() = default;

WindowFrameProviderGtk::Asset::Asset(const WindowFrameProviderGtk::Asset& src) =
    default;

WindowFrameProviderGtk::Asset& WindowFrameProviderGtk::Asset::operator=(
    const WindowFrameProviderGtk::Asset& src) = default;

WindowFrameProviderGtk::Asset::~Asset() = default;

WindowFrameProviderGtk::WindowFrameProviderGtk(bool solid_frame, bool tiled)
    : solid_frame_(solid_frame), tiled_(tiled) {
  GtkSettings* settings = gtk_settings_get_default();
  // Unretained() is safe since WindowFrameProviderGtk will own the signals.
  auto callback = base::BindRepeating(&WindowFrameProviderGtk::OnThemeChanged,
                                      base::Unretained(this));
  theme_name_signal_ = ScopedGSignal(settings, "notify::gtk-theme-name",
                                     callback, G_CONNECT_AFTER);
  prefer_dark_signal_ =
      ScopedGSignal(settings, "notify::gtk-application-prefer-dark-theme",
                    callback, G_CONNECT_AFTER);
}

WindowFrameProviderGtk::~WindowFrameProviderGtk() = default;

int WindowFrameProviderGtk::GetTopCornerRadiusDip() {
  if (!top_corner_radius_dip_.has_value()) {
    top_corner_radius_dip_ = ComputeTopCornerRadius();
  }
  return *top_corner_radius_dip_;
}

bool WindowFrameProviderGtk::IsTopFrameTranslucent() {
  if (!top_frame_is_translucent_.has_value()) {
    top_frame_is_translucent_ = !solid_frame_ && HeaderIsTranslucent();
  }
  return *top_frame_is_translucent_;
}

gfx::Insets WindowFrameProviderGtk::GetFrameThicknessDip() {
  if (!frame_thickness_dip_.has_value()) {
    const auto& asset = GetOrCreateAsset(1.0f);

    // In GTK4, there's no way to obtain the frame thickness from CSS values
    // directly, so we must determine it experimentally based on the drawn
    // bitmaps.
    auto get_inset = [&](auto&& pixel_iterator) -> int {
      for (int i = 0; i < asset.frame_size_px; ++i) {
        if (SkColorGetA(pixel_iterator(i))) {
          return asset.frame_size_px - i;
        }
      }
      return 0;
    };

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
  }
  return *frame_thickness_dip_;
}

void WindowFrameProviderGtk::PaintWindowFrame(gfx::Canvas* canvas,
                                              const gfx::Rect& rect_dip,
                                              int top_area_height_dip,
                                              bool focused,
                                              const gfx::Insets& input_insets) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();

  const auto& asset = GetOrCreateAsset(scale);

  const auto input_insets_px = gfx::ScaleToRoundedInsets(input_insets, scale);
  auto effective_frame_thickness_px =
      gfx::ScaleToRoundedInsets(GetFrameThicknessDip(), scale);
  effective_frame_thickness_px.SetToMax(input_insets_px);

  auto client_bounds_px = gfx::ScaleToRoundedRect(rect_dip, scale);
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
    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
      return;
    }
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

  const int top_area_bottom_dip = rect_dip.y() + top_area_height_dip;
  const int top_area_bottom_px = base::ClampCeil(top_area_bottom_dip * scale);
  const int top_area_height_px = top_area_bottom_px - client_bounds_px.y();

  auto header =
      PaintHeaderbar({client_bounds_px.width(), top_area_height_px},
                     HeaderContext(solid_frame_, tiled_, focused), scale);
  image = gfx::ImageSkia::CreateFrom1xBitmap(header);
  // In GTK4, the headerbar gets clipped by the window.
  if (GtkCheckVersion(4)) {
    gfx::RectF bounds_px =
        gfx::RectF(client_bounds_px.x(), client_bounds_px.y(), header.width(),
                   header.height());
    float radius_px = scale * GetTopCornerRadiusDip();
    SkVector radii[4]{{radius_px, radius_px}, {radius_px, radius_px}, {}, {}};
    SkRRect clip;
    clip.setRectRadii(gfx::RectFToSkRect(bounds_px), radii);
    canvas->sk_canvas()->clipRRect(clip, SkClipOp::kIntersect, true);
  }
  draw_image(0, 0, header.width(), header.height(), client_bounds_px.x(),
             client_bounds_px.y(), header.width(), header.height());
}

WindowFrameProviderGtk::Asset& WindowFrameProviderGtk::GetOrCreateAsset(
    float scale) {
  auto it = assets_.find(scale);
  if (it != assets_.end()) {
    return it->second;
  }
  auto& asset = assets_[scale];

  asset.frame_size_px = std::ceil(kMaxFrameSizeDip * scale);

  gfx::Rect frame_bounds_dip(kMaxFrameSizeDip, kMaxFrameSizeDip,
                             2 * kMaxFrameSizeDip, 2 * kMaxFrameSizeDip);
  auto focused_context = DecorationContext(solid_frame_, tiled_, true);
  frame_bounds_dip.Inset(-GtkStyleContextGetPadding(focused_context));
  frame_bounds_dip.Inset(-GtkStyleContextGetBorder(focused_context));
  gfx::Size bitmap_size(BitmapSizePx(asset), BitmapSizePx(asset));
  asset.focused_bitmap = PaintBitmap(bitmap_size, gfx::RectF(frame_bounds_dip),
                                     focused_context, scale);
  asset.unfocused_bitmap =
      PaintBitmap(bitmap_size, gfx::RectF(frame_bounds_dip),
                  DecorationContext(solid_frame_, tiled_, false), scale);

  return asset;
}

int WindowFrameProviderGtk::BitmapSizePx(const Asset& asset) const {
  // The window decoration will be rendered into a square with this side length.
  // The left and right sides of the decoration add 2 * kMaxDecorationThickness,
  // and the window itself has size 2 * kMaxDecorationThickness.
  return 4 * asset.frame_size_px;
}

void WindowFrameProviderGtk::OnThemeChanged(GtkSettings* settings,
                                            GtkParamSpec* param) {
  assets_.clear();
  frame_thickness_dip_ = std::nullopt;
  top_corner_radius_dip_ = std::nullopt;
  top_frame_is_translucent_ = std::nullopt;
}

}  // namespace gtk
