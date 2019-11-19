// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/scroll_bar_views.h"

#include "base/logging.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_button.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

// Wrapper for the scroll buttons.
class ScrollBarButton : public BaseScrollBarButton {
 public:
  enum class Type {
    kUp,
    kDown,
    kLeft,
    kRight,
  };

  ScrollBarButton(ButtonListener* listener, Type type);
  ~ScrollBarButton() override;

  gfx::Size CalculatePreferredSize() const override;

 protected:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  ui::NativeTheme::ExtraParams GetNativeThemeParams() const;
  ui::NativeTheme::Part GetNativeThemePart() const;
  ui::NativeTheme::State GetNativeThemeState() const;

  Type type_;
};

// Wrapper for the scroll thumb
class ScrollBarThumb : public BaseScrollBarThumb {
 public:
  explicit ScrollBarThumb(ScrollBar* scroll_bar);
  ~ScrollBarThumb() override;

  gfx::Size CalculatePreferredSize() const override;

 protected:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  ui::NativeTheme::ExtraParams GetNativeThemeParams() const;
  ui::NativeTheme::Part GetNativeThemePart() const;
  ui::NativeTheme::State GetNativeThemeState() const;

  ScrollBar* scroll_bar_;
};

/////////////////////////////////////////////////////////////////////////////
// ScrollBarButton

ScrollBarButton::ScrollBarButton(ButtonListener* listener, Type type)
    : BaseScrollBarButton(listener), type_(type) {
  EnableCanvasFlippingForRTLUI(true);
  SetFocusBehavior(FocusBehavior::NEVER);
}

ScrollBarButton::~ScrollBarButton() = default;

gfx::Size ScrollBarButton::CalculatePreferredSize() const {
  return GetNativeTheme()->GetPartSize(
      GetNativeThemePart(), GetNativeThemeState(), GetNativeThemeParams());
}

void ScrollBarButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect bounds(GetPreferredSize());
  GetNativeTheme()->Paint(canvas->sk_canvas(), GetNativeThemePart(),
                          GetNativeThemeState(), bounds,
                          GetNativeThemeParams());
}

ui::NativeTheme::ExtraParams ScrollBarButton::GetNativeThemeParams() const {
  ui::NativeTheme::ExtraParams params;

  switch (state()) {
    case Button::STATE_HOVERED:
      params.scrollbar_arrow.is_hovering = true;
      break;
    default:
      params.scrollbar_arrow.is_hovering = false;
      break;
  }

  return params;
}

ui::NativeTheme::Part ScrollBarButton::GetNativeThemePart() const {
  switch (type_) {
    case Type::kUp:
      return ui::NativeTheme::kScrollbarUpArrow;
    case Type::kDown:
      return ui::NativeTheme::kScrollbarDownArrow;
    case Type::kLeft:
      return ui::NativeTheme::kScrollbarLeftArrow;
    case Type::kRight:
      return ui::NativeTheme::kScrollbarRightArrow;
  }

  NOTREACHED();
  return ui::NativeTheme::kScrollbarUpArrow;
}

ui::NativeTheme::State ScrollBarButton::GetNativeThemeState() const {
  switch (state()) {
    case Button::STATE_HOVERED:
      return ui::NativeTheme::kHovered;
    case Button::STATE_PRESSED:
      return ui::NativeTheme::kPressed;
    case Button::STATE_DISABLED:
      return ui::NativeTheme::kDisabled;
    case Button::STATE_NORMAL:
      return ui::NativeTheme::kNormal;
    case Button::STATE_COUNT:
      break;
  }

  NOTREACHED();
  return ui::NativeTheme::kNormal;
}

/////////////////////////////////////////////////////////////////////////////
// ScrollBarThumb

ScrollBarThumb::ScrollBarThumb(ScrollBar* scroll_bar)
    : BaseScrollBarThumb(scroll_bar), scroll_bar_(scroll_bar) {}

