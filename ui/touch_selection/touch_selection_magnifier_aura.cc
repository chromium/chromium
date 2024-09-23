// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_magnifier_aura.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
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
#include "ui/native_theme/native_theme_observer.h"

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

constexpr float kMagnifierBorderOpacity = 0.2f;

// Shadows to draw around the zoomed contents.
// TODO(b/299966070): Try to unify this with how other shadows are handled.
gfx::ShadowValues GetMagnifierShadowValues(SkColor key_shadow_color,
                                           SkColor ambient_shadow_color) {
  constexpr int kShadowElevation = 3;
  constexpr int kShadowBlur = 2 * kShadowElevation;
  return {gfx::ShadowValue(gfx::Vector2d(0, kShadowElevation), kShadowBlur,
                           key_shadow_color),
          gfx::ShadowValue(gfx::Vector2d(), kShadowBlur, ambient_shadow_color)};
}

// The space outside the zoom layer needed for shadows.
gfx::Outsets GetMagnifierShadowOutsets() {
  return gfx::ShadowValue::GetMargin(
             GetMagnifierShadowValues(gfx::kPlaceholderColor,
                                      gfx::kPlaceholderColor))
      .ToOutsets();
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

}  // namespace

// Delegate for drawing the magnifier border and shadows onto the border layer.
class TouchSelectionMagnifierAura::BorderRenderer : public LayerDelegate {
 public:
  BorderRenderer() { UpdateTheme(NativeTheme::GetInstanceForNativeUi()); }
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
    shadow_flags.setLooper(gfx::CreateShadowDrawLooper(
        GetMagnifierShadowValues(key_shadow_color_, ambient_shadow_color)));
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
    border_flags.setColor(border_color_);
    border_flags.setAlphaf(kMagnifierBorderOpacity);
    recorder.canvas()->DrawRoundRect(zoom_layer_bounds, kMagnifierRadius,
                                     border_flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  void UpdateTheme(NativeTheme* theme) {
    auto* color_provider = ColorProviderManager::Get().GetColorProviderFor(
        theme->GetColorProviderKey(nullptr));
    border_color_ = color_provider->GetColor(kColorSeparator);
    key_shadow_color_ =
        color_provider->GetColor(kColorShadowValueKeyShadowElevationThree);
    ambient_shadow_color =
        color_provider->GetColor(kColorShadowValueAmbientShadowElevationThree);
  }

 private:
  SkColor border_color_ = gfx::kPlaceholderColor;
  SkColor key_shadow_color_ = gfx::kPlaceholderColor;
  SkColor ambient_shadow_color = gfx::kPlaceholderColor;
};

TouchSelectionMagnifierAura::TouchSelectionMagnifierAura() {
  CreateMagnifierLayer();
  theme_observation_.Observe(NativeTheme::GetInstanceForNativeUi());
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
  ScopedLayerAnimationSettings settings(magnifier_layer_->GetAnimator());
  if (!magnifier_layer_->IsVisible()) {
    // Set the magnifier to appear immediately once its bounds are set.
    settings.SetTransitionDuration(base::Milliseconds(0));
    settings.SetTweenType(gfx::Tween::ZERO);
    settings.SetPreemptionStrategy(LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  } else {
    // Set the magnifier to move smoothly from its current bounds to the updated
    // bounds.
    settings.SetTransitionDuration(kMagnifierTransitionDuration);
    settings.SetTweenType(gfx::Tween::LINEAR);
    settings.SetPreemptionStrategy(
        LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  }

  const gfx::Size magnifier_parent_size =
      magnifier_layer_->parent()->bounds().size();
  const gfx::Rect focus_rect = gfx::BoundingRect(focus_start, focus_end);
  const gfx::Rect magnifier_layer_bounds =
      GetMagnifierLayerBounds(magnifier_parent_size, focus_rect.top_center());
  magnifier_layer_->SetBounds(magnifier_layer_bounds);

  // Compute the background offset to center the zoomed contents on the source
  // center. Note that the zoom layer center here is not the same as the
  // magnifier layer center, since the magnifier layer includes non-uniform
  // shadows that surround the zoomed contents.
  gfx::Rect zoom_layer_bounds = GetZoomLayerBounds();
  const gfx::Point magnifier_source_center =
      GetMagnifierSourceCenter(magnifier_parent_size, focus_rect.CenterPoint());
  const gfx::Point zoom_layer_center =
      zoom_layer_bounds.CenterPoint() +
      magnifier_layer_bounds.OffsetFromOrigin();
  const gfx::Point zoom_offset =
      gfx::PointAtOffsetFromOrigin(zoom_layer_center - magnifier_source_center);

  // The zoom_layer_ is relative to the magnifier_layer_ widget, which has been
  // shifted to avoid overlapping the content of the zoom. Un-shift the zoom
  // bounds so that its layer corresponds directly with what will be magnified
  // (backdrop filters can only access pixels under their own layer). Then use
  // a regular filter to offset the zoom's output to align with the magnifier.
  zoom_layer_bounds.Offset(-zoom_offset.x(), -zoom_offset.y());
  zoom_layer_->SetBounds(zoom_layer_bounds);
  zoom_layer_->SetLayerOffset(zoom_offset);

  if (!magnifier_layer_->IsVisible()) {
    magnifier_layer_->SetVisible(true);
  }
}

void TouchSelectionMagnifierAura::OnNativeThemeUpdated(
    NativeTheme* observed_theme) {
  border_renderer_->UpdateTheme(observed_theme);
  border_layer_->SchedulePaint(gfx::Rect(border_layer_->size()));
}

gfx::Rect TouchSelectionMagnifierAura::GetZoomedContentsBoundsForTesting()
    const {
  // The zoom_layer_ bounds are the upscaled source bounds, so we have to undo
  // that scaling to get the true contents bounds (undoes the logic inside
  // GetMagnifierSourceCenter()).
  const gfx::RectF bounds{zoom_layer_->bounds()};
  const gfx::PointF center = bounds.CenterPoint();
  const gfx::SizeF contents_size(bounds.width() / kMagnifierScale,
                                 bounds.height() / kMagnifierScale);
  const gfx::PointF contents_origin(center.x() - contents_size.width() / 2,
                                    center.y() - contents_size.height() / 2);
  gfx::Rect contents_bounds =
      gfx::ToEnclosingRect(gfx::RectF(contents_origin, contents_size));
  // The zoomed contents is drawn by the zoom layer. We just need to convert its
  // bounds to coordinates of the magnifier layer's parent layer.
  return contents_bounds + magnifier_layer_->bounds().OffsetFromOrigin();
}

gfx::Rect TouchSelectionMagnifierAura::GetMagnifierBoundsForTesting() const {
  return magnifier_layer_->bounds();
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
  zoom_layer_->SetBackgroundZoom(kMagnifierScale, 0);
  zoom_layer_->SetFillsBoundsOpaquely(false);
  // BackdropFilterBounds applies after the backdrop filter (the zoom effect)
  // but before anything else, meaning its clipping effect is transformed by
  // the layer_offset() filter operation. SetRoundedCornerRadius() applies too
  // late and is affected by the layer offset, so would incorrectly clip the
  // zoomed contents.
  zoom_layer_->SetBackdropFilterBounds(
      gfx::RRectF{gfx::RectF(kMagnifierSize), kMagnifierRadius});
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
