// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Color of slider at the active and the disabled state, respectively.
constexpr SkColor kActiveColor = SkColorSetARGB(0xFF, 0x25, 0x81, 0xDF);
constexpr SkColor kDisabledColor = SkColorSetARGB(0x6E, 0xF1, 0xF3, 0xF4);

constexpr uint8_t kActiveColorAlpha = 0x40;
constexpr uint8_t kDisabledColorAlpha = 0x19;

// The thickness of the slider.
constexpr int kLineThickness = 2;

// The radius used to draw rounded slider ends.
constexpr float kSliderRoundedRadius = 2.f;

// The padding used to hide the slider underneath the thumb.
constexpr int kSliderPadding = 2;

// The radius of the thumb and the highlighted thumb of the slider,
// respectively.
constexpr float kThumbRadius = 4.f;
constexpr float kThumbWidth = 2 * kThumbRadius;
constexpr float kThumbHighlightRadius = 12.f;

}  // namespace

Slider::Slider(SliderListener* listener)
    : listener_(listener),
      highlight_animation_(this),
      pending_accessibility_value_change_(false) {
  highlight_animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(150));
  EnableCanvasFlippingForRTLUI(true);
#if defined(OS_MACOSX)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif

  SchedulePaint();
}

Slider::~Slider() = default;

float Slider::GetValue() const {
  return value_;
}

void Slider::SetValue(float value) {
  SetValueInternal(value, VALUE_CHANGED_BY_API);
}

bool Slider::GetEnableAccessibilityEvents() const {
  return accessibility_events_enabled_;
}

void Slider::SetEnableAccessibilityEvents(bool enabled) {
  if (accessibility_events_enabled_ == enabled)
    return;
  accessibility_events_enabled_ = enabled;
  OnPropertyChanged(&accessibility_events_enabled_, kPropertyEffectsNone);
}

bool Slider::GetIsActive() const {
  return is_active_;
}

void Slider::SetIsActive(bool is_active) {
  if (is_active == is_active_)
    return;
  is_active_ = is_active;
  OnPropertyChanged(&is_active_, kPropertyEffectsPaint);
}

float Slider::GetAnimatingValue() const{
  return move_animation_ && move_animation_->is_animating()
             ? move_animation_->CurrentValueBetween(initial_animating_value_,
                                                    value_)
             : value_;
}

void Slider::SetHighlighted(bool is_highlighted) {
  if (is_highlighted)
    highlight_animation_.Show();
  else
    highlight_animation_.Hide();
}

void Slider::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &highlight_animation_) {
    thumb_highlight_radius_ =
        animation->CurrentValueBetween(kThumbRadius, kThumbHighlightRadius);
  }

  SchedulePaint();
}

void Slider::AnimationEnded(const gfx::Animation* animation) {
  if (animation == move_animation_.get()) {
    move_animation_.reset();
    return;
  }
  DCHECK_EQ(animation, &highlight_animation_);
}

void Slider::SetValueInternal(float value, SliderChangeReason reason) {
  bool old_value_valid = value_is_valid_;

  value_is_valid_ = true;
  if (value < 0.0)
    value = 0.0;
  else if (value > 1.0)
    value = 1.0;
  if (value_ == value)
    return;
  float old_value = value_;
  value_ = value;
  if (listener_)
    listener_->SliderValueChanged(this, value_, old_value, reason);

  if (old_value_valid && base::MessageLoopCurrent::Get()) {
    // Do not animate when setting the value of the slider for the first time.
    // There is no message-loop when running tests. So we cannot animate then.
    if (!move_animation_) {
      initial_animating_value_ = old_value;
      move_animation_ = std::make_unique<gfx::SlideAnimation>(this);
      move_animation_->SetSlideDuration(base::TimeDelta::FromMilliseconds(150));
      move_animation_->Show();
    }
    OnPropertyChanged(&value_, kPropertyEffectsNone);
  } else {
    OnPropertyChanged(&value_, kPropertyEffectsPaint);
  }

  if (accessibility_events_enabled_) {
    if (GetWidget() && GetWidget()->IsVisible()) {
      DCHECK(!pending_accessibility_value_change_);
      NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
    } else {
      pending_accessibility_value_change_ = true;
    }
  }
}

void Slider::PrepareForMove(const int new_x) {
  // Try to remember the position of the mouse cursor on the button.
  gfx::Insets inset = GetInsets();
  gfx::Rect content = GetContentsBounds();
  float value = GetAnimatingValue();

  const int thumb_x = value * (content.width() - kThumbWidth);
  const int candidate_x = (base::i18n::IsRTL() ?
      width() - (new_x - inset.left()) :
      new_x - inset.left()) - thumb_x;
  if (candidate_x >= 0 && candidate_x < kThumbWidth)
    initial_button_offset_ = candidate_x;
  else
    initial_button_offset_ = kThumbRadius;
}

void Slider::MoveButtonTo(const gfx::Point& point) {
  const gfx::Insets inset = GetInsets();
  // Calculate the value.
  int amount = base::i18n::IsRTL()
                   ? width() - inset.left() - point.x() - initial_button_offset_
                   : point.x() - inset.left() - initial_button_offset_;
  SetValueInternal(
      static_cast<float>(amount) / (width() - inset.width() - kThumbWidth),
      VALUE_CHANGED_BY_USER);
}

void Slider::OnSliderDragStarted() {
  SetHighlighted(true);
  if (listener_)
    listener_->SliderDragStarted(this);
}

