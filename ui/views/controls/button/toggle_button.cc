// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/toggle_button.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// Constants are measured in dip.
constexpr gfx::Size kTrackSize = gfx::Size(26, 16);
constexpr int kThumbInset = -4;
constexpr int kThumbInsetSelected = -2;
constexpr int kThumbPressedOutset = 1;
constexpr int kHoverDiameter = 20;
constexpr float kBorderStrokeWidth = 1.0f;

const gfx::Size GetTrackSize() {
  return kTrackSize;
}

int GetThumbInset(bool is_on) {
  return is_on ? kThumbInsetSelected : kThumbInset;
}

std::optional<SkColor> GetSkColorFromVariant(
    const absl::variant<ui::ColorId, SkColor>& color_variant) {
  return absl::holds_alternative<SkColor>(color_variant)
             ? std::make_optional(absl::get<SkColor>(color_variant))
             : std::nullopt;
}

SkColor ConvertVariantToSkColor(
    const absl::variant<ui::ColorId, SkColor> color_variant,
    const ui::ColorProvider* color_provider) {
  return absl::holds_alternative<SkColor>(color_variant)
             ? absl::get<SkColor>(color_variant)
             : color_provider->GetColor(absl::get<ui::ColorId>(color_variant));
}

}  // namespace

class ToggleButton::FocusRingHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const ToggleButton*>(view)->GetFocusRingPath();
  }
};

// Class representing the thumb (the circle that slides horizontally).
class ToggleButton::ThumbView : public View {
  METADATA_HEADER(ThumbView, View)

 public:
  explicit ThumbView(bool has_shadow) : has_shadow_(has_shadow) {
    // Make the thumb behave as part of the parent for event handling.
    SetCanProcessEventsWithinSubtree(false);
  }
  ThumbView(const ThumbView&) = delete;
  ThumbView& operator=(const ThumbView&) = delete;
  ~ThumbView() override = default;

  void Update(const gfx::Rect& bounds,
              float color_ratio,
              float hover_ratio,
              bool is_on,
              bool is_hovered) {
    SetBoundsRect(bounds);
    color_ratio_ = color_ratio;
    hover_ratio_ = hover_ratio;
    is_on_ = is_on;
    is_hovered_ = is_hovered;
    SchedulePaint();
  }

  // Returns the extra space needed to draw the shadows around the thumb. Since
  // the extra space is around the thumb, the insets will be negative.
  gfx::Insets GetShadowOutsets() {
    return has_shadow_ ? gfx::Insets(-kShadowBlur) +
                             gfx::Vector2d(kShadowOffsetX, kShadowOffsetY)
                       : gfx::Insets();
  }

  void SetThumbColor(bool is_on, SkColor thumb_color) {
    (is_on ? thumb_on_color_ : thumb_off_color_) = thumb_color;
  }

  std::optional<SkColor> GetThumbColor(bool is_on) const {
    return GetSkColorFromVariant(is_on ? thumb_on_color_ : thumb_off_color_);
  }

 private:
  static constexpr int kShadowOffsetX = 0;
  static constexpr int kShadowOffsetY = 1;
  static constexpr int kShadowBlur = 2;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    const float dsf = canvas->UndoDeviceScaleFactor();
    const ui::ColorProvider* color_provider = GetColorProvider();
    cc::PaintFlags thumb_flags;
    if (has_shadow_) {
      std::vector<gfx::ShadowValue> shadows;
      gfx::ShadowValue shadow(
          gfx::Vector2d(kShadowOffsetX, kShadowOffsetY), 2 * kShadowBlur,
          color_provider->GetColor(ui::kColorToggleButtonShadow));
      shadows.push_back(shadow.Scale(dsf));
      thumb_flags.setLooper(gfx::CreateShadowDrawLooper(shadows));
    }
    thumb_flags.setAntiAlias(true);
    const SkColor thumb_on_color =
        ConvertVariantToSkColor(thumb_on_color_, color_provider);
    const SkColor thumb_off_color =
        ConvertVariantToSkColor(thumb_off_color_, color_provider);
    SkColor thumb_color =
        color_utils::AlphaBlend(thumb_on_color, thumb_off_color, color_ratio_);
    if (is_hovered_ && is_on_) {
      // This will blend and additional color into the "on" state thumb color
      // while the view is hovered. This will also take into account both the
      // off->on color animating along with the hover animation. Those
      // animations are running independently.
      thumb_color = color_utils::AlphaBlend(
          color_provider->GetColor(ui::kColorToggleButtonThumbOnHover),
          thumb_color, hover_ratio_);
    }
    thumb_flags.setColor(thumb_color);