ScrollBarThumb::~ScrollBarThumb() = default;

gfx::Size ScrollBarThumb::CalculatePreferredSize() const {
  return GetNativeTheme()->GetPartSize(
      GetNativeThemePart(), GetNativeThemeState(), GetNativeThemeParams());
}

void ScrollBarThumb::OnPaint(gfx::Canvas* canvas) {
  const gfx::Rect local_bounds(GetLocalBounds());
  const ui::NativeTheme::State theme_state = GetNativeThemeState();
  const ui::NativeTheme::ExtraParams extra_params(GetNativeThemeParams());
  GetNativeTheme()->Paint(canvas->sk_canvas(), GetNativeThemePart(),
                          theme_state, local_bounds, extra_params);
  const ui::NativeTheme::Part gripper_part =
      scroll_bar_->IsHorizontal() ? ui::NativeTheme::kScrollbarHorizontalGripper
                                  : ui::NativeTheme::kScrollbarVerticalGripper;
  GetNativeTheme()->Paint(canvas->sk_canvas(), gripper_part, theme_state,
                          local_bounds, extra_params);
}

ui::NativeTheme::ExtraParams ScrollBarThumb::GetNativeThemeParams() const {
  // This gives the behavior we want.
  ui::NativeTheme::ExtraParams params;
  params.scrollbar_thumb.is_hovering = (GetState() != Button::STATE_HOVERED);
  return params;
}

ui::NativeTheme::Part ScrollBarThumb::GetNativeThemePart() const {
  if (scroll_bar_->IsHorizontal())
    return ui::NativeTheme::kScrollbarHorizontalThumb;
  return ui::NativeTheme::kScrollbarVerticalThumb;
}

ui::NativeTheme::State ScrollBarThumb::GetNativeThemeState() const {
  switch (GetState()) {
    case Button::STATE_HOVERED:
      return ui::NativeTheme::kHovered;
    case Button::STATE_PRESSED:
      return ui::NativeTheme::kPressed;
    case Button::STATE_DISABLED:
      return ui::NativeTheme::kDisabled;
    case Button::STATE_NORMAL:
      return ui::NativeTheme::kNormal;
    case Button::STATE_COUNT:
      break;
  }

  NOTREACHED();
  return ui::NativeTheme::kNormal;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ScrollBarViews, public:

ScrollBarViews::ScrollBarViews(bool horizontal) : ScrollBar(horizontal) {
  EnableCanvasFlippingForRTLUI(true);
  state_ = ui::NativeTheme::kNormal;

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());

  std::unique_ptr<ScrollBarButton> prev_button, next_button;
  using Type = ScrollBarButton::Type;
  if (horizontal) {
    prev_button = std::make_unique<ScrollBarButton>(this, Type::kLeft);
    next_button = std::make_unique<ScrollBarButton>(this, Type::kRight);

    part_ = ui::NativeTheme::kScrollbarHorizontalTrack;
  } else {
    layout->SetOrientation(views::LayoutOrientation::kVertical);

    prev_button = std::make_unique<ScrollBarButton>(this, Type::kUp);
    next_button = std::make_unique<ScrollBarButton>(this, Type::kDown);

    part_ = ui::NativeTheme::kScrollbarVerticalTrack;
  }
  prev_button->set_context_menu_controller(this);
  next_button->set_context_menu_controller(this);

  prev_button_ = AddChildView(std::move(prev_button));
  SetThumb(new ScrollBarThumb(this));
  // Allow the thumb to take up the whole size of the scrollbar, save for the
  // prev/next buttons.  Layout need only set the thumb cross-axis coordinate;
  // ScrollBar::Update() will set the thumb size/offset.
  GetThumb()->SetProperty(views::kFlexBehaviorKey,
                          views::FlexSpecification::ForSizeRule(
                              views::MinimumFlexSizeRule::kPreferred,
                              views::MaximumFlexSizeRule::kUnbounded));
  next_button_ = AddChildView(std::move(next_button));
}

