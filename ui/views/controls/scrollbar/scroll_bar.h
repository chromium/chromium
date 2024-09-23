// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_SCROLL_BAR_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_SCROLL_BAR_H_

#include <memory>
#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/animation/scroll_animator.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/repeat_controller.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

namespace test {
class ScrollViewTestApi;
}

class BaseScrollBarThumb;
class MenuRunner;

class ScrollBar;

/////////////////////////////////////////////////////////////////////////////
//
// ScrollBarController
//
// ScrollBarController defines the method that should be implemented to
// receive notification from a scrollbar
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT ScrollBarController {
 public:
  // Invoked by the scrollbar when the scrolling position changes
  // This method typically implements the actual scrolling.
  //
  // The provided position is expressed in pixels. It is the new X or Y
  // position which is in the GetMinPosition() / GetMaxPosition range.
  virtual void ScrollToPosition(ScrollBar* source, int position) = 0;

  // Called when the scroll that triggered by gesture or scroll events sequence
  // ended.
  virtual void OnScrollEnded() {}

  // Returns the amount to scroll. The amount to scroll may be requested in
  // two different amounts. If is_page is true the 'page scroll' amount is
  // requested. The page scroll amount typically corresponds to the
  // visual size of the view. If is_page is false, the 'line scroll' amount
  // is being requested. The line scroll amount typically corresponds to the
  // size of one row/column.
  //
  // The return value should always be positive. A value <= 0 results in
  // scrolling by a fixed amount.
  virtual int GetScrollIncrement(ScrollBar* source,
                                 bool is_page,
                                 bool is_positive) = 0;
};