void Slider::OnSliderDragEnded() {
  SetHighlighted(false);
  if (listener_)
    listener_->SliderDragEnded(this);
}

gfx::Size Slider::CalculatePreferredSize() const {
  constexpr int kSizeMajor = 200;
  constexpr int kSizeMinor = 40;

  return gfx::Size(std::max(width(), kSizeMajor), kSizeMinor);
}

bool Slider::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;
  OnSliderDragStarted();
  PrepareForMove(event.location().x());
  MoveButtonTo(event.location());
  return true;
}

bool Slider::OnMouseDragged(const ui::MouseEvent& event) {
  MoveButtonTo(event.location());
  return true;
}

void Slider::OnMouseReleased(const ui::MouseEvent& event) {
  OnSliderDragEnded();
}

bool Slider::OnKeyPressed(const ui::KeyEvent& event) {
  int direction = 1;
  switch (event.key_code()) {
    case ui::VKEY_LEFT:
      direction = base::i18n::IsRTL() ? 1 : -1;
      break;
    case ui::VKEY_RIGHT:
      direction = base::i18n::IsRTL() ? -1 : 1;
      break;
    case ui::VKEY_UP:
      direction = 1;
      break;
    case ui::VKEY_DOWN:
      direction = -1;
      break;

    default:
      return false;
  }
  SetValueInternal(value_ + direction * keyboard_increment_,
                   VALUE_CHANGED_BY_USER);
  return true;
}

void Slider::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kSlider;
  node_data->SetValue(base::UTF8ToUTF16(
      base::StringPrintf("%d%%", static_cast<int>(value_ * 100 + 0.5))));
}

void Slider::OnPaint(gfx::Canvas* canvas) {
  // Paint the slider.
  const gfx::Rect content = GetContentsBounds();
  const int width = content.width() - kThumbRadius * 2;
  const int full = GetAnimatingValue() * width;
  const int empty = width - full;
  const int y = content.height() / 2 - kLineThickness / 2;
  const int x = content.x() + full + kThumbRadius;
  const SkColor current_thumb_color =
      is_active_ ? kActiveColor : kDisabledColor;
  const uint8_t current_color_alpha =
      is_active_ ? kActiveColorAlpha : kDisabledColorAlpha;
  const SkColor empty_slider_color =
      SkColorSetA(current_thumb_color, current_color_alpha);

  // Padding used to adjust space between slider ends and slider thumb.
  // Value is negative when slider is active so that there is no separation
  // between slider and thumb.
  const int extra_padding = is_active_ ? -kSliderPadding : kSliderPadding;

  cc::PaintFlags slider_flags;
  slider_flags.setAntiAlias(true);
  slider_flags.setColor(current_thumb_color);
  canvas->DrawRoundRect(
      gfx::Rect(content.x(), y, full - extra_padding, kLineThickness),
      kSliderRoundedRadius, slider_flags);
  slider_flags.setColor(empty_slider_color);
  canvas->DrawRoundRect(gfx::Rect(x + kThumbRadius + extra_padding, y,
                                  empty - extra_padding, kLineThickness),
                        kSliderRoundedRadius, slider_flags);

  gfx::Point thumb_center(x, content.height() / 2);

  // Paint the thumb highlight if it exists.
  const int thumb_highlight_radius =
      HasFocus() ? kThumbHighlightRadius : thumb_highlight_radius_;
  if (thumb_highlight_radius > kThumbRadius) {
    cc::PaintFlags highlight;
    SkColor highlight_color =
        SkColorSetA(current_thumb_color, current_color_alpha);
    highlight.setColor(highlight_color);
    highlight.setAntiAlias(true);
    canvas->DrawCircle(thumb_center, thumb_highlight_radius, highlight);
  }

  // Paint the thumb of the slider.
  cc::PaintFlags flags;
  flags.setColor(current_thumb_color);
  flags.setAntiAlias(true);

  canvas->DrawCircle(thumb_center, kThumbRadius, flags);
}

void Slider::OnFocus() {
  View::OnFocus();
  SchedulePaint();
}

void Slider::OnBlur() {
  View::OnBlur();
  SchedulePaint();
}

void Slider::VisibilityChanged(View* starting_from, bool is_visible) {
  if (is_visible)
    NotifyPendingAccessibilityValueChanged();
}

void Slider::AddedToWidget() {
  if (GetWidget()->IsVisible())
    NotifyPendingAccessibilityValueChanged();
}

void Slider::NotifyPendingAccessibilityValueChanged() {
  if (!pending_accessibility_value_change_)
    return;

  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  pending_accessibility_value_change_ = false;
}

void Slider::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    // In a multi point gesture only the touch point will generate
    // an ET_GESTURE_TAP_DOWN event.
    case ui::ET_GESTURE_TAP_DOWN:
      OnSliderDragStarted();
      PrepareForMove(event->location().x());
      FALLTHROUGH;
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      MoveButtonTo(event->location());
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      MoveButtonTo(event->location());
      event->SetHandled();
      if (event->details().touch_points() <= 1)
        OnSliderDragEnded();
      break;
    default:
      break;
  }
}

BEGIN_METADATA(Slider)
METADATA_PARENT_CLASS(View)
ADD_PROPERTY_METADATA(Slider, float, Value)
ADD_PROPERTY_METADATA(Slider, bool, EnableAccessibilityEvents)
ADD_PROPERTY_METADATA(Slider, bool, IsActive)
END_METADATA()

}  // namespace views
