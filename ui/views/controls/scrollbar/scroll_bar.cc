// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/scroll_bar.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/cxx17_backports.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/widget/widget.h"

namespace views {

ScrollBar::~ScrollBar() = default;

bool ScrollBar::IsHorizontal() const {
  return is_horiz_;
}

void ScrollBar::SetThumb(BaseScrollBarThumb* thumb) {
  DCHECK(!thumb_);
  thumb_ = thumb;
  AddChildView(thumb);
  thumb->set_context_menu_controller(this);
}

bool ScrollBar::ScrollByAmount(ScrollAmount amount) {
  auto desired_offset = GetDesiredScrollOffset(amount);
  if (!desired_offset)
    return false;

  SetContentsScrollOffset(desired_offset.value());
  ScrollContentsToOffset();
  return true;
}

void ScrollBar::ScrollToThumbPosition(int thumb_position,
                                      bool scroll_to_middle) {
  SetContentsScrollOffset(CalculateContentsOffset(
      static_cast<float>(thumb_position), scroll_to_middle));
  ScrollContentsToOffset();
  SchedulePaint();
}

bool ScrollBar::ScrollByContentsOffset(int contents_offset) {
  int old_offset = contents_scroll_offset_;
  SetContentsScrollOffset(contents_scroll_offset_ - contents_offset);
  if (old_offset == contents_scroll_offset_)
    return false;

  ScrollContentsToOffset();
  return true;
}

int ScrollBar::GetMaxPosition() const {
  return max_pos_;
}

int ScrollBar::GetMinPosition() const {
  return 0;
}

int ScrollBar::GetPosition() const {
  return thumb_->GetPosition();
}

///////////////////////////////////////////////////////////////////////////////
// ScrollBar, View implementation:

void ScrollBar::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kScrollBar;
}

bool ScrollBar::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton())
    ProcessPressEvent(event);
  return true;
}

void ScrollBar::OnMouseReleased(const ui::MouseEvent& event) {
  repeater_.Stop();
}

void ScrollBar::OnMouseCaptureLost() {
  repeater_.Stop();
}

bool ScrollBar::OnKeyPressed(const ui::KeyEvent& event) {
  return ScrollByAmount(DetermineScrollAmountByKeyCode(event.key_code()));
}

bool ScrollBar::OnMouseWheel(const ui::MouseWheelEvent& event) {
  OnScroll(event.x_offset(), event.y_offset());
  return true;
}

void ScrollBar::OnGestureEvent(ui::GestureEvent* event) {
  // If a fling is in progress, then stop the fling for any incoming gesture
  // event (except for the GESTURE_END event that is generated at the end of the
  // fling).
  if (scroll_animator_ && scroll_animator_->is_scrolling() &&
      (event->type() != ui::ET_GESTURE_END ||
       event->details().touch_points() > 1)) {
    scroll_animator_->Stop();
  }

  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    ProcessPressEvent(*event);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    // For a long-press, the repeater started in tap-down should continue. So
    // return early.
    return;
  }

  repeater_.Stop();

  if (event->type() == ui::ET_GESTURE_TAP) {
    // TAP_DOWN would have already scrolled some amount. So scrolling again on
    // TAP is not necessary.
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    scroll_status_ = ScrollStatus::kScrollStarted;
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_END) {
    scroll_status_ = ScrollStatus::kScrollEnded;
    controller()->OnScrollEnded();
    event->SetHandled();
    return;
  }

  // Update the |scroll_status_| to |kScrollEnded| in case the gesture sequence
  // ends incorrectly.
  if (event->type() == ui::ET_GESTURE_END &&
      scroll_status_ != ScrollStatus::kScrollInEnding &&
      scroll_status_ != ScrollStatus::kScrollEnded) {
    scroll_status_ = ScrollStatus::kScrollEnded;
    controller()->OnScrollEnded();
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    if (scroll_status_ == ScrollStatus::kScrollStarted)
      scroll_status_ = ScrollStatus::kScrollInProgress;

    float scroll_amount_f;
    int scroll_amount;
    if (IsHorizontal()) {
      scroll_amount_f = event->details().scroll_x() - roundoff_error_.x();
      scroll_amount = base::ClampRound(scroll_amount_f);
      roundoff_error_.set_x(scroll_amount - scroll_amount_f);
    } else {
      scroll_amount_f = event->details().scroll_y() - roundoff_error_.y();
      scroll_amount = base::ClampRound(scroll_amount_f);
      roundoff_error_.set_y(scroll_amount - scroll_amount_f);
    }
    if (ScrollByContentsOffset(scroll_amount))
      event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_SCROLL_FLING_START) {
    scroll_status_ = ScrollStatus::kScrollInEnding;
    GetOrCreateScrollAnimator()->Start(
        IsHorizontal() ? event->details().velocity_x() : 0.f,
        IsHorizontal() ? 0.f : event->details().velocity_y());
    event->SetHandled();
  }
}

