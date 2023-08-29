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
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
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

// Size of the zoomed contents, which excludes border and shadows.
constexpr gfx::Size kMagnifierSize{100, 40};

// Offset to apply to the magnifier bounds so that the magnifier is shown
// vertically above the caret (or selection endpoint). The offset specifies
// vertical displacement from the the top of the caret to the bottom of the
// magnifier's zoomed contents. Note that it is negative since the bottom of the
// zoomed contents should be above the top of the caret.
constexpr int kMagnifierVerticalBoundsOffset = -8;

constexpr int kMagnifierBorderThickness = 1;

// Shadows values to draw around the zoomed contents.
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

// Gets the bounds at which to show the magnifier layer. We try to horizontally
// center the magnifier above `anchor_point`, but adjust if needed to keep it
// within the parent bounds. `anchor_point` and returned bounds should be in
// coordinates of the magnifier's parent container.
gfx::Rect GetMagnifierLayerBounds(const gfx::Size& parent_container_size,
                                  const gfx::Point& anchor_point) {
  // Compute bounds for the magnifier zoomed contents, which we try to
  // horizontally center above `anchor_point`.
  const gfx::Point origin(anchor_point.x() - kMagnifierSize.width() / 2,
                          anchor_point.y() - kMagnifierSize.height() +
                              kMagnifierVerticalBoundsOffset);
  gfx::Rect magnifier_layer_bounds(origin, kMagnifierSize);

  // Outset the bounds to account for the magnifier border and shadows.
  magnifier_layer_bounds.Outset(GetMagnifierShadowOutsets());

  // Adjust if needed to keep the magnifier layer within the parent container
  // bounds, while keeping the magnifier size fixed.
  magnifier_layer_bounds.AdjustToFit(gfx::Rect(parent_container_size));
  return magnifier_layer_bounds;
}

// Gets the center of the source content to be shown in the magnifier. We try to
// center the source content on `focus_center`, but adjust if needed to keep the
// source content within the parent bounds. `focus_center` and returned source
// center should be in coordinates of the magnifier's parent container.
gfx::Point GetMagnifierSourceCenter(const gfx::Size& parent_container_size,
                                    const gfx::Point& focus_center) {
  const gfx::SizeF source_size(kMagnifierSize.width() / kMagnifierScale,
                               kMagnifierSize.height() / kMagnifierScale);
  const gfx::PointF source_origin(focus_center.x() - source_size.width() / 2,
                                  focus_center.y() - source_size.height() / 2);
  gfx::RectF source_bounds(source_origin, source_size);
  source_bounds.AdjustToFit(gfx::RectF(parent_container_size));
  return gfx::ToRoundedPoint(source_bounds.CenterPoint());
}

// Gets the background offset needed to correctly center the magnifier's zoomed
// contents. `magnifier_layer_bounds` and `magnifier_source_center` should be in
// coordinates of the magnifier's parent container.
gfx::Point GetZoomLayerBackgroundOffset(
    const gfx::Rect& magnifier_layer_bounds,
    const gfx::Point& magnifier_source_center) {
  // Compute the zoom layer center in coordinates of the magnifier's parent
  // container. Note that this is not exactly the same as the center of the
  // magnifier layer, since the magnifier layer includes non-uniform shadows
  // that surround the zoomed contents.
  const gfx::Point zoom_layer_center =
      GetZoomLayerBounds().CenterPoint() +
      magnifier_layer_bounds.OffsetFromOrigin();
  return gfx::PointAtOffsetFromOrigin(zoom_layer_center -
                                      magnifier_source_center);
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
    // zoomed contents, so we draw them around the zoom layer bounds.
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
    // bounds so that the border surrounds the zoomed contents.
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
  const gfx::Size magnifier_parent_size =
      magnifier_layer_->parent()->bounds().size();
  const gfx::Rect focus_rect = gfx::BoundingRect(focus_start, focus_end);
  const gfx::Rect magnifier_layer_bounds =
      GetMagnifierLayerBounds(magnifier_parent_size, focus_rect.top_center());
  const gfx::Point magnifier_source_center =
      GetMagnifierSourceCenter(magnifier_parent_size, focus_rect.CenterPoint());
  zoom_layer_->SetBackgroundOffset(GetZoomLayerBackgroundOffset(
      magnifier_layer_bounds, magnifier_source_center));
  magnifier_layer_->SetBounds(magnifier_layer_bounds);

  if (!magnifier_layer_->IsVisible()) {
    magnifier_layer_->SetVisible(true);
  }
}

gfx::Rect TouchSelectionMagnifierAura::GetZoomedContentsBoundsForTesting()
    const {
  // The zoomed contents is drawn by the zoom layer. We just need to convert its
  // bounds to coordinates of the magnifier layer's parent layer.
  return zoom_layer_->bounds() + magnifier_layer_->bounds().OffsetFromOrigin();
}

const Layer* TouchSelectionMagnifierAura::GetMagnifierParentForTesting() const {
  return magnifier_layer_->parent();
}

void TouchSelectionMagnifierAura::CreateMagnifierLayer() {
  // Create the magnifier layer, which will parent the zoom layer and border
  // layer.
  magnifier_layer_ = std::make_unique<Layer>(LAYER_NOT_DRAWN);
  magnifier_layer_->SetFillsBoundsOpaquely(false);

  // Create the zoom layer, which will show the zoomed contents.
  zoom_layer_ = std::make_unique<Layer>(LAYER_SOLID_COLOR);
  zoom_layer_->SetBounds(GetZoomLayerBounds());
  zoom_layer_->SetBackgroundZoom(kMagnifierScale, 0);
  zoom_layer_->SetFillsBoundsOpaquely(false);
  zoom_layer_->SetRoundedCornerRadius(gfx::RoundedCornersF{kMagnifierRadius});
  magnifier_layer_->Add(zoom_layer_.get());

  // Create the border layer. This is stacked above the zoom layer so that the
  // magnifier border and shadows aren't shown in the zoomed contents drawn by
  // the zoom layer.
  border_layer_ = std::make_unique<Layer>();
  border_layer_->SetBounds(gfx::Rect(GetBorderLayerSize()));
  border_renderer_ = std::make_unique<BorderRenderer>();
  border_layer_->set_delegate(border_renderer_.get());
  border_layer_->SetFillsBoundsOpaquely(false);
  magnifier_layer_->Add(border_layer_.get());
}

}  // namespace ui