/////////////////////////////////////////////////////////////////////////////
//
// ScrollBar
//
// A View subclass to wrap to implement a ScrollBar. Our current windows
// version simply wraps a native windows scrollbar.
//
// A scrollbar is either horizontal or vertical
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT ScrollBar : public View,
                               public ScrollDelegate,
                               public ContextMenuController,
                               public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(ScrollBar, View)

 public:
  // An enumeration of different amounts of incremental scroll, representing
  // events sent from different parts of the UI/keyboard.
  enum class ScrollAmount {
    kNone,
    kStart,
    kEnd,
    kPrevLine,
    kNextLine,
    kPrevPage,
    kNextPage,
  };

  // Whether the scrollbar is horizontal or vertical.
  enum class Orientation : bool {
    kHorizontal,
    kVertical,
  };

  ScrollBar(const ScrollBar&) = delete;
  ScrollBar& operator=(const ScrollBar&) = delete;

  ~ScrollBar() override;

  Orientation GetOrientation() const;

  void set_controller(ScrollBarController* controller) {
    controller_ = controller;
  }
  ScrollBarController* controller() const { return controller_; }

  void SetThumb(BaseScrollBarThumb* thumb);

  // Scroll the contents by the specified type (see ScrollAmount above).
  bool ScrollByAmount(ScrollAmount amount);

  // Scroll the contents to the appropriate position given the supplied
  // position of the thumb (thumb track coordinates). If |scroll_to_middle| is
  // true, then the conversion assumes |thumb_position| is in the middle of the
  // thumb rather than the top.
  void ScrollToThumbPosition(int thumb_position, bool scroll_to_middle);

  // Scroll the contents by the specified offset (contents coordinates).
  bool ScrollByContentsOffset(int contents_offset);

  // Returns the max and min positions.
  int GetMaxPosition() const;
  int GetMinPosition() const;

  // Returns the position of the scrollbar.
  int GetPosition() const;

  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ScrollDelegate:
  bool OnScroll(float dx, float dy) override;
  void OnFlingScrollEnded() override;

  // ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int id) const override;
  bool IsCommandIdEnabled(int id) const override;
  void ExecuteCommand(int id, int event_flags) override;

  // Returns true if the scrollbar should sit on top of the content area (e.g.
  // for overlay scrollbars).
  virtual bool OverlapsContent() const;

  // Update the scrollbar appearance given a viewport size, content size and
  // current position.
  virtual void Update(int viewport_size,
                      int content_size,
                      int contents_scroll_offset);

  // Called when a ScrollEvent (in any, or no, direction) is seen by the parent
  // ScrollView. E.g., this may reveal an overlay scrollbar to indicate
  // possible scrolling directions to the user.
  virtual void ObserveScrollEvent(const ui::ScrollEvent& event);

  // Get the bounds of the "track" area that the thumb is free to slide within.
  virtual gfx::Rect GetTrackBounds() const = 0;

  // Get the width or height of this scrollbar. For a vertical scrollbar, this
  // is the width of the scrollbar, likewise it is the height for a horizontal
  // scrollbar.
  virtual int GetThickness() const = 0;

  // Gets or creates ScrollAnimator if it does not exist.
  ScrollAnimator* GetOrCreateScrollAnimator();

  // Sets `fling_multiplier_` which is used to modify animation velocities
  // in `scroll_animator_`.
  void SetFlingMultiplier(float fling_multiplier);

  bool is_scrolling() const {
    return scroll_status_ == ScrollStatus::kScrollInProgress;
  }

 protected:
  // Create new scrollbar, either horizontal or vertical. These are protected
  // since you need to be creating either a NativeScrollBar or a
  // ImageScrollBar.
  explicit ScrollBar(Orientation orientation);

  BaseScrollBarThumb* GetThumb() const;

  // Wrapper functions that calls the controller. We need this since native
  // scrollbars wrap around a different scrollbar. When calling the controller
  // we need to pass in the appropriate scrollbar. For normal scrollbars it's
  // the |this| scrollbar, for native scrollbars it's the native scrollbar used
  // to create this.
  virtual void ScrollToPosition(int position);
  virtual int GetScrollIncrement(bool is_page, bool is_positive);

 private:
  friend class test::ScrollViewTestApi;
  FRIEND_TEST_ALL_PREFIXES(ScrollBarViewsTest, ScrollBarFitsToBottom);
  FRIEND_TEST_ALL_PREFIXES(ScrollBarViewsTest, ThumbFullLengthOfTrack);
  FRIEND_TEST_ALL_PREFIXES(ScrollBarViewsTest, DragThumbScrollsContent);
  FRIEND_TEST_ALL_PREFIXES(ScrollBarViewsTest,
                           DragThumbScrollsContentWhenSnapBackDisabled);
  FRIEND_TEST_ALL_PREFIXES(ScrollBarViewsTest, RightClickOpensMenu);
  FRIEND_TEST_ALL_PREFIXES(ScrollBarViewsTest, TestPageScrollingByPress);

  static base::RetainingOneShotTimer* GetHideTimerForTesting(
      ScrollBar* scroll_bar);
  int GetThumbLengthForTesting();

  // Changes to 'pushed' state and starts a timer to scroll repeatedly.
  void ProcessPressEvent(const ui::LocatedEvent& event);

  // Called when the mouse is pressed down in the track area.
  void TrackClicked();

  // Responsible for scrolling the contents and also updating the UI to the
  // current value of the Scroll Offset.
  void ScrollContentsToOffset();

  // Returns the size (width or height) of the track area of the ScrollBar.
  int GetTrackSize() const;

  // Calculate the position of the thumb within the track based on the
  // specified scroll offset of the contents.
  int CalculateThumbPosition(int contents_scroll_offset) const;

  // Calculates the current value of the contents offset (contents coordinates)
  // based on the current thumb position (thumb track coordinates). See
  // ScrollToThumbPosition() for an explanation of |scroll_to_middle|.
  int CalculateContentsOffset(float thumb_position,
                              bool scroll_to_middle) const;

  // Sets |contents_scroll_offset_| by given |contents_scroll_offset|.
  // |contents_scroll_offset| is clamped between GetMinPosition() and
  // GetMaxPosition().
  void SetContentsScrollOffset(int contents_scroll_offset);

  ScrollAmount DetermineScrollAmountByKeyCode(
      const ui::KeyboardCode& keycode) const;

  std::optional<int> GetDesiredScrollOffset(ScrollAmount amount);

  // The size of the scrolled contents, in pixels.
  int contents_size_ = 0;

  // The current amount the contents is offset by in the viewport.
  int contents_scroll_offset_ = 0;

  // The current size of the view port, in pixels.
  int viewport_size_ = 0;

  // The last amount of incremental scroll that this scrollbar performed. This
  // is accessed by the callbacks for the auto-repeat up/down buttons to know
  // what direction to repeatedly scroll in.
  ScrollAmount last_scroll_amount_ = ScrollAmount::kNone;

  // The position of the mouse within the scroll bar when the context menu
  // was invoked.
  int context_menu_mouse_position_ = 0;

  const Orientation orientation_;

  raw_ptr<BaseScrollBarThumb> thumb_ = nullptr;

  raw_ptr<ScrollBarController> controller_ = nullptr;

  int max_pos_ = 0;

  float fling_multiplier_ = 1.f;

  // An instance of a RepeatController which scrolls the scrollbar continuously
  // as the user presses the mouse button down on the up/down buttons or the
  // track.
  RepeatController repeater_;

  // Difference between current position and cumulative deltas obtained from
  // scroll update events.
  // TODO(tdresser): This should be removed when raw pixel scrolling for views
  // is enabled. See crbug.com/329354.
  gfx::Vector2dF roundoff_error_;

  // The enumeration keeps track of the current status of the scroll. Used when
  // the contents scrolled by the gesture or scroll events sequence.
  enum class ScrollStatus {
    kScrollNone,
    kScrollStarted,
    kScrollInProgress,

    // The contents will keep scrolling for a while if the events sequence ends
    // with ui::EventType::kScrollFlingStart. Set the status to kScrollInEnding
    // if it happens, and set it to kScrollEnded while the scroll really ended.
    kScrollInEnding,
    kScrollEnded,
  };

  ScrollStatus scroll_status_ = ScrollStatus::kScrollNone;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<MenuRunner> menu_runner_;
  // Used to animate gesture flings on the scroll bar.
  std::unique_ptr<ScrollAnimator> scroll_animator_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_SCROLL_BAR_H_
