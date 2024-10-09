// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// The thickness of the slider.
constexpr int kLineThickness = 2;

// The radius used to draw rounded slider ends.
constexpr float kSliderRoundedRadius = 2.f;

// The padding used to hide the slider underneath the thumb.
constexpr int kSliderPadding = 2;

// The radius of the highlighted thumb of the slider
constexpr float kThumbHighlightRadius = 12.f;

float GetNearestAllowedValue(const base::flat_set<float>& allowed_values,
                             float suggested_value) {
  if (allowed_values.empty())
    return suggested_value;

  const base::flat_set<float>::const_iterator greater =
      allowed_values.upper_bound(suggested_value);
  if (greater == allowed_values.end())
    return *allowed_values.rbegin();

  if (greater == allowed_values.begin())
    return *allowed_values.cbegin();

  // Select a value nearest to the |suggested_value|.
  if ((*greater - suggested_value) > (suggested_value - *std::prev(greater)))
    return *std::prev(greater);

  return *greater;
}

}  // namespace

Slider::Slider(SliderListener* listener) : listener_(listener) {
  highlight_animation_.SetSlideDuration(base::Milliseconds(150));
  SetFlipCanvasOnPaintForRTLUI(true);

#if BUILDFLAG(IS_MAC)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif

  SchedulePaint();
  GetViewAccessibility().SetRole(ax::mojom::Role::kSlider);
  GetViewAccessibility().AddAction(ax::mojom::Action::kIncrement);
  GetViewAccessibility().AddAction(ax::mojom::Action::kDecrement);
}

Slider::~Slider() = default;

float Slider::GetValue() const {
  return value_;
}

void Slider::SetValue(float value) {
  SetValueInternal(value, SliderChangeReason::kByApi);
}

float Slider::GetValueIndicatorRadius() const {
  return value_indicator_radius_;
}