///////////////////////////////////////////////////////////////////////////////
// ScrollBar, ScrollDelegate implementation:

bool ScrollBar::OnScroll(float dx, float dy) {
  return IsHorizontal() ? ScrollByContentsOffset(dx)
                        : ScrollByContentsOffset(dy);
}

void ScrollBar::OnFlingScrollEnded() {
  scroll_status_ = ScrollStatus::kScrollEnded;
  controller()->OnScrollEnded();
}

///////////////////////////////////////////////////////////////////////////////
// ScrollBar, ContextMenuController implementation:

enum ScrollBarContextMenuCommands {
  ScrollBarContextMenuCommand_ScrollHere = 1,
  ScrollBarContextMenuCommand_ScrollStart,
  ScrollBarContextMenuCommand_ScrollEnd,
  ScrollBarContextMenuCommand_ScrollPageUp,
  ScrollBarContextMenuCommand_ScrollPageDown,
  ScrollBarContextMenuCommand_ScrollPrev,
  ScrollBarContextMenuCommand_ScrollNext
};

void ScrollBar::ShowContextMenuForViewImpl(View* source,
                                           const gfx::Point& p,
                                           ui::MenuSourceType source_type) {
  Widget* widget = GetWidget();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  gfx::Point temp_pt(p.x() - widget_bounds.x(), p.y() - widget_bounds.y());
  View::ConvertPointFromWidget(this, &temp_pt);
  context_menu_mouse_position_ = IsHorizontal() ? temp_pt.x() : temp_pt.y();

  if (!menu_model_) {
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    menu_model_->AddItemWithStringId(ScrollBarContextMenuCommand_ScrollHere,
                                     IDS_APP_SCROLLBAR_CXMENU_SCROLLHERE);
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItemWithStringId(
        ScrollBarContextMenuCommand_ScrollStart,
        IsHorizontal() ? IDS_APP_SCROLLBAR_CXMENU_SCROLLLEFTEDGE
                       : IDS_APP_SCROLLBAR_CXMENU_SCROLLHOME);
    menu_model_->AddItemWithStringId(
        ScrollBarContextMenuCommand_ScrollEnd,
        IsHorizontal() ? IDS_APP_SCROLLBAR_CXMENU_SCROLLRIGHTEDGE
                       : IDS_APP_SCROLLBAR_CXMENU_SCROLLEND);
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItemWithStringId(ScrollBarContextMenuCommand_ScrollPageUp,
                                     IDS_APP_SCROLLBAR_CXMENU_SCROLLPAGEUP);
    menu_model_->AddItemWithStringId(ScrollBarContextMenuCommand_ScrollPageDown,
                                     IDS_APP_SCROLLBAR_CXMENU_SCROLLPAGEDOWN);
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItemWithStringId(ScrollBarContextMenuCommand_ScrollPrev,
                                     IsHorizontal()
                                         ? IDS_APP_SCROLLBAR_CXMENU_SCROLLLEFT
                                         : IDS_APP_SCROLLBAR_CXMENU_SCROLLUP);
    menu_model_->AddItemWithStringId(ScrollBarContextMenuCommand_ScrollNext,
                                     IsHorizontal()
                                         ? IDS_APP_SCROLLBAR_CXMENU_SCROLLRIGHT
                                         : IDS_APP_SCROLLBAR_CXMENU_SCROLLDOWN);
  }
  menu_runner_ = std::make_unique<MenuRunner>(
      menu_model_.get(),
      MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(p, gfx::Size()),
                          MenuAnchorPosition::kTopLeft, source_type);
}

///////////////////////////////////////////////////////////////////////////////
// ScrollBar, Menu::Delegate implementation:

bool ScrollBar::IsCommandIdEnabled(int id) const {
  switch (id) {
    case ScrollBarContextMenuCommand_ScrollPageUp:
    case ScrollBarContextMenuCommand_ScrollPageDown:
      return !IsHorizontal();
  }
  return true;
}

bool ScrollBar::IsCommandIdChecked(int id) const {
  return false;
}