    // We want the circle to have an integer pixel diameter and to be aligned
    // with pixel boundaries, so we scale dip bounds to pixel bounds and round.
    gfx::RectF thumb_bounds(GetLocalBounds());
    thumb_bounds.Inset(-gfx::InsetsF(GetShadowOutsets()));
    thumb_bounds.Inset(0.5f);
    thumb_bounds.Scale(dsf);
    thumb_bounds = gfx::RectF(gfx::ToEnclosingRect(thumb_bounds));
    canvas->DrawCircle(thumb_bounds.CenterPoint(), thumb_bounds.height() / 2.f,
                       thumb_flags);
  }

  void OnEnabledStateChanged() {
    // If using default color ID, update it according to the enabled state.
    if (absl::holds_alternative<ui::ColorId>(thumb_on_color_)) {
      thumb_on_color_ = GetEnabled() ? ui::kColorToggleButtonThumbOn
                                     : ui::kColorToggleButtonThumbOnDisabled;
    }

    if (absl::holds_alternative<ui::ColorId>(thumb_off_color_)) {
      thumb_off_color_ = GetEnabled() ? ui::kColorToggleButtonThumbOff
                                      : ui::kColorToggleButtonThumbOffDisabled;
    }
  }

  // Indicate if the thumb has shadow.
  const bool has_shadow_;

  // Colors used for the thumb.
  absl::variant<ui::ColorId, SkColor> thumb_on_color_ =
      ui::kColorToggleButtonThumbOn;
  absl::variant<ui::ColorId, SkColor> thumb_off_color_ =
      ui::kColorToggleButtonThumbOff;

  bool is_on_ = false;
  bool is_hovered_ = false;

  // Color ratio between 0 and 1 that controls the thumb color.
  float color_ratio_ = 0.0f;
  // Color ratio between 0 and 1 that controls the thumb hover color.
  float hover_ratio_ = 0.0f;

  // Callback when the enabled state changes.
  base::CallbackListSubscription enabled_state_changed_subscription_{
      AddEnabledChangedCallback(
          base::BindRepeating(&ThumbView::OnEnabledStateChanged,
                              base::Unretained(this)))};
};

ToggleButton::ToggleButton(PressedCallback callback)
    : ToggleButton(std::move(callback), /*has_thumb_shadow=*/false) {}

