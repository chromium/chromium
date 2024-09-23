// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/frame_caption_button.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_flags.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/window/hit_test_utils.h"

namespace views {

namespace {

// Ink drop parameters.
constexpr float kInkDropVisibleOpacity = 0.06f;

// The duration of the fade out animation of the old icon during a crossfade
// animation as a ratio of the duration of |swap_images_animation_|.
constexpr float kFadeOutRatio = 0.5f;

// The ratio applied to the button's alpha when the button is disabled.
constexpr float kDisabledButtonAlphaRatio = 0.5f;

}  // namespace

// Custom highlight path generator for clipping ink drops and drawing focus
// rings.
class FrameCaptionButton::HighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  explicit HighlightPathGenerator(FrameCaptionButton* frame_caption_button)
      : frame_caption_button_(frame_caption_button) {}
  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;
  ~HighlightPathGenerator() override = default;

  // views::HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::Rect bounds = gfx::ToRoundedRect(rect);
    bounds.Inset(frame_caption_button_->GetInkdropInsets(bounds.size()));
    return gfx::RRectF(gfx::RectF(bounds),
                       frame_caption_button_->GetInkDropCornerRadius());
  }

 private:
  const raw_ptr<FrameCaptionButton> frame_caption_button_;
};

FrameCaptionButton::FrameCaptionButton(PressedCallback callback,
                                       CaptionButtonIcon icon,
                                       int hit_test_type)
    : Button(std::move(callback)),
      icon_(icon),
      swap_images_animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  views::SetHitTestComponent(this, hit_test_type);
  // Not focusable by default, only for accessibility.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  SetAnimateOnStateChange(true);
  swap_images_animation_->Reset(1);

  SetHasInkDropActionOnClick(true);
  InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  InkDrop::Get(this)->SetVisibleOpacity(kInkDropVisibleOpacity);
  InkDrop::UseInkDropWithoutAutoHighlight(InkDrop::Get(this),
                                          /*highlight_on_hover=*/false);
  InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](FrameCaptionButton* host) -> std::unique_ptr<views::InkDropRipple> {
        return std::make_unique<views::FloodFillInkDropRipple>(
            InkDrop::Get(host), host->size(),
            host->GetInkdropInsets(host->size()),
            InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
            InkDrop::Get(host)->GetBaseColor(),
            InkDrop::Get(host)->GetVisibleOpacity());
      },
      this));

  views::HighlightPathGenerator::Install(
      this, std::make_unique<HighlightPathGenerator>(this));

  // Do not flip the gfx::Canvas passed to the OnPaint() method. The snap left
  // and snap right button icons should not be flipped. The other icons are
  // horizontally symmetrical.
}

FrameCaptionButton::~FrameCaptionButton() = default;

// static
SkColor FrameCaptionButton::GetButtonColor(SkColor background_color) {
  // Use IsDark() to change target colors instead of PickContrastingColor(), so
  // that DefaultFrameHeader::GetTitleColor() (which uses different target
  // colors) can change between light/dark targets at the same time.  It looks
  // bad when the title and caption buttons disagree about whether to be light
  // or dark.
  const SkColor default_foreground = color_utils::IsDark(background_color)
                                         ? gfx::kGoogleGrey200
                                         : gfx::kGoogleGrey700;
  const SkColor high_contrast_foreground =
      color_utils::GetColorWithMaxContrast(background_color);
  return color_utils::BlendForMinContrast(
             default_foreground, background_color, high_contrast_foreground,
             color_utils::kMinimumVisibleContrastRatio)
      .color;
}

// static
float FrameCaptionButton::GetInactiveButtonColorAlphaRatio() {
  return 0.38f;
}

