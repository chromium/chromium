// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_THUMB_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_THUMB_H_

#include "base/macros.h"
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
 public:
  METADATA_HEADER(BaseScrollBarThumb);

  explicit BaseScrollBarThumb(ScrollBar* scroll_bar);
  ~BaseScrollBarThumb() override;

  // Sets the length (width or height) of the thumb to the specified value.
  void SetLength(int length);

  // Retrieves the size (width or height) of the thumb.
  int GetSize() const;

  // Sets the position of the thumb on the x or y axis.
  void SetPosition(int position);

  // Gets the position of the thumb on the x or y axis.
  int GetPosition() const;

  // View overrides:
  gfx::Size CalculatePreferredSize() const override = 0;

 protected:
  // View overrides:
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
  ScrollBar* scroll_bar_;

  int drag_start_position_;

  // The position of the mouse on the scroll axis relative to the top of this
  // View when the drag started.
  int mouse_offset_;

  // The current state of the thumb button.
  Button::ButtonState state_;

  DISALLOW_COPY_AND_ASSIGN(BaseScrollBarThumb);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_BASE_SCROLL_BAR_THUMB_H_