void ScrollBar::ExecuteCommand(int id, int event_flags) {
  switch (id) {
    case ScrollBarContextMenuCommand_ScrollHere:
      ScrollToThumbPosition(context_menu_mouse_position_, true);
      break;
    case ScrollBarContextMenuCommand_ScrollStart:
      ScrollByAmount(ScrollAmount::kStart);
      break;
    case ScrollBarContextMenuCommand_ScrollEnd:
      ScrollByAmount(ScrollAmount::kEnd);
      break;
    case ScrollBarContextMenuCommand_ScrollPageUp:
      ScrollByAmount(ScrollAmount::kPrevPage);
      break;
    case ScrollBarContextMenuCommand_ScrollPageDown:
      ScrollByAmount(ScrollAmount::kNextPage);
      break;
    case ScrollBarContextMenuCommand_ScrollPrev:
      ScrollByAmount(ScrollAmount::kPrevLine);
      break;
    case ScrollBarContextMenuCommand_ScrollNext:
      ScrollByAmount(ScrollAmount::kNextLine);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// ScrollBar implementation:

bool ScrollBar::OverlapsContent() const {
  return false;
}

void ScrollBar::Update(int viewport_size,
                       int content_size,
                       int contents_scroll_offset) {
  max_pos_ = std::max(0, content_size - viewport_size);
  // Make sure contents_size is always > 0 to avoid divide by zero errors in
  // calculations throughout this code.
  contents_size_ = std::max(1, content_size);
  viewport_size_ = std::max(1, viewport_size);

  SetContentsScrollOffset(contents_scroll_offset);

  // Thumb Height and Thumb Pos.
  // The height of the thumb is the ratio of the Viewport height to the
  // content size multiplied by the height of the thumb track.
  float ratio =
      std::min<float>(1.0, static_cast<float>(viewport_size) / contents_size_);
  thumb_->SetLength(base::ClampRound(ratio * GetTrackSize()));

  int thumb_position = CalculateThumbPosition(contents_scroll_offset);
  thumb_->SetPosition(thumb_position);
}

///////////////////////////////////////////////////////////////////////////////
// ScrollBar, protected:

BaseScrollBarThumb* ScrollBar::GetThumb() const {
  return thumb_;
}

void ScrollBar::ScrollToPosition(int position) {
  controller()->ScrollToPosition(this, position);
}

int ScrollBar::GetScrollIncrement(bool is_page, bool is_positive) {
  return controller()->GetScrollIncrement(this, is_page, is_positive);
}

void ScrollBar::ObserveScrollEvent(const ui::ScrollEvent& event) {
  switch (event.type()) {
    case ui::ET_SCROLL_FLING_CANCEL:
      scroll_status_ = ScrollStatus::kScrollStarted;
      break;
    case ui::ET_SCROLL:
      if (scroll_status_ == ScrollStatus::kScrollStarted)
        scroll_status_ = ScrollStatus::kScrollInProgress;
      break;
    case ui::ET_SCROLL_FLING_START:
      scroll_status_ = ScrollStatus::kScrollEnded;
      controller()->OnScrollEnded();
      break;
    case ui::ET_GESTURE_END:
      if (scroll_status_ != ScrollStatus::kScrollEnded) {
        scroll_status_ = ScrollStatus::kScrollEnded;
        controller()->OnScrollEnded();
      }
      break;
    default:
      break;
  }
}

ScrollAnimator* ScrollBar::GetOrCreateScrollAnimator() {
  if (!scroll_animator_) {
    scroll_animator_ = std::make_unique<ScrollAnimator>(this);
    scroll_animator_->set_velocity_multiplier(fling_multiplier_);
  }
  return scroll_animator_.get();
}

void ScrollBar::SetFlingMultiplier(float fling_multiplier) {
  fling_multiplier_ = fling_multiplier;
  // `scroll_animator_` is lazily created when needed.
  if (!scroll_animator_)
    return;

  GetOrCreateScrollAnimator()->set_velocity_multiplier(fling_multiplier_);
}

ScrollBar::ScrollBar(bool is_horiz)
    : is_horiz_(is_horiz),
      repeater_(base::BindRepeating(&ScrollBar::TrackClicked,
                                    base::Unretained(this))) {
  set_context_menu_controller(this);
}

///////////////////////////////////////////////////////////////////////////////
// ScrollBar, private:

#if !BUILDFLAG(IS_MAC)
// static
base::RetainingOneShotTimer* ScrollBar::GetHideTimerForTesting(
    ScrollBar* scroll_bar) {
  return nullptr;
}
#endif

int ScrollBar::GetThumbLengthForTesting() {
  return thumb_->GetLength();
}

void ScrollBar::ProcessPressEvent(const ui::LocatedEvent& event) {
  gfx::Rect thumb_bounds = thumb_->bounds();
  if (IsHorizontal()) {
    if (GetMirroredXInView(event.x()) < thumb_bounds.x()) {
      last_scroll_amount_ = ScrollAmount::kPrevPage;
    } else if (GetMirroredXInView(event.x()) > thumb_bounds.right()) {
      last_scroll_amount_ = ScrollAmount::kNextPage;
    }
  } else {
    if (event.y() < thumb_bounds.y()) {
      last_scroll_amount_ = ScrollAmount::kPrevPage;
    } else if (event.y() > thumb_bounds.bottom()) {
      last_scroll_amount_ = ScrollAmount::kNextPage;
    }
  }
  TrackClicked();
  repeater_.Start();
}

void ScrollBar::TrackClicked() {
  ScrollByAmount(last_scroll_amount_);
}

void ScrollBar::ScrollContentsToOffset() {
  ScrollToPosition(contents_scroll_offset_);
  thumb_->SetPosition(CalculateThumbPosition(contents_scroll_offset_));
}

int ScrollBar::GetTrackSize() const {
  gfx::Rect track_bounds = GetTrackBounds();
  return IsHorizontal() ? track_bounds.width() : track_bounds.height();
}

int ScrollBar::CalculateThumbPosition(int contents_scroll_offset) const {
  // In some combination of viewport_size and contents_size_, the result of
  // simple division can be rounded and there could be 1 pixel gap even when the
  // contents scroll down to the bottom. See crbug.com/244671.
  int thumb_max = GetTrackSize() - thumb_->GetLength();
  if (contents_scroll_offset + viewport_size_ == contents_size_)
    return thumb_max;
  return (contents_scroll_offset * thumb_max) /
         (contents_size_ - viewport_size_);
}

int ScrollBar::CalculateContentsOffset(float thumb_position,
                                       bool scroll_to_middle) const {
  float thumb_size = static_cast<float>(thumb_->GetLength());
  int track_size = GetTrackSize();
  if (track_size == thumb_size)
    return 0;
  if (scroll_to_middle)
    thumb_position = thumb_position - (thumb_size / 2);
  float result = (thumb_position * (contents_size_ - viewport_size_)) /
                 (track_size - thumb_size);
  return base::ClampRound(result);
}

void ScrollBar::SetContentsScrollOffset(int contents_scroll_offset) {
  contents_scroll_offset_ =
      base::clamp(contents_scroll_offset, GetMinPosition(), GetMaxPosition());
}

ScrollBar::ScrollAmount ScrollBar::DetermineScrollAmountByKeyCode(
    const ui::KeyboardCode& keycode) const {
  // Reject arrows that don't match the scrollbar orientation.
  if (IsHorizontal() ? (keycode == ui::VKEY_UP || keycode == ui::VKEY_DOWN)
                     : (keycode == ui::VKEY_LEFT || keycode == ui::VKEY_RIGHT))
    return ScrollAmount::kNone;

  static const base::NoDestructor<
      base::flat_map<ui::KeyboardCode, ScrollAmount>>
      kMap({
          {ui::VKEY_LEFT, ScrollAmount::kPrevLine},
          {ui::VKEY_RIGHT, ScrollAmount::kNextLine},
          {ui::VKEY_UP, ScrollAmount::kPrevLine},
          {ui::VKEY_DOWN, ScrollAmount::kNextLine},
          {ui::VKEY_PRIOR, ScrollAmount::kPrevPage},
          {ui::VKEY_NEXT, ScrollAmount::kNextPage},
          {ui::VKEY_HOME, ScrollAmount::kStart},
          {ui::VKEY_END, ScrollAmount::kEnd},
      });

  const auto i = kMap->find(keycode);
  return (i == kMap->end()) ? ScrollAmount::kNone : i->second;
}

absl::optional<int> ScrollBar::GetDesiredScrollOffset(ScrollAmount amount) {
  switch (amount) {
    case ScrollAmount::kStart:
      return GetMinPosition();
    case ScrollAmount::kEnd:
      return GetMaxPosition();
    case ScrollAmount::kPrevLine:
      return contents_scroll_offset_ - GetScrollIncrement(false, false);
    case ScrollAmount::kNextLine:
      return contents_scroll_offset_ + GetScrollIncrement(false, true);
    case ScrollAmount::kPrevPage:
      return contents_scroll_offset_ - GetScrollIncrement(true, false);
    case ScrollAmount::kNextPage:
      return contents_scroll_offset_ + GetScrollIncrement(true, true);
    default:
      return absl::nullopt;
  }
}

BEGIN_METADATA(ScrollBar, View)
ADD_READONLY_PROPERTY_METADATA(int, MaxPosition)
ADD_READONLY_PROPERTY_METADATA(int, MinPosition)
ADD_READONLY_PROPERTY_METADATA(int, Position)
END_METADATA

}  // namespace views
