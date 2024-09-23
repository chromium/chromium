// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_THUMB_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_THUMB_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

namespace views {

class ScrollBar;

///////////////////////////////////////////////////////////////////////////////
//
// BaseScrollBarThumb
//
//  A view that acts as the thumb in the scroll bar track that the user can
//  drag to scroll the associated contents view within the viewport.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BaseScrollBarThumb : public View {
  METADATA_HEADER(BaseScrollBarThumb, View)

 public:
  explicit BaseScrollBarThumb(ScrollBar* scroll_bar);

  BaseScrollBarThumb(const BaseScrollBarThumb&) = delete;
  BaseScrollBarThumb& operator=(const BaseScrollBarThumb&) = delete;

  ~BaseScrollBarThumb() override;

  // Sets the length (width or height) of the thumb to the specified value.
  void SetLength(int length);

  // Retrieves the length (width or height) of the thumb.
  int GetLength() const;

  // Sets the position of the thumb on the x or y axis.
  void SetPosition(int position);

  // Gets the position of the thumb on the x or y axis.
  int GetPosition() const;

  // Sets whether a drag that starts on the scroll thumb and then moves far
  // outside the thumb should "snap back" to the original scroll position.
  void SetSnapBackOnDragOutside(bool value);
  bool GetSnapBackOnDragOutside() const;

  // View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override = 0;

 protected:
  // View:
  void OnPaint(gfx::Canvas* canvas) override = 0;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;

  Button::ButtonState GetState() const;
  // Update our state and schedule a repaint when the mouse moves over us.
  void SetState(Button::ButtonState state);
  virtual void OnStateChanged();

  bool IsHorizontal() const;

  ScrollBar* scroll_bar() { return scroll_bar_; }

 private:
  // The ScrollBar that owns us.
  raw_ptr<ScrollBar> scroll_bar_;

  // See SetSnapBackOnDragOutside() above.
  bool snap_back_on_drag_outside_ = true;

  int drag_start_position_ = -1;

  // The position of the mouse on the scroll axis relative to the top of this
  // View when the drag started.
  int mouse_offset_ = -1;

  // The current state of the thumb button.
  Button::ButtonState state_ = Button::STATE_NORMAL;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_THUMB_H_
