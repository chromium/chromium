// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_FRAME_VIEW_UTILS_LINUX_H_
#define UI_VIEWS_WINDOW_FRAME_VIEW_UTILS_LINUX_H_

#include <optional>
#include <vector>

#include "base/functional/function_ref.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/shadow_value.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/views_export.h"
#include "ui/views/window/frame_buttons.h"

namespace gfx {
class Canvas;
class Insets;
class InsetsF;
class RectF;
}  // namespace gfx

namespace views {

class FrameBackground;
class View;

// Parameters used for drawing frame buttons, used for caching.
struct VIEWS_EXPORT DrawFrameButtonParams {
  bool operator==(const DrawFrameButtonParams& other) const;

  int top_area_height;
  bool maximized;
  bool active;
};

// Redraws the image resources associated with the minimize, maximize,
// restore, and close buttons.
VIEWS_EXPORT void MaybeUpdateCachedFrameButtonImages(
    ui::NavButtonProvider* nav_button_provider,
    const DrawFrameButtonParams& params,
    std::optional<DrawFrameButtonParams>& cache,
    base::FunctionRef<views::Button*(
        ui::NavButtonProvider::FrameButtonDisplayType)> get_button);

// Converts a views::Button::ButtonState to the corresponding
// ui::NavButtonProvider::ButtonState.
VIEWS_EXPORT ui::NavButtonProvider::ButtonState
ButtonStateToNavButtonProviderState(Button::ButtonState state);

// Returns the FrameButtonDisplayType for the given FrameButton, taking into
// account the current maximized state of the window.
VIEWS_EXPORT ui::NavButtonProvider::FrameButtonDisplayType
GetFrameButtonDisplayType(FrameButton button_id, bool is_maximized);

// Paint the window borders, shadows and the background of the top bar area for
// frame views on Linux that use client-side decorations.
VIEWS_EXPORT void PaintRestoredFrameBorderLinux(
    gfx::Canvas& canvas,
    const views::View& view,
    views::FrameBackground* frame_background,
    const SkRRect& clip,
    bool showing_shadow,
    bool is_active,
    const gfx::Insets& border,
    const gfx::ShadowValues& shadow_values,
    bool tiled);

// Get the insets from the native window edge to the client view when the window
// is restored for frame views on Linux that use client-side decorations.
VIEWS_EXPORT gfx::Insets GetRestoredFrameBorderInsetsLinux(
    bool showing_shadow,
    const gfx::Insets& default_border,
    const gfx::ShadowValues& shadow_values,
    const gfx::Insets& resize_border);

// Build a clip region for a restored window with rounded corners.
// `bounds` is the outer frame rect, `border` the insets to apply before
// rounding, and `radii` the corner radii in DIPs.
VIEWS_EXPORT SkRRect GetRestoredClipRegion(const gfx::RectF& bounds,
                                           const gfx::InsetsF& border,
                                           const gfx::RoundedCornersF& radii);

// Computes the opaque region for a window with the given rounded clip region,
// subtracting the rounded corners and any translucent top area.
VIEWS_EXPORT std::vector<gfx::Rect> GetRestoredOpaqueRegion(
    const SkRRect& clip_region,
    float scale,
    int translucent_top_area_height_dip);

}  // namespace views

#endif  // UI_VIEWS_WINDOW_FRAME_VIEW_UTILS_LINUX_H_
