// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/frame_view_utils_linux.h"

#include <cmath>

#include "base/notreached.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"
#include "ui/views/window/frame_background.h"

namespace views {

namespace {

constexpr int kBorderAlpha = 0x26;

}  // namespace

void PaintRestoredFrameBorderLinux(gfx::Canvas& canvas,
                                   const views::View& view,
                                   views::FrameBackground* frame_background,
                                   const SkRRect& clip,
                                   bool showing_shadow,
                                   bool is_active,
                                   const gfx::Insets& border,
                                   const gfx::ShadowValues& shadow_values,
                                   bool tiled) {
  const auto* color_provider = view.GetColorProvider();
  if (frame_background) {
    gfx::ScopedCanvas scoped_canvas(&canvas);
    canvas.sk_canvas()->clipRRect(clip, SkClipOp::kIntersect, true);
    auto shadow_inset = showing_shadow ? border : gfx::Insets();
    frame_background->PaintMaximized(
        &canvas, view.GetNativeTheme(), color_provider, shadow_inset.left(),
        shadow_inset.top(), view.width() - shadow_inset.width());
    if (!showing_shadow) {
      frame_background->FillFrameBorders(&canvas, &view, border.left(),
                                         border.right(), border.bottom());
    }
  }

  // If rendering shadows, draw a 1px exterior border, otherwise draw a 1px
  // interior border.
  const SkScalar one_pixel = SkFloatToScalar(1 / canvas.image_scale());
  SkRRect outset_rect = clip;
  SkRRect inset_rect = clip;
  if (tiled) {
    outset_rect.outset(1, 1);
  } else if (showing_shadow) {
    outset_rect.outset(one_pixel, one_pixel);
  } else {
    inset_rect.inset(one_pixel, one_pixel);
  }

  cc::PaintFlags flags;
  const SkColor frame_color = color_provider->GetColor(
      is_active ? ui::kColorFrameActive : ui::kColorFrameInactive);
  const SkColor border_color =
      showing_shadow ? SK_ColorBLACK
                     : color_utils::PickContrastingColor(
                           SK_ColorBLACK, SK_ColorWHITE, frame_color);
  flags.setColor(SkColorSetA(border_color, kBorderAlpha));
  flags.setAntiAlias(true);
  if (showing_shadow) {
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values));
  }

  gfx::ScopedCanvas scoped_canvas(&canvas);
  canvas.sk_canvas()->clipRRect(inset_rect, SkClipOp::kDifference, true);
  canvas.sk_canvas()->drawRRect(outset_rect, flags);
}

gfx::Insets GetRestoredFrameBorderInsetsLinux(
    bool showing_shadow,
    const gfx::Insets& default_border,
    const gfx::ShadowValues& shadow_values,
    const gfx::Insets& resize_border) {
  if (!showing_shadow) {
    auto no_shadow_border = default_border;
    no_shadow_border.set_top(0);
    return no_shadow_border;
  }

  // The border must be at least as large as the shadow.
  gfx::Rect frame_extents;
  for (const auto& shadow_value : shadow_values) {
    const auto shadow_radius = shadow_value.blur() / 4;
    const gfx::InsetsF shadow_insets(shadow_radius);
    gfx::RectF shadow_extents;
    shadow_extents.Inset(-shadow_insets);
    shadow_extents.set_origin(shadow_extents.origin() + shadow_value.offset());
    frame_extents.Union(gfx::ToEnclosingRect(shadow_extents));
  }

  // The border must be at least as large as the input region.
  const gfx::Insets insets(resize_border);
  gfx::Rect input_extents;
  input_extents.Inset(-insets);
  frame_extents.Union(input_extents);

  return gfx::Insets::TLBR(-frame_extents.y(), -frame_extents.x(),
                           frame_extents.bottom(), frame_extents.right());
}

SkRRect GetRestoredClipRegion(const gfx::RectF& bounds,
                              const gfx::InsetsF& border,
                              const gfx::RoundedCornersF& radii) {
  gfx::RectF clipped_bounds = bounds;
  clipped_bounds.Inset(border);
  SkVector sk_radii[4]{
      {radii.upper_left(), radii.upper_left()},
      {radii.upper_right(), radii.upper_right()},
      {radii.lower_right(), radii.lower_right()},
      {radii.lower_left(), radii.lower_left()},
  };
  SkRRect clip;
  clip.setRectRadii(gfx::RectFToSkRect(clipped_bounds), sk_radii);
  return clip;
}

ui::NavButtonProvider::FrameButtonDisplayType GetFrameButtonDisplayType(
    FrameButton button_id,
    bool is_maximized) {
  switch (button_id) {
    case FrameButton::kMinimize:
      return ui::NavButtonProvider::FrameButtonDisplayType::kMinimize;
    case FrameButton::kMaximize:
      return is_maximized
                 ? ui::NavButtonProvider::FrameButtonDisplayType::kRestore
                 : ui::NavButtonProvider::FrameButtonDisplayType::kMaximize;
    case FrameButton::kClose:
      return ui::NavButtonProvider::FrameButtonDisplayType::kClose;
  }
}