ToggleButton::ToggleButton(PressedCallback callback, bool has_thumb_shadow)
    : Button(std::move(callback)) {
  slide_animation_.SetSlideDuration(base::Milliseconds(80));
  slide_animation_.SetTweenType(gfx::Tween::LINEAR);
  hover_animation_.SetSlideDuration(base::Milliseconds(250));
  hover_animation_.SetTweenType(gfx::Tween::LINEAR);
  thumb_view_ = AddChildView(std::make_unique<ThumbView>(has_thumb_shadow));
  InkDrop::Get(this)->SetMode(InkDropHost::InkDropMode::ON);
  InkDrop::Get(this)->SetLayerRegion(LayerRegion::kAbove);
  // Do not set a clip, allow the ink drop to burst out.
  // TODO(pbos): Consider an explicit InkDrop API to not use a clip rect / mask.
  views::InstallEmptyHighlightPathGenerator(this);
  // InkDrop event triggering is handled in NotifyClick().
  SetHasInkDropActionOnClick(false);
  InkDrop::UseInkDropForSquareRipple(InkDrop::Get(this),
                                     /*highlight_on_hover=*/true,
                                     /*highlight_on_focus=*/false,
                                     /*show_highlight_on_ripple=*/true);
  InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](ToggleButton* host,
         gfx::Insets insets) -> std::unique_ptr<InkDropRipple> {
        gfx::Rect rect = host->thumb_view_->GetLocalBounds();
        rect.Inset(insets);
        const SkColor pressed_color = host->GetPressedColor();
        const float pressed_alpha = SkColorGetA(pressed_color);
        std::unique_ptr<SquareInkDropRipple> ripple =
            std::make_unique<SquareInkDropRipple>(
                InkDrop::Get(host), gfx::Size(kHoverDiameter, kHoverDiameter),
                kHoverDiameter / 2, rect.size(), rect.height() / 2,
                rect.CenterPoint(), SkColorSetA(pressed_color, SK_AlphaOPAQUE),
                pressed_alpha / SK_AlphaOPAQUE);
        ripple->set_activated_shape(
            views::SquareInkDropRipple::ActivatedShape::kCircle);
        return ripple;
      },
      this, -thumb_view_->GetShadowOutsets()));
  InkDrop::Get(this)->SetBaseColorCallback(base::BindRepeating(
      [](ToggleButton* host) {
        return host->GetTrackColor(host->GetIsOn() || host->HasFocus());
      },
      this));
  InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
      [](ToggleButton* host) {
        const gfx::Rect thumb_bounds = host->thumb_view_->GetLocalBounds();
        const gfx::Size thumb_size(kHoverDiameter, kHoverDiameter);
        const SkColor hover_color = host->GetHoverColor();
        const float hover_alpha = SkColorGetA(hover_color);
        auto ink_drop_highlight = std::make_unique<InkDropHighlight>(
            thumb_size, thumb_size.height() / 2,
            gfx::PointF(thumb_bounds.CenterPoint()),
            SkColorSetA(hover_color, SK_AlphaOPAQUE));
        ink_drop_highlight->set_visible_opacity(hover_alpha / SK_AlphaOPAQUE);
        return ink_drop_highlight;
      },
      this));

  // Even though ToggleButton doesn't paint anything, declare us as flipped in
  // RTL mode so that FocusRing correctly flips as well.
  SetFlipCanvasOnPaintForRTLUI(true);
  SetInstallFocusRingOnFocus(true);
  FocusRing::Get(this)->SetPathGenerator(
      std::make_unique<FocusRingHighlightPathGenerator>());

  GetViewAccessibility().SetRole(ax::mojom::Role::kSwitch);
}

ToggleButton::~ToggleButton() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerFromRegions(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

void ToggleButton::AnimateIsOn(bool is_on) {
  if (GetIsOn() == is_on) {
    return;
  }
  if (is_on) {
    slide_animation_.Show();
  } else {
    slide_animation_.Hide();
  }
  GetViewAccessibility().SetCheckedState(GetIsOn()
                                             ? ax::mojom::CheckedState::kTrue
                                             : ax::mojom::CheckedState::kFalse);
  OnPropertyChanged(&slide_animation_, kPropertyEffectsNone);
}

void ToggleButton::SetIsOn(bool is_on) {
  if ((GetIsOn() == is_on) && !slide_animation_.is_animating()) {
    return;
  }
  slide_animation_.Reset(is_on ? 1.0 : 0.0);
  GetViewAccessibility().SetCheckedState(GetIsOn()
                                             ? ax::mojom::CheckedState::kTrue
                                             : ax::mojom::CheckedState::kFalse);
  UpdateThumb();
  OnPropertyChanged(&slide_animation_, kPropertyEffectsPaint);
}

bool ToggleButton::GetIsOn() const {
  return slide_animation_.IsShowing();
}

void ToggleButton::SetThumbOnColor(SkColor thumb_on_color) {
  thumb_view_->SetThumbColor(true /* is_on */, thumb_on_color);
}

std::optional<SkColor> ToggleButton::GetThumbOnColor() const {
  return thumb_view_->GetThumbColor(true);
}

void ToggleButton::SetThumbOffColor(SkColor thumb_off_color) {
  thumb_view_->SetThumbColor(false /* is_on */, thumb_off_color);
}

std::optional<SkColor> ToggleButton::GetThumbOffColor() const {
  return thumb_view_->GetThumbColor(false);
}

void ToggleButton::SetTrackOnColor(SkColor track_on_color) {
  track_on_color_ = track_on_color;
}

std::optional<SkColor> ToggleButton::GetTrackOnColor() const {
  return GetSkColorFromVariant(track_on_color_);
}

void ToggleButton::SetTrackOffColor(SkColor track_off_color) {
  track_off_color_ = track_off_color;
}

std::optional<SkColor> ToggleButton::GetTrackOffColor() const {
  return GetSkColorFromVariant(track_off_color_);
}

void ToggleButton::SetInnerBorderEnabled(bool enabled) {
  if (inner_border_enabled_ == enabled) {
    return;
  }
  inner_border_enabled_ = enabled;
  OnPropertyChanged(&inner_border_enabled_, kPropertyEffectsPaint);
}

bool ToggleButton::GetInnerBorderEnabled() const {
  return inner_border_enabled_;
}

void ToggleButton::SetAcceptsEvents(bool accepts_events) {
  if (GetAcceptsEvents() == accepts_events) {
    return;
  }
  accepts_events_ = accepts_events;
  OnPropertyChanged(&accepts_events_, kPropertyEffectsNone);
}

bool ToggleButton::GetAcceptsEvents() const {
  return accepts_events_;
}

void ToggleButton::AddLayerToRegion(ui::Layer* layer,
                                    views::LayerRegion region) {
  // Ink-drop layers should go above/below the ThumbView.
  thumb_view_->AddLayerToRegion(layer, region);
}

void ToggleButton::RemoveLayerFromRegions(ui::Layer* layer) {
  thumb_view_->RemoveLayerFromRegions(layer);
}

gfx::Size ToggleButton::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  gfx::Rect rect(GetTrackSize());
  rect.Inset(-GetInsets());
  return rect.size();
}