void Slider::SetValueIndicatorRadius(float radius) {
  value_indicator_radius_ = radius;
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

void Slider::SetRenderingStyle(RenderingStyle style) {
  style_ = style;
  SchedulePaint();
}

void Slider::SetAllowedValues(const base::flat_set<float>* allowed_values) {
  if (!allowed_values) {
    allowed_values_.clear();
    return;
  }
#if DCHECK_IS_ON()
  // Disallow empty sliders.
  DCHECK(allowed_values->size());
  for (const float v : *allowed_values) {
    // sanity check.
    DCHECK_GE(v, 0.0f);
    DCHECK_LE(v, 1.0f);
  }
#endif
  allowed_values_ = *allowed_values;

  const auto position = allowed_values_.lower_bound(value_);
  const float new_value = (position == allowed_values_.end())
                              ? *allowed_values_.cbegin()
                              : *position;
  if (new_value != value_)
    SetValue(new_value);
}

float Slider::GetAnimatingValue() const {
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
  value = GetNearestAllowedValue(allowed_values_, value);
  if (value_ == value)
    return;
  float old_value = value_;
  value_ = value;
  if (listener_)
    listener_->SliderValueChanged(this, value_, old_value, reason);

  if (old_value_valid && base::CurrentThread::Get()) {
    // Do not animate when setting the value of the slider for the first time.
    // There is no message-loop when running tests. So we cannot animate then.
    if (!move_animation_) {
      initial_animating_value_ = old_value;
      move_animation_ = std::make_unique<gfx::SlideAnimation>(this);
      move_animation_->SetSlideDuration(base::Milliseconds(150));
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

  const int thumb_x = value * (content.width() - 2 * value_indicator_radius_);
  const int candidate_x = GetMirroredXInView(new_x - inset.left()) - thumb_x;
  if (candidate_x >= value_indicator_radius_ - kThumbRadius &&
      candidate_x < value_indicator_radius_ + kThumbRadius)
    initial_button_offset_ = candidate_x;
  else
    initial_button_offset_ = value_indicator_radius_;
}

void Slider::MoveButtonTo(const gfx::Point& point) {
  const gfx::Insets inset = GetInsets();
  // Calculate the value.
  int amount = base::i18n::IsRTL()
                   ? width() - inset.left() - point.x() - initial_button_offset_
                   : point.x() - inset.left() - initial_button_offset_;
  SetValueInternal(static_cast<float>(amount) /
                       (width() - inset.width() - 2 * value_indicator_radius_),
                   SliderChangeReason::kByUser);
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

gfx::Size Slider::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  constexpr int kSizeMajor = 200;
  constexpr int kSizeMinor = 40;

  return gfx::Size(std::max(available_size.width().value_or(0), kSizeMajor),
                   kSizeMinor);
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
  if (allowed_values_.empty()) {
    SetValueInternal(value_ + direction * keyboard_increment_,
                     SliderChangeReason::kByUser);
  } else {
    // discrete slider.
    if (direction > 0) {
      const base::flat_set<float>::const_iterator greater =
          allowed_values_.upper_bound(value_);
      SetValueInternal(greater == allowed_values_.cend()
                           ? *allowed_values_.crend()
                           : *greater,
                       SliderChangeReason::kByUser);
    } else {
      const base::flat_set<float>::const_iterator lesser =
          allowed_values_.lower_bound(value_);
      // Current value must be in the list of allowed values.
      DCHECK(lesser != allowed_values_.cend());
      SetValueInternal(lesser == allowed_values_.cbegin()
                           ? *allowed_values_.cbegin()
                           : *std::prev(lesser),
                       SliderChangeReason::kByUser);
    }
  }
  return true;
}

void Slider::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetValue(base::UTF8ToUTF16(
      base::StringPrintf("%d%%", static_cast<int>(value_ * 100 + 0.5))));
}

bool Slider::HandleAccessibleAction(const ui::AXActionData& action_data) {
  if (action_data.action == ax::mojom::Action::kIncrement) {
    SetValueInternal(value_ + keyboard_increment_, SliderChangeReason::kByUser);
    return true;
  } else if (action_data.action == ax::mojom::Action::kDecrement) {
    SetValueInternal(value_ - keyboard_increment_, SliderChangeReason::kByUser);
    return true;
  } else {
    return views::View::HandleAccessibleAction(action_data);
  }
}

void Slider::OnPaint(gfx::Canvas* canvas) {
  // Paint the slider.
  const gfx::Rect content = GetContentsBounds();
  const int width = content.width() - kThumbRadius * 2;
  const int full = GetAnimatingValue() * width;
  const int empty = width - full;
  const int y = content.height() / 2 - kLineThickness / 2;
  const int x = content.x() + full + kThumbRadius;

  cc::PaintFlags slider_flags;
  slider_flags.setAntiAlias(true);
  slider_flags.setColor(GetThumbColor());
  canvas->DrawRoundRect(
      gfx::Rect(content.x(), y, full - GetSliderExtraPadding(), kLineThickness),
      kSliderRoundedRadius, slider_flags);
  slider_flags.setColor(GetTroughColor());
  canvas->DrawRoundRect(
      gfx::Rect(x + kThumbRadius + GetSliderExtraPadding(), y,
                empty - GetSliderExtraPadding(), kLineThickness),
      kSliderRoundedRadius, slider_flags);

  gfx::Point thumb_center(x, content.height() / 2);

  // Paint the thumb highlight if it exists.
  const int thumb_highlight_radius =
      HasFocus() ? kThumbHighlightRadius : thumb_highlight_radius_;
  if (thumb_highlight_radius > kThumbRadius) {
    cc::PaintFlags highlight_background;
    highlight_background.setColor(GetTroughColor());
    highlight_background.setAntiAlias(true);
    canvas->DrawCircle(thumb_center, thumb_highlight_radius,
                       highlight_background);

    cc::PaintFlags highlight_border;
    highlight_border.setColor(GetThumbColor());
    highlight_border.setAntiAlias(true);
    highlight_border.setStyle(cc::PaintFlags::kStroke_Style);
    highlight_border.setStrokeWidth(kLineThickness);
    canvas->DrawCircle(thumb_center, thumb_highlight_radius, highlight_border);
  }

  // Paint the thumb of the slider.
  cc::PaintFlags flags;
  flags.setColor(GetThumbColor());
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
    // an EventType::kGestureTapDown event.
    case ui::EventType::kGestureTapDown:
      OnSliderDragStarted();
      PrepareForMove(event->location().x());
      [[fallthrough]];
    case ui::EventType::kGestureScrollBegin:
    case ui::EventType::kGestureScrollUpdate:
      MoveButtonTo(event->location());
      event->SetHandled();
      break;
    case ui::EventType::kGestureEnd:
      MoveButtonTo(event->location());
      event->SetHandled();
      if (event->details().touch_points() <= 1)
        OnSliderDragEnded();
      break;
    default:
      break;
  }
}

SkColor Slider::GetThumbColor() const {
  switch (style_) {
    case RenderingStyle::kDefaultStyle:
      return GetColorProvider()->GetColor(ui::kColorSliderThumb);
    case RenderingStyle::kMinimalStyle:
      return GetColorProvider()->GetColor(ui::kColorSliderThumbMinimal);
  }
}

SkColor Slider::GetTroughColor() const {
  switch (style_) {
    case RenderingStyle::kDefaultStyle:
      return GetColorProvider()->GetColor(ui::kColorSliderTrack);
    case RenderingStyle::kMinimalStyle:
      return GetColorProvider()->GetColor(ui::kColorSliderTrackMinimal);
  }
}

int Slider::GetSliderExtraPadding() const {
  // Padding is negative when slider style is default so that there is no
  // separation between slider and thumb.
  switch (style_) {
    case RenderingStyle::kDefaultStyle:
      return -kSliderPadding;
    case RenderingStyle::kMinimalStyle:
      return kSliderPadding;
  }
}

BEGIN_METADATA(Slider)
ADD_PROPERTY_METADATA(float, Value)
ADD_PROPERTY_METADATA(bool, EnableAccessibilityEvents)
ADD_PROPERTY_METADATA(float, ValueIndicatorRadius)
END_METADATA

}  // namespace views
