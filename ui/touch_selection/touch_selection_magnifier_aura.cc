// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_magnifier_aura.h"

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

namespace {

constexpr float kMagnifierScale = 1.25f;

constexpr int kMagnifierRadius = 20;

// Duration of the animation when updating magnifier bounds.
constexpr base::TimeDelta kMagnifierTransitionDuration = base::Milliseconds(50);

// Size of the magnified area, which excludes border and shadows.
constexpr gfx::Size kMagnifierSize{100, 40};

// Offset to apply to the magnifier bounds so that the magnifier is shown
// vertically above the caret (or selection endpoint). The offset specifies
// vertical displacement from the the top of the caret to the bottom of the
// magnified area. Note that it is negative since the bottom of the magnified
// area should be above the top of the caret.
constexpr int kMagnifierVerticalBoundsOffset = -8;

constexpr int kMagnifierBorderThickness = 1;

// Shadows values to draw around the magnified area.
gfx::ShadowValues GetMagnifierShadowValues() {
  constexpr int kShadowElevation = 3;
  constexpr int kShadowBlurCorrection = 2;
  return {gfx::ShadowValue(gfx::Vector2d(0, kShadowElevation),
                           kShadowBlurCorrection * kShadowElevation,
                           SkColorSetA(SK_ColorBLACK, 0x3d)),
          gfx::ShadowValue(gfx::Vector2d(),
                           kShadowBlurCorrection * kShadowElevation,
                           SkColorSetA(SK_ColorBLACK, 0x1a))};
}

// The space outside the zoom layer needed for shadows.
gfx::Outsets GetMagnifierShadowOutsets() {
  return gfx::ShadowValue::GetMargin(GetMagnifierShadowValues()).ToOutsets();
}

// Bounds of the zoom layer in coordinates of its parent. These zoom layer
// bounds are fixed since we only update the bounds of the parent magnifier
// layer when the magnifier moves.
gfx::Rect GetZoomLayerBounds() {
  const gfx::Outsets magnifier_shadow_outsets = GetMagnifierShadowOutsets();
  return gfx::Rect(magnifier_shadow_outsets.left(),
                   magnifier_shadow_outsets.top(), kMagnifierSize.width(),
                   kMagnifierSize.height());
}

// Size of the border layer, which includes space for the zoom layer and
// surrounding border and shadows.
gfx::Size GetBorderLayerSize() {
  return kMagnifierSize + GetMagnifierShadowOutsets().size();
}

// Gets the bounds of the content that will be magnified, relative to the parent
// (`parent_bounds` should be the parent's bounds in its own coordinate space,
// e.g. {0,0,w,h}). The magnified bounds will be in the same coordinate space as
// `parent_bounds` and are adjusted to be contained within them.
gfx::Rect GetMagnifiedBounds(const gfx::Rect& parent_bounds,
                             const gfx::Point& focus_center) {
  gfx::SizeF magnified_size(kMagnifierSize.width() / kMagnifierScale,
                            kMagnifierSize.height() / kMagnifierScale);
  gfx::PointF origin(focus_center.x() - magnified_size.width() / 2,
                     focus_center.y() - magnified_size.height() / 2);

  gfx::RectF magnified_bounds(origin, magnified_size);
  magnified_bounds.AdjustToFit(gfx::RectF(parent_bounds));

  // Transform the adjusted magnified_bounds to the layer's scale. It's okay if
  // these bounds go outside the container, since they will be offset and then
  // fit to the parent.
  magnified_size = {kMagnifierScale * magnified_bounds.width(),
                    kMagnifierScale * magnified_bounds.height()};
  origin = {magnified_bounds.CenterPoint().x() - magnified_size.width() / 2,
            magnified_bounds.CenterPoint().y() - magnified_size.height() / 2};
  return gfx::ToEnclosingRect(gfx::RectF(origin, magnified_size));
}

std::pair<gfx::Rect, gfx::Point> GetMagnifierLayerBoundsAndOffset(
    const gfx::Size& parent_size,
    const gfx::Rect& focus_rect) {
  // The parent-relative bounding box of the parent container, which is the
  // coordinate space that the magnifier layer's bounds need to be in.
  const gfx::Rect parent_bounds(parent_size);
  // `magnified_bounds` holds the bounds of the content that will be magnified,
  // but that contains the `focus_center`, making it so the user's finger blocks
  // it if the final magnified content were shown in place.
  const gfx::Rect magnified_bounds =
      GetMagnifiedBounds(parent_bounds, focus_rect.CenterPoint());
  // To avoid being blocked, offset the bounds (and the background so it
  // remains visually consistent) along the Y axis. This must be clamped to
  // `parent_bounds` so that it's not drawn off the top edge of the screen.
  gfx::Rect layer_bounds = magnified_bounds;
  layer_bounds.Offset(0, kMagnifierVerticalBoundsOffset -
                             magnified_bounds.height() / 2 -
                             focus_rect.height() / 2);

  layer_bounds.Outset(GetMagnifierShadowOutsets());
  layer_bounds.AdjustToFit(parent_bounds);

  // `zoom_layer_center` is the center of the zoom layer relative to the
  // magnifier layer's parent. Since the magnifier layer has non-uniform outsets
  // for the shadows, its center (layer_bounds.CenterPoint()) is not exactly
  // the same as the center of the zoom layer.
  const gfx::Point zoom_layer_center =
      GetZoomLayerBounds().CenterPoint() + layer_bounds.OffsetFromOrigin();
  const gfx::Point offset = gfx::PointAtOffsetFromOrigin(
      zoom_layer_center - magnified_bounds.CenterPoint());
  return {layer_bounds, offset};
}

// Gets the color to use for the border based on the default native theme.
SkColor GetBorderColor() {
  auto* native_theme = NativeTheme::GetInstanceForNativeUi();
  return SkColorSetA(
      ColorProviderManager::Get()
          .GetColorProviderFor(native_theme->GetColorProviderKey(nullptr))
          ->GetColor(ui::kColorSeparator),
      0x23);
}

}  // namespace

