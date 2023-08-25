// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/frame_background.h"

#include <algorithm>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/buildflags.h"
#include "ui/views/view.h"

namespace views {

FrameBackground::FrameBackground() = default;

FrameBackground::~FrameBackground() = default;

void FrameBackground::SetSideImages(const gfx::ImageSkia* left,
                                    const gfx::ImageSkia* top,
                                    const gfx::ImageSkia* right,
                                    const gfx::ImageSkia* bottom) {
  left_edge_ = left;
  top_edge_ = top;
  right_edge_ = right;
  bottom_edge_ = bottom;
}

void FrameBackground::SetCornerImages(const gfx::ImageSkia* top_left,
                                      const gfx::ImageSkia* top_right,
                                      const gfx::ImageSkia* bottom_left,
                                      const gfx::ImageSkia* bottom_right) {
  top_left_corner_ = top_left;
  top_right_corner_ = top_right;
  bottom_left_corner_ = bottom_left;
  bottom_right_corner_ = bottom_right;
}

void FrameBackground::PaintRestored(gfx::Canvas* canvas,
                                    const View* view) const {
  // Restored window painting is a superset of maximized window painting; let
  // the maximized code paint the frame color and images.
  PaintMaximized(canvas, view);

  // Fill the frame borders with the frame color before drawing the edge images.
  FillFrameBorders(canvas, view, left_edge_->width(), right_edge_->width(),
                   bottom_edge_->height());

  // Draw the top corners and edge, scaling the corner images down if they
  // are too big and relative to the vertical space available.
  int top_left_height =
      std::min(top_left_corner_->height(),
               view->height() - bottom_left_corner_->height());
  canvas->DrawImageInt(*top_left_corner_, 0, 0, top_left_corner_->width(),
                       top_left_height, 0, 0, top_left_corner_->width(),
                       top_left_height, false);
  canvas->TileImageInt(
      *top_edge_, top_left_corner_->width(), 0,
      view->width() - top_left_corner_->width() - top_right_corner_->width(),
      top_edge_->height());
  int top_right_height =
      std::min(top_right_corner_->height(),
               view->height() - bottom_right_corner_->height());
  canvas->DrawImageInt(*top_right_corner_, 0, 0, top_right_corner_->width(),
                       top_right_height,
                       view->width() - top_right_corner_->width(), 0,
                       top_right_corner_->width(), top_right_height, false);

  // Right edge.
  int right_edge_height =
      view->height() - top_right_height - bottom_right_corner_->height();
  canvas->TileImageInt(*right_edge_, view->width() - right_edge_->width(),
                       top_right_height, right_edge_->width(),
                       right_edge_height);

  // Bottom corners and edge.
  canvas->DrawImageInt(*bottom_right_corner_,
                       view->width() - bottom_right_corner_->width(),
                       view->height() - bottom_right_corner_->height());
  canvas->TileImageInt(*bottom_edge_, bottom_left_corner_->width(),
                       view->height() - bottom_edge_->height(),
                       view->width() - bottom_left_corner_->width() -
                           bottom_right_corner_->width(),
                       bottom_edge_->height());
  canvas->DrawImageInt(*bottom_left_corner_, 0,
                       view->height() - bottom_left_corner_->height());

  // Left edge.
  int left_edge_height =
      view->height() - top_left_height - bottom_left_corner_->height();
  canvas->TileImageInt(*left_edge_, 0, top_left_height, left_edge_->width(),
                       left_edge_height);
}

void FrameBackground::PaintMaximized(gfx::Canvas* canvas,
                                     const View* view) const {
  PaintMaximized(canvas, view->GetNativeTheme(), view->GetColorProvider(),
                 view->x(), view->y(), view->width());
}

void FrameBackground::PaintMaximized(gfx::Canvas* canvas,
                                     const ui::NativeTheme* native_theme,
                                     const ui::ColorProvider* color_provider,
                                     int x,
                                     int y,
                                     int width) const {
// Fill the top with the frame color first so we have a constant background
// for areas not covered by the theme image.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    BUILDFLAG(ENABLE_DESKTOP_AURA)
  ui::NativeTheme::FrameTopAreaExtraParams frame_top_area;
  frame_top_area.use_custom_frame = use_custom_frame_;
  frame_top_area.is_active = is_active_;
  frame_top_area.default_background_color = frame_color_;
  ui::NativeTheme::ExtraParams params(frame_top_area);
  native_theme->Paint(canvas->sk_canvas(), color_provider,
                      ui::NativeTheme::kFrameTopArea, ui::NativeTheme::kNormal,
                      gfx::Rect(x, y, width, top_area_height_), params);
#else
  canvas->FillRect(gfx::Rect(x, y, width, top_area_height_), frame_color_);
#endif

  // Draw the theme frame and overlay, if available.
  // See TopContainerBackground::PaintThemeCustomImage for details on the
  // positioning logic.
  if (!theme_image_.isNull()) {
    int x_offset = theme_image_inset_.x() + x;
    int y_offset = theme_image_inset_.y() + y;
    canvas->TileImageInt(theme_image_, x_offset, y_offset, x, y, width,
                         top_area_height_, 1.0f, SkTileMode::kRepeat,
                         SkTileMode::kMirror);
  }
  if (!theme_overlay_image_.isNull())
    canvas->DrawImageInt(theme_overlay_image_, x, y - maximized_top_inset_);
}

void FrameBackground::FillFrameBorders(gfx::Canvas* canvas,
                                       const View* view,
                                       int left_edge_width,
                                       int right_edge_width,
                                       int bottom_edge_height) const {
  // If the window is very short, we don't need to fill any borders.
  int remaining_height = view->height() - top_area_height_;
  if (remaining_height <= 0)
    return;

  // Fill down the sides.
  canvas->FillRect(
      gfx::Rect(0, top_area_height_, left_edge_width, remaining_height),
      frame_color_);
  canvas->FillRect(gfx::Rect(view->width() - right_edge_width, top_area_height_,
                             right_edge_width, remaining_height),
                   frame_color_);

  // If the window is very narrow, we're done.
  int center_width = view->width() - left_edge_width - right_edge_width;
  if (center_width <= 0)
    return;

  // Fill the bottom area.
  canvas->FillRect(
      gfx::Rect(left_edge_width, view->height() - bottom_edge_height,
                center_width, bottom_edge_height),
      frame_color_);
}

}  // namespace views
