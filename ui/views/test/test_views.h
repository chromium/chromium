// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_VIEWS_H_
#define UI_VIEWS_TEST_TEST_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/event_constants.h"
#include "ui/views/view.h"

namespace views {

// A view that requests a set amount of space.
class StaticSizedView : public View {
 public:
  explicit StaticSizedView(const gfx::Size& preferred_size = gfx::Size());
  ~StaticSizedView() override;

  void set_minimum_size(const gfx::Size& minimum_size) {
    minimum_size_ = minimum_size;
  }

  void set_maximum_size(const gfx::Size& maximum_size) {
    maximum_size_ = maximum_size;
  }

  // View overrides:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

 private:
  gfx::Size preferred_size_;
  gfx::Size minimum_size_;
  gfx::Size maximum_size_;

  DISALLOW_COPY_AND_ASSIGN(StaticSizedView);
};

// A view that accomodates testing layouts that use GetHeightForWidth.
class ProportionallySizedView : public View {
 public:
  explicit ProportionallySizedView(int factor);
  ~ProportionallySizedView() override;

  void SetPreferredWidth(int width);

  int GetHeightForWidth(int w) const override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  // The multiplicative factor between width and height, i.e.
  // height = width * factor_.
  int factor_;

  // The width used as the preferred size. -1 if not used.
  int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(ProportionallySizedView);
};

// Class that closes the widget (which ends up deleting it immediately) when the
// appropriate event is received.
class CloseWidgetView : public View {
 public:
  explicit CloseWidgetView(ui::EventType event_type);

  // ui::EventHandler override:
  void OnEvent(ui::Event* event) override;

 private:
  const ui::EventType event_type_;

  DISALLOW_COPY_AND_ASSIGN(CloseWidgetView);
};

// A view that keeps track of the events it receives, optionally consuming them.
class EventCountView : public View {
 public:
  // Whether to call SetHandled() on events as they are received. For some event
  // types, this will allow EventCountView to receives future events in the
  // event sequence, such as a drag.
  enum HandleMode { PROPAGATE_EVENTS, CONSUME_EVENTS };

  EventCountView();
  ~EventCountView() override;

  int GetEventCount(ui::EventType type);
  void ResetCounts();

  int last_flags() const { return last_flags_; }

  void set_handle_mode(HandleMode handle_mode) { handle_mode_ = handle_mode; }

 protected:
  // Overridden from View:
  void OnMouseMoved(const ui::MouseEvent& event) override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  void RecordEvent(ui::Event* event);

  std::map<ui::EventType, int> event_count_;
  int last_flags_ = 0;
  HandleMode handle_mode_ = PROPAGATE_EVENTS;

  DISALLOW_COPY_AND_ASSIGN(EventCountView);
};

// A view which reacts to PreferredSizeChanged() from its children and calls
// Layout().
class ResizeAwareParentView : public View {
 public:
  ResizeAwareParentView();

  // Overridden from View:
  void ChildPreferredSizeChanged(View* child) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResizeAwareParentView);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_VIEWS_H_