gfx::Rect ToggleButton::GetTrackBounds() const {
  gfx::Rect track_bounds(GetContentsBounds());
  track_bounds.ClampToCenteredSize(GetTrackSize());
  return track_bounds;
}

gfx::Rect ToggleButton::GetThumbBounds() const {
  gfx::Rect thumb_bounds(GetTrackBounds());
  thumb_bounds.Inset(gfx::Insets(-GetThumbInset(GetIsOn())));
  thumb_bounds.set_x(thumb_bounds.x() +
                     slide_animation_.GetCurrentValue() *
                         (thumb_bounds.width() - thumb_bounds.height()));
  // The thumb is a circle, so the width should match the height.
  thumb_bounds.set_width(thumb_bounds.height());
  thumb_bounds.Inset(thumb_view_->GetShadowOutsets());
  if (GetState() == STATE_PRESSED) {
    thumb_bounds.Outset(kThumbPressedOutset);
  }
  return thumb_bounds;
}

double ToggleButton::GetAnimationProgress() const {
  return slide_animation_.GetCurrentValue();
}

void ToggleButton::UpdateThumb() {
  thumb_view_->Update(GetThumbBounds(),
                      static_cast<float>(slide_animation_.GetCurrentValue()),
                      static_cast<float>(hover_animation_.GetCurrentValue()),
                      GetIsOn(), IsMouseHovered());
  if (IsMouseHovered()) {
    InkDrop::Get(this)->GetInkDrop()->SetHovered(
        !slide_animation_.is_animating());
  }
  if (FocusRing::Get(this)) {
    // Updating the thumb changes the result of GetFocusRingPath(), make sure
    // the focus ring gets updated to match this new state.
    FocusRing::Get(this)->InvalidateLayout();
    FocusRing::Get(this)->SchedulePaint();
  }
}

SkColor ToggleButton::GetTrackColor(bool is_on) const {
  return ConvertVariantToSkColor(is_on ? track_on_color_ : track_off_color_,
                                 GetColorProvider());
}

SkColor ToggleButton::GetHoverColor() const {
  return GetColorProvider()->GetColor(ui::kColorToggleButtonHover);
}

SkColor ToggleButton::GetPressedColor() const {
  return GetColorProvider()->GetColor(ui::kColorToggleButtonPressed);
}

bool ToggleButton::CanAcceptEvent(const ui::Event& event) {
  return GetAcceptsEvents() && Button::CanAcceptEvent(event);
}

void ToggleButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateThumb();
}

void ToggleButton::OnThemeChanged() {
  Button::OnThemeChanged();
  SchedulePaint();
}