// Delegate for drawing the magnifier border and shadows onto the border layer.
class TouchSelectionMagnifierAura::BorderRenderer : public LayerDelegate {
 public:
  BorderRenderer() = default;
  BorderRenderer(const BorderRenderer&) = delete;
  BorderRenderer& operator=(const BorderRenderer&) = delete;
  ~BorderRenderer() override = default;

  // LayerDelegate:
  void OnPaintLayer(const PaintContext& context) override {
    PaintRecorder recorder(context, GetBorderLayerSize());
    const gfx::Rect zoom_layer_bounds = GetZoomLayerBounds();

    // Draw shadows onto the border layer. These shadows should surround the
    // magnified area, so we draw them around the zoom layer bounds.
    cc::PaintFlags shadow_flags;
    shadow_flags.setAntiAlias(true);
    shadow_flags.setColor(SK_ColorTRANSPARENT);
    shadow_flags.setLooper(
        gfx::CreateShadowDrawLooper(GetMagnifierShadowValues()));
    recorder.canvas()->DrawRoundRect(zoom_layer_bounds, kMagnifierRadius,
                                     shadow_flags);

    // Since the border layer is stacked above the zoom layer (to prevent the
    // magnifier border and shadows from being magnified), we now need to clear
    // the parts of the shadow covering the zoom layer.
    cc::PaintFlags mask_flags;
    mask_flags.setAntiAlias(true);
    mask_flags.setBlendMode(SkBlendMode::kClear);
    mask_flags.setStyle(cc::PaintFlags::kFill_Style);
    recorder.canvas()->DrawRoundRect(zoom_layer_bounds, kMagnifierRadius,
                                     mask_flags);

    // Draw the magnifier border onto the border layer, using the zoom layer
    // bounds so that the border surrounds the magnified area.
    cc::PaintFlags border_flags;
    border_flags.setAntiAlias(true);
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);
    border_flags.setStrokeWidth(kMagnifierBorderThickness);
    border_flags.setColor(GetBorderColor());
    recorder.canvas()->DrawRoundRect(zoom_layer_bounds, kMagnifierRadius,
                                     border_flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
};

TouchSelectionMagnifierAura::TouchSelectionMagnifierAura() {
  CreateMagnifierLayer();
}

TouchSelectionMagnifierAura::~TouchSelectionMagnifierAura() = default;

void TouchSelectionMagnifierAura::ShowFocusBound(Layer* parent,
                                                 const gfx::Point& focus_start,
                                                 const gfx::Point& focus_end) {
  DCHECK(parent);
  if (magnifier_layer_->parent() != parent) {
    // Hide the magnifier when parenting or reparenting the magnifier so that it
    // doesn't appear with the wrong bounds.
    magnifier_layer_->SetVisible(false);
    parent->Add(magnifier_layer_.get());
  }

  // Set up the animation for updating the magnifier bounds.
  ui::ScopedLayerAnimationSettings settings(magnifier_layer_->GetAnimator());
  if (!magnifier_layer_->IsVisible()) {
    // Set the magnifier to appear immediately once its bounds are set.
    settings.SetTransitionDuration(base::Milliseconds(0));
    settings.SetTweenType(gfx::Tween::ZERO);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  } else {
    // Set the magnifier to move smoothly from its current bounds to the updated
    // bounds.
    settings.SetTransitionDuration(kMagnifierTransitionDuration);
    settings.SetTweenType(gfx::Tween::LINEAR);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  }

  // Update magnifier bounds and background offset.
  const gfx::Rect focus_rect = gfx::BoundingRect(focus_start, focus_end);
  auto [magnifier_layer_bounds, background_offset] =
      GetMagnifierLayerBoundsAndOffset(
          magnifier_layer_->parent()->bounds().size(), focus_rect);
  zoom_layer_->SetBackgroundOffset(background_offset);
  magnifier_layer_->SetBounds(magnifier_layer_bounds);

  if (!magnifier_layer_->IsVisible()) {
    magnifier_layer_->SetVisible(true);
  }
}

gfx::Rect TouchSelectionMagnifierAura::GetMagnifiedAreaBoundsForTesting()
    const {
  // The magnified area is drawn by the zoom layer. We just need to convert its
  // bounds to coordinates of the magnifier layer's parent layer.
  return GetZoomLayerBounds() + magnifier_layer_->bounds().OffsetFromOrigin();
}

const Layer* TouchSelectionMagnifierAura::GetMagnifierParentForTesting() const {
  return magnifier_layer_->parent();
}

void TouchSelectionMagnifierAura::CreateMagnifierLayer() {
  // Create the magnifier layer, which will parent the zoom layer and border
  // layer.
  magnifier_layer_ = std::make_unique<Layer>(LAYER_NOT_DRAWN);
  magnifier_layer_->SetFillsBoundsOpaquely(false);

  // Create the zoom layer, which will show the magnified area.
  zoom_layer_ = std::make_unique<Layer>(LAYER_SOLID_COLOR);
  zoom_layer_->SetBounds(GetZoomLayerBounds());
  zoom_layer_->SetBackgroundZoom(kMagnifierScale, 0);
  zoom_layer_->SetFillsBoundsOpaquely(false);
  zoom_layer_->SetRoundedCornerRadius(gfx::RoundedCornersF{kMagnifierRadius});
  magnifier_layer_->Add(zoom_layer_.get());

  // Create the border layer. This is stacked above the zoom layer so that the
  // magnifier border and shadows aren't shown in the magnified area drawn by
  // the zoom layer.
  border_layer_ = std::make_unique<Layer>();
  border_layer_->SetBounds(gfx::Rect(GetBorderLayerSize()));
  border_renderer_ = std::make_unique<BorderRenderer>();
  border_layer_->set_delegate(border_renderer_.get());
  border_layer_->SetFillsBoundsOpaquely(false);
  magnifier_layer_->Add(border_layer_.get());
}

}  // namespace ui