void FrameCaptionButton::SetImage(CaptionButtonIcon icon,
                                  Animate animate,
                                  const gfx::VectorIcon& icon_definition) {
  // If the button is not yet in a widget, OnThemeChanged() will call back
  // here once it is, updating the color as needed.
  SkColor icon_color = gfx::kPlaceholderColor;
  if (absl::holds_alternative<SkColor>(color_)) {
    icon_color = GetButtonColor(absl::get<SkColor>(color_));
  } else if (const auto* color_provider = GetColorProvider()) {
    icon_color = color_provider->GetColor(absl::get<ui::ColorId>(color_));
  }

  gfx::ImageSkia new_icon_image =
      gfx::CreateVectorIcon(icon_definition, icon_color);

  // The early return is dependent on |animate| because callers use SetImage()
  // with Animate::kNo to progress the crossfade animation to the end.
  if (icon == icon_ &&
      (animate == Animate::kYes || !swap_images_animation_->is_animating()) &&
      new_icon_image.BackedBySameObjectAs(icon_image_)) {
    return;
  }

  if (animate == Animate::kYes)
    crossfade_icon_image_ = icon_image_;

  icon_ = icon;
  icon_definition_ = &icon_definition;
  icon_image_ = new_icon_image;

  if (animate == Animate::kYes) {
    swap_images_animation_->Reset(0);
    swap_images_animation_->SetSlideDuration(base::Milliseconds(200));
    swap_images_animation_->Show();
  } else {
    swap_images_animation_->Reset(1);
  }

  if (GetWidget()) {
    SchedulePaint();
  }
}

bool FrameCaptionButton::IsAnimatingImageSwap() const {
  return swap_images_animation_->is_animating();
}

void FrameCaptionButton::SetAlpha(SkAlpha alpha) {
  if (alpha_ != alpha) {
    alpha_ = alpha;
    SchedulePaint();
  }
}

void FrameCaptionButton::OnGestureEvent(ui::GestureEvent* event) {
  // Button does not become pressed when the user drags off and then back
  // onto the button. Make FrameCaptionButton pressed in this case because this
  // behavior is more consistent with AlternateFrameSizeButton.
  if (event->type() == ui::EventType::kGestureScrollBegin ||
      event->type() == ui::EventType::kGestureScrollUpdate) {
    if (HitTestPoint(event->location())) {
      SetState(STATE_PRESSED);
      RequestFocus();
      event->StopPropagation();
    } else {
      SetState(STATE_NORMAL);
    }
  } else if (event->type() == ui::EventType::kGestureScrollEnd) {
    if (HitTestPoint(event->location())) {
      SetState(STATE_HOVERED);
      NotifyClick(*event);
      event->StopPropagation();
    }
  }

  if (!event->handled())
    Button::OnGestureEvent(event);
}

views::PaintInfo::ScaleType FrameCaptionButton::GetPaintScaleType() const {
  return views::PaintInfo::ScaleType::kUniformScaling;
}

void FrameCaptionButton::SetBackgroundColor(SkColor background_color) {
  if (absl::holds_alternative<SkColor>(color_) &&
      absl::get<SkColor>(color_) == background_color) {
    return;
  }

  color_ = background_color;
  MaybeRefreshIconAndInkdropBaseColor();
}

void FrameCaptionButton::SetIconColorId(ui::ColorId icon_color_id) {
  if (absl::holds_alternative<ui::ColorId>(color_) &&
      absl::get<ui::ColorId>(color_) == icon_color_id) {
    return;
  }

  color_ = icon_color_id;
  MaybeRefreshIconAndInkdropBaseColor();
}

SkColor FrameCaptionButton::GetBackgroundColor() const {
  return absl::get<SkColor>(color_);
}

void FrameCaptionButton::SetInkDropCornerRadius(int ink_drop_corner_radius) {
  ink_drop_corner_radius_ = ink_drop_corner_radius;
  // Changes to |ink_drop_corner_radius| will affect the ink drop. Therefore
  // this effect is handled by the ink drop.
  OnPropertyChanged(&ink_drop_corner_radius_, kPropertyEffectsNone);
}

int FrameCaptionButton::GetInkDropCornerRadius() const {
  return ink_drop_corner_radius_;
}

base::CallbackListSubscription
FrameCaptionButton::AddBackgroundColorChangedCallback(
    PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(&color_, callback);
}