ScrollBarViews::~ScrollBarViews() = default;

// static
int ScrollBarViews::GetVerticalScrollBarWidth(const ui::NativeTheme* theme) {
  ui::NativeTheme::ExtraParams button_params;
  button_params.scrollbar_arrow.is_hovering = false;
  gfx::Size button_size =
      theme->GetPartSize(ui::NativeTheme::kScrollbarUpArrow,
                         ui::NativeTheme::kNormal, button_params);

  ui::NativeTheme::ExtraParams thumb_params;
  thumb_params.scrollbar_thumb.is_hovering = false;
  gfx::Size track_size =
      theme->GetPartSize(ui::NativeTheme::kScrollbarVerticalThumb,
                         ui::NativeTheme::kNormal, thumb_params);

  return std::max(track_size.width(), button_size.width());
}

////////////////////////////////////////////////////////////////////////////////
// ScrollBarViews, View overrides:

void ScrollBarViews::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetTrackBounds();
  if (bounds.IsEmpty())
    return;

  params_.scrollbar_track.track_x = bounds.x();
  params_.scrollbar_track.track_y = bounds.y();
  params_.scrollbar_track.track_width = bounds.width();
  params_.scrollbar_track.track_height = bounds.height();
  params_.scrollbar_track.classic_state = 0;
  const BaseScrollBarThumb* thumb = GetThumb();

  params_.scrollbar_track.is_upper = true;
  gfx::Rect upper_bounds = bounds;
  if (IsHorizontal())
    upper_bounds.set_width(thumb->x() - upper_bounds.x());
  else
    upper_bounds.set_height(thumb->y() - upper_bounds.y());
  if (!upper_bounds.IsEmpty()) {
    GetNativeTheme()->Paint(canvas->sk_canvas(), part_, state_, upper_bounds,
                            params_);
  }

  params_.scrollbar_track.is_upper = false;
  if (IsHorizontal())
    bounds.Inset(thumb->bounds().right() - bounds.x(), 0, 0, 0);
  else
    bounds.Inset(0, thumb->bounds().bottom() - bounds.y(), 0, 0);
  if (!bounds.IsEmpty()) {
    GetNativeTheme()->Paint(canvas->sk_canvas(), part_, state_, bounds,
                            params_);
  }
}

int ScrollBarViews::GetThickness() const {
  const gfx::Size size = GetPreferredSize();
  return IsHorizontal() ? size.height() : size.width();
}

//////////////////////////////////////////////////////////////////////////////
// BaseButton::ButtonListener overrides:

void ScrollBarViews::ButtonPressed(Button* sender, const ui::Event& event) {
  const bool is_prev = sender == prev_button_;
  DCHECK(is_prev || sender == next_button_);
  ScrollByAmount(is_prev ? ScrollBar::ScrollAmount::kPrevLine
                         : ScrollBar::ScrollAmount::kNextLine);
}

////////////////////////////////////////////////////////////////////////////////
// ScrollBarViews, private:

gfx::Rect ScrollBarViews::GetTrackBounds() const {
  gfx::Rect bounds = GetLocalBounds();
  gfx::Size size = prev_button_->GetPreferredSize();
  BaseScrollBarThumb* thumb = GetThumb();

  if (IsHorizontal()) {
    bounds.set_x(bounds.x() + size.width());
    bounds.set_width(std::max(0, bounds.width() - 2 * size.width()));
    bounds.set_height(thumb->GetPreferredSize().height());
  } else {
    bounds.set_y(bounds.y() + size.height());
    bounds.set_height(std::max(0, bounds.height() - 2 * size.height()));
    bounds.set_width(thumb->GetPreferredSize().width());
  }

  return bounds;
}

BEGIN_METADATA(ScrollBarViews)
METADATA_PARENT_CLASS(ScrollBar)
END_METADATA()

}  // namespace views