bool DrawFrameButtonParams::operator==(
    const DrawFrameButtonParams& other) const {
  return top_area_height == other.top_area_height &&
         maximized == other.maximized && active == other.active;
}

void MaybeUpdateCachedFrameButtonImages(
    ui::NavButtonProvider* nav_button_provider,
    const DrawFrameButtonParams& params,
    std::optional<DrawFrameButtonParams>& cache,
    base::FunctionRef<views::Button*(
        ui::NavButtonProvider::FrameButtonDisplayType)> get_button) {
  if (cache == params) {
    return;
  }
  cache = params;
  nav_button_provider->RedrawImages(params.top_area_height, params.maximized,
                                    params.active);
  for (auto type : {
           ui::NavButtonProvider::FrameButtonDisplayType::kMinimize,
           params.maximized
               ? ui::NavButtonProvider::FrameButtonDisplayType::kRestore
               : ui::NavButtonProvider::FrameButtonDisplayType::kMaximize,
           ui::NavButtonProvider::FrameButtonDisplayType::kClose,
       }) {
    for (size_t state = 0; state < views::Button::STATE_COUNT; state++) {
      views::Button::ButtonState button_state =
          static_cast<views::Button::ButtonState>(state);
      views::Button* button = get_button(type);
      DCHECK_EQ(views::ImageButton::kViewClassName, button->GetClassName());
      static_cast<views::ImageButton*>(button)->SetImageModel(
          button_state,
          ui::ImageModel::FromImageSkia(nav_button_provider->GetImage(
              type, ButtonStateToNavButtonProviderState(button_state))));
    }
  }
}

ui::NavButtonProvider::ButtonState ButtonStateToNavButtonProviderState(
    Button::ButtonState state) {
  switch (state) {
    case Button::STATE_NORMAL:
      return ui::NavButtonProvider::ButtonState::kNormal;
    case Button::STATE_HOVERED:
      return ui::NavButtonProvider::ButtonState::kHovered;
    case Button::STATE_PRESSED:
      return ui::NavButtonProvider::ButtonState::kPressed;
    case Button::STATE_DISABLED:
      return ui::NavButtonProvider::ButtonState::kDisabled;
    case Button::STATE_COUNT:
    default:
      NOTREACHED();
  }
}

std::vector<gfx::Rect> GetRestoredOpaqueRegion(
    const SkRRect& clip_region,
    float scale,
    int translucent_top_area_height_dip) {
  // The opaque region is a list of rectangles that contain only fully
  // opaque pixels of the window.  We need to convert the clipping
  // rounded-rect into this format.
  gfx::RectF rectf = gfx::SkRectToRectF(clip_region.rect());
  rectf.Scale(scale);
  // It is acceptable to omit some pixels that are opaque, but the region
  // must not include any translucent pixels.  Therefore, we must
  // conservatively scale to the enclosed rectangle.
  gfx::Rect rect = gfx::ToEnclosedRect(rectf);

  // Create the initial region from the clipping rectangle without rounded
  // corners.
  SkRegion region(gfx::RectToSkIRect(rect));

  // Now subtract out the small rectangles that cover the corners.
  struct {
    SkRRect::Corner corner;
    bool left;
    bool upper;
  } kCorners[] = {
      {SkRRect::kUpperLeft_Corner, true, true},
      {SkRRect::kUpperRight_Corner, false, true},
      {SkRRect::kLowerLeft_Corner, true, false},
      {SkRRect::kLowerRight_Corner, false, false},
  };
  for (const auto& corner : kCorners) {
    auto radii = clip_region.radii(corner.corner);
    auto rx = std::ceil(scale * radii.x());
    auto ry = std::ceil(scale * radii.y());
    auto corner_rect =
        SkIRect::MakeXYWH(corner.left ? rect.x() : rect.right() - rx,
                          corner.upper ? rect.y() : rect.bottom() - ry, rx, ry);
    region.op(corner_rect, SkRegion::kDifference_Op);
  }

  auto translucent_top_area_rect = SkIRect::MakeXYWH(
      rect.x(), rect.y(), rect.width(),
      std::ceil(translucent_top_area_height_dip * scale - rect.y()));
  region.op(translucent_top_area_rect, SkRegion::kDifference_Op);

  // Convert the region to a list of rectangles.
  std::vector<gfx::Rect> opaque_region;
  for (SkRegion::Iterator i(region); !i.done(); i.next()) {
    opaque_region.push_back(gfx::SkIRectToRect(i.rect()));
  }
  return opaque_region;
}

}  // namespace views