void ToggleButton::NotifyClick(const ui::Event& event) {
  AnimateIsOn(!GetIsOn());

  InkDrop::Get(this)->AnimateToState(InkDropState::ACTION_TRIGGERED,
                                     ui::LocatedEvent::FromIfValid(&event));

  Button::NotifyClick(event);
}

void ToggleButton::StateChanged(ButtonState old_state) {
  Button::StateChanged(old_state);

  // Update default track color ID and propagate the enabled state to the thumb.
  const bool enabled = GetState() != ButtonState::STATE_DISABLED;
  if (absl::holds_alternative<ui::ColorId>(track_on_color_)) {
    track_on_color_ = enabled ? ui::kColorToggleButtonTrackOn
                              : ui::kColorToggleButtonTrackOnDisabled;
  }

  if (absl::holds_alternative<ui::ColorId>(track_off_color_)) {
    track_off_color_ = enabled ? ui::kColorToggleButtonTrackOff
                               : ui::kColorToggleButtonTrackOffDisabled;
  }

  thumb_view_->SetEnabled(enabled);

  // Update thumb bounds.
  if (GetState() == STATE_PRESSED || old_state == STATE_PRESSED) {
    UpdateThumb();
  } else if (GetState() == STATE_HOVERED || old_state == STATE_HOVERED) {
    if (old_state == STATE_HOVERED) {
      hover_animation_.Hide();
    } else {
      hover_animation_.Show();
    }
    UpdateThumb();
  }
}

SkPath ToggleButton::GetFocusRingPath() const {
  SkPath path;
  gfx::RectF bounds(GetTrackBounds());
  const SkRect sk_rect = gfx::RectFToSkRect(bounds);
  const float corner_radius = sk_rect.height() / 2;
  path.addRoundRect(sk_rect, corner_radius, corner_radius);
  return path;
}

void ToggleButton::PaintButtonContents(gfx::Canvas* canvas) {
  // Paint the toggle track. To look sharp even at fractional scale factors,
  // round up to pixel boundaries.
  canvas->Save();
  float dsf = canvas->UndoDeviceScaleFactor();
  gfx::RectF track_rect(GetTrackBounds());
  track_rect.Scale(dsf);
  track_rect = gfx::RectF(gfx::ToEnclosingRect(track_rect));
  const SkScalar radius = track_rect.height() / 2;
  cc::PaintFlags track_flags;
  track_flags.setAntiAlias(true);
  const float color_ratio =
      static_cast<float>(slide_animation_.GetCurrentValue());
  track_flags.setColor(color_utils::AlphaBlend(
      GetTrackColor(true), GetTrackColor(false), color_ratio));
  canvas->DrawRoundRect(track_rect, radius, track_flags);
  if (!GetIsOn() && inner_border_enabled_) {
    track_rect.Inset(kBorderStrokeWidth * dsf / 2.0f);
    track_flags.setColor(
        GetColorProvider()->GetColor(ui::kColorToggleButtonShadow));
    track_flags.setStrokeWidth(kBorderStrokeWidth * dsf);
    track_flags.setStyle(cc::PaintFlags::kStroke_Style);
    canvas->DrawRoundRect(track_rect, radius, track_flags);
  }
  canvas->Restore();
}

void ToggleButton::AnimationEnded(const gfx::Animation* animation) {
  if (animation == &slide_animation_ && IsMouseHovered()) {
    InkDrop::Get(this)->GetInkDrop()->SetHovered(true);
  }
}

void ToggleButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &slide_animation_ || animation == &hover_animation_) {
    // TODO(varkha, estade): The thumb is using its own view. Investigate if
    // repainting in every animation step to update colors could be avoided.
    UpdateThumb();
    SchedulePaint();
    return;
  }
  Button::AnimationProgressed(animation);
}

BEGIN_METADATA(ToggleButton, ThumbView)
END_METADATA

BEGIN_METADATA(ToggleButton)
ADD_PROPERTY_METADATA(bool, IsOn)
ADD_PROPERTY_METADATA(bool, InnerBorderEnabled)
ADD_PROPERTY_METADATA(bool, AcceptsEvents)
END_METADATA

}  // namespace views