void FrameCaptionButton::SetPaintAsActive(bool paint_as_active) {
  if (paint_as_active == paint_as_active_)
    return;
  paint_as_active_ = paint_as_active;
  OnPropertyChanged(&paint_as_active_, kPropertyEffectsPaint);
}

bool FrameCaptionButton::GetPaintAsActive() const {
  return paint_as_active_;
}

void FrameCaptionButton::DrawHighlight(gfx::Canvas* canvas,
                                       cc::PaintFlags flags) {
  const gfx::Point center(GetMirroredRect(GetContentsBounds()).CenterPoint());
  canvas->DrawCircle(center, ink_drop_corner_radius_, flags);
}

void FrameCaptionButton::DrawIconContents(gfx::Canvas* canvas,
                                          gfx::ImageSkia image,
                                          int x,
                                          int y,
                                          cc::PaintFlags flags) {
  canvas->DrawImageInt(image, x, y, flags);
}

gfx::Size FrameCaptionButton::GetInkDropSize() const {
  return gfx::Size(2 * GetInkDropCornerRadius(), 2 * GetInkDropCornerRadius());
}

gfx::Insets FrameCaptionButton::GetInkdropInsets(
    const gfx::Size& button_size) const {
  return gfx::Insets::VH((button_size.height() - GetInkDropSize().height()) / 2,
                         (button_size.width() - GetInkDropSize().width()) / 2);
}

void FrameCaptionButton::MaybeRefreshIconAndInkdropBaseColor() {
  if (!GetColorProvider()) {
    return;
  }

  if (icon_definition_) {
    SetImage(icon_, Animate::kNo, *icon_definition_);
  }
  UpdateInkDropBaseColor();
}

void FrameCaptionButton::PaintButtonContents(gfx::Canvas* canvas) {
  constexpr SkAlpha kHighlightVisibleOpacity = 0x14;
  SkAlpha highlight_alpha = SK_AlphaTRANSPARENT;
  if (hover_animation().is_animating()) {
    highlight_alpha =
        static_cast<SkAlpha>(hover_animation().CurrentValueBetween(
            SK_AlphaTRANSPARENT, kHighlightVisibleOpacity));
  } else if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED) {
    // Painting a circular highlight in both "hovered" and "pressed" states
    // simulates and ink drop highlight mode of
    // AutoHighlightMode::SHOW_ON_RIPPLE.
    highlight_alpha = kHighlightVisibleOpacity;
  }

  if (highlight_alpha != SK_AlphaTRANSPARENT) {
    // We paint the highlight manually here rather than relying on the ink drop
    // highlight as it doesn't work well when the button size is changing while
    // the window is moving as a result of the animation from normal to
    // maximized state or vice versa. https://crbug.com/840901.
    cc::PaintFlags flags;
    flags.setColor(InkDrop::Get(this)->GetBaseColor());
    flags.setAlphaf(highlight_alpha / 255.0f);
    DrawHighlight(canvas, flags);
  }

  SkAlpha icon_alpha =
      static_cast<SkAlpha>(swap_images_animation_->CurrentValueBetween(
          SK_AlphaTRANSPARENT, SK_AlphaOPAQUE));
  SkAlpha crossfade_icon_alpha = 0;
  if (icon_alpha < base::ClampRound<SkAlpha>(kFadeOutRatio * SK_AlphaOPAQUE)) {
    crossfade_icon_alpha =
        base::ClampRound<SkAlpha>(SK_AlphaOPAQUE - icon_alpha / kFadeOutRatio);
  }

  gfx::Rect icon_bounds = GetContentsBounds();
  icon_bounds.ClampToCenteredSize(icon_image_.size());
  const int icon_bounds_x = icon_bounds.x();
  const int icon_bounds_y = icon_bounds.y();

  if (crossfade_icon_alpha > 0 && !crossfade_icon_image_.isNull()) {
    canvas->SaveLayerAlpha(GetAlphaForIcon(alpha_));
    cc::PaintFlags flags;
    flags.setAlphaf(icon_alpha / 255.0f);
    DrawIconContents(canvas, icon_image_, icon_bounds_x, icon_bounds_y, flags);

    flags.setAlphaf(crossfade_icon_alpha / 255.0f);
    flags.setBlendMode(SkBlendMode::kPlus);
    DrawIconContents(canvas, crossfade_icon_image_, icon_bounds_x,
                     icon_bounds_y, flags);
    canvas->Restore();
  } else {
    if (!swap_images_animation_->is_animating())
      icon_alpha = alpha_;
    cc::PaintFlags flags;
    flags.setAlphaf(GetAlphaForIcon(icon_alpha) / 255.0f);
    DrawIconContents(canvas, icon_image_, icon_bounds_x, icon_bounds_y, flags);
  }
}

