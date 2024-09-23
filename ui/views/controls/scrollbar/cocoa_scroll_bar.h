// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_COCOA_SCROLL_BAR_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_COCOA_SCROLL_BAR_H_

#include "base/timer/timer.h"
#import "components/remote_cocoa/app_shim/views_scrollbar_bridge.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/views_export.h"

namespace views {

class CocoaScrollBarThumb;

// The transparent scrollbar for Mac which overlays its contents.
class VIEWS_EXPORT CocoaScrollBar : public ScrollBar,
                                    public ViewsScrollbarBridgeDelegate,
                                    public ui::ImplicitAnimationObserver,
                                    public gfx::AnimationDelegate {
  METADATA_HEADER(CocoaScrollBar, ScrollBar)

 public:
  explicit CocoaScrollBar(ScrollBar::Orientation orientation);

  CocoaScrollBar(const CocoaScrollBar&) = delete;
  CocoaScrollBar& operator=(const CocoaScrollBar&) = delete;

  ~CocoaScrollBar() override;

  // ScrollBar:
  void Update(int viewport_size,
              int content_size,
              int contents_scroll_offset) override;
  void ObserveScrollEvent(const ui::ScrollEvent& event) override;

  // ViewsScrollbarBridgeDelegate:
  void OnScrollerStyleChanged() override;

  // View:
  bool GetCanProcessEventsWithinSubtree() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Returns the scroller style.
  NSScrollerStyle GetScrollerStyle() const { return scroller_style_; }

  // Returns the thickness of the scrollbar.
  int ScrollbarThickness() const;

  // Returns true if the opacity is 0.0.
  bool IsScrollbarFullyHidden() const;

  // Get the parameters for painting.
  ui::NativeTheme::ExtraParams GetPainterParams() const;

 protected:
  // ScrollBar:
  gfx::Rect GetTrackBounds() const override;

  // ScrollBar:
  int GetThickness() const override;
  bool OverlapsContent() const override;

  // View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  friend class ScrollBar;  // For ScrollBar::GetHideTimerForTesting().

  // Methods to change the visibility of the scrollbar.
  void ShowScrollbar();
  void HideScrollbar();

  // Returns true if the scrollbar is in a hover or pressed state.
  bool IsHoverOrPressedState() const;

  // Updates the thickness of the scrollbar according to the current state of
  // the expand animation.
  void UpdateScrollbarThickness();

  // Resets the scrolltrack and the thickness if the scrollbar is not hidden
  // and is not in a hover/pressed state.
  void ResetOverlayScrollbar();

  // Sets the scrolltrack's visibility and then repaints it.
  void SetScrolltrackVisible(bool visible);

  // Converts GetThumb() into a CocoaScrollBarThumb object and returns it.
  CocoaScrollBarThumb* GetCocoaScrollBarThumb() const;

  // Scroller style the scrollbar is using.
  NSScrollerStyle scroller_style_;

  // Timer that will start the scrollbar's hiding animation when it reaches 0.
  base::RetainingOneShotTimer hide_scrollbar_timer_;

  // Slide animation that animates the thickness of an overlay scrollbar.
  // The animation expands the scrollbar as the showing animation and shrinks
  // the scrollbar as the hiding animation.
  gfx::SlideAnimation thickness_animation_;

  // The scroll offset from the last adjustment to the scrollbar.
  int last_contents_scroll_offset_ = 0;

  // True when the scrollbar is expanded.
  bool is_expanded_ = false;

  // True when the scrolltrack should be drawn.
  bool has_scrolltrack_;

  // True when the scrollbar has started dragging since it was last shown.
  // This is set to false when we begin to hide the scrollbar.
  bool did_start_dragging_ = false;

  // The bridge for NSScroller.
  ViewsScrollbarBridge* __strong bridge_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_COCOA_SCROLL_BAR_H_
