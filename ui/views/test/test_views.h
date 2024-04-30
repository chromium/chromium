// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_VIEWS_H_
#define UI_VIEWS_TEST_TEST_VIEWS_H_

#include <map>
#include <memory>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/types/event_type.h"
#include "ui/views/view.h"

namespace views {

// A view that requests a set amount of space.
class StaticSizedView : public View {
  METADATA_HEADER(StaticSizedView, View)

 public:
  explicit StaticSizedView(const gfx::Size& preferred_size = gfx::Size());

  StaticSizedView(const StaticSizedView&) = delete;
  StaticSizedView& operator=(const StaticSizedView&) = delete;

  ~StaticSizedView() override;

  void set_minimum_size(const gfx::Size& minimum_size) {
    minimum_size_ = minimum_size;
    InvalidateLayout();
  }

  void set_maximum_size(const gfx::Size& maximum_size) {
    maximum_size_ = maximum_size;
    InvalidateLayout();
  }

  // View overrides:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

 private:
  gfx::Size preferred_size_;
  gfx::Size minimum_size_;
  gfx::Size maximum_size_;
};

// A view that accomodates testing layouts that use GetHeightForWidth.
class ProportionallySizedView : public View {
  METADATA_HEADER(ProportionallySizedView, View)

 public:
  explicit ProportionallySizedView(int factor);

  ProportionallySizedView(const ProportionallySizedView&) = delete;
  ProportionallySizedView& operator=(const ProportionallySizedView&) = delete;

  ~ProportionallySizedView() override;

  void SetPreferredWidth(int width);

  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;

 private:
  // The multiplicative factor between width and height, i.e.
  // height = width * factor_.
  int factor_;

  // The width used as the preferred size. -1 if not used.
  int preferred_width_ = -1;
};

// Class that closes the widget (which ends up deleting it immediately) when the
// appropriate event is received.
class CloseWidgetView : public View {
  METADATA_HEADER(CloseWidgetView, View)

 public:
  explicit CloseWidgetView(ui::EventType event_type);

  CloseWidgetView(const CloseWidgetView&) = delete;
  CloseWidgetView& operator=(const CloseWidgetView&) = delete;

  // ui::EventHandler override:
  void OnEvent(ui::Event* event) override;

 private:
  const ui::EventType event_type_;
};

// A view that keeps track of the events it receives, optionally consuming them.
class EventCountView : public View {
  METADATA_HEADER(EventCountView, View)

 public:
  // Whether to call SetHandled() on events as they are received. For some event
  // types, this will allow EventCountView to receives future events in the
  // event sequence, such as a drag.
  enum HandleMode { PROPAGATE_EVENTS, CONSUME_EVENTS };

  EventCountView();

  EventCountView(const EventCountView&) = delete;
  EventCountView& operator=(const EventCountView&) = delete;

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
};

// A view which reacts to PreferredSizeChanged() from its children by doing
// layout.
class ResizeAwareParentView : public View {
  METADATA_HEADER(ResizeAwareParentView, View)

 public:
  ResizeAwareParentView();

  ResizeAwareParentView(const ResizeAwareParentView&) = delete;
  ResizeAwareParentView& operator=(const ResizeAwareParentView&) = delete;

  // Overridden from View:
  void ChildPreferredSizeChanged(View* child) override;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_VIEWS_H_