void FrameCaptionButton::OnThemeChanged() {
  views::Button::OnThemeChanged();

  MaybeRefreshIconAndInkdropBaseColor();
}

SkAlpha FrameCaptionButton::GetAlphaForIcon(SkAlpha base_alpha) const {
  if (!GetEnabled())
    return base::ClampRound<SkAlpha>(base_alpha * kDisabledButtonAlphaRatio);

  if (paint_as_active_)
    return base_alpha;

  // Paint icons as active when they are hovered over or pressed.
  double inactive_alpha = GetInactiveButtonColorAlphaRatio();

  if (hover_animation().is_animating()) {
    inactive_alpha =
        hover_animation().CurrentValueBetween(inactive_alpha, 1.0f);
  } else if (GetState() == STATE_PRESSED || GetState() == STATE_HOVERED) {
    inactive_alpha = 1.0f;
  }
  return base::ClampRound<SkAlpha>(base_alpha * inactive_alpha);
}

void FrameCaptionButton::UpdateInkDropBaseColor() {
  using color_utils::GetColorWithMaxContrast;

  // A typical implementation would simply do
  // GetColorWithMaxContrast(background_color_).  However, this could look odd
  // if we use a light button glyph and dark ink drop or vice versa.  So
  // instead, use the lightest/darkest color in the same direction as the button
  // glyph color.
  // TODO(pkasting): It would likely be better to make the button glyph always
  // be an alpha-blended version of GetColorWithMaxContrast(background_color_).
  const SkColor button_color =
      absl::holds_alternative<ui::ColorId>(color_)
          ? GetColorProvider()->GetColor(absl::get<ui::ColorId>(color_))
          : GetButtonColor(absl::get<SkColor>(color_));

  InkDrop::Get(this)->SetBaseColor(
      GetColorWithMaxContrast(GetColorWithMaxContrast(button_color)));
}

BEGIN_METADATA(FrameCaptionButton)
ADD_PROPERTY_METADATA(int, InkDropCornerRadius)
ADD_READONLY_PROPERTY_METADATA(CaptionButtonIcon, Icon)
ADD_PROPERTY_METADATA(bool, PaintAsActive)
END_METADATA

}  // namespace views

DEFINE_ENUM_CONVERTERS(
    views::CaptionButtonIcon,
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_MINIMIZE,
     u"CAPTION_BUTTON_ICON_MINIMIZE"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
     u"CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_CLOSE,
     u"CAPTION_BUTTON_ICON_CLOSE"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
     u"CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
     u"CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_BACK,
     u"CAPTION_BUTTON_ICON_BACK"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_LOCATION,
     u"CAPTION_BUTTON_ICON_LOCATION"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_MENU,
     u"CAPTION_BUTTON_ICON_MENU"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_ZOOM,
     u"CAPTION_BUTTON_ICON_ZOOM"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_CENTER,
     u"CAPTION_BUTTON_ICON_CENTER"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_FLOAT,
     u"CAPTION_BUTTON_ICON_FLOAT"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_CUSTOM,
     u"CAPTION_BUTTON_ICON_CUSTOM"},
    {views::CaptionButtonIcon::CAPTION_BUTTON_ICON_COUNT,
     u"CAPTION_BUTTON_ICON_COUNT"})
