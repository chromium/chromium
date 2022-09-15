// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_HANDLER_H_
#define UI_EVENTS_TEST_TEST_EVENT_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

typedef std::vector<std::string> HandlerSequenceRecorder;

namespace ui {
namespace test {

// A simple EventHandler that keeps track of the number of key events that it's
// seen.
class TestEventHandler : public EventHandler {
 public:
  TestEventHandler();

  TestEventHandler(const TestEventHandler&) = delete;
  TestEventHandler& operator=(const TestEventHandler&) = delete;

  ~TestEventHandler() override;

  int num_key_events() const { return num_key_events_; }
  int num_mouse_events() const { return num_mouse_events_; }
  int num_scroll_events() const { return num_scroll_events_; }
  int num_touch_events() const { return num_touch_events_; }
  int num_gesture_events() const { return num_gesture_events_; }

  void Reset();

  void set_recorder(HandlerSequenceRecorder* recorder) {
    recorder_ = recorder;
  }
  void set_handler_name(const std::string& handler_name) {
    handler_name_ = handler_name;
  }
  const std::string& handler_name() const { return handler_name_; }

  // EventHandler overrides:
  void OnKeyEvent(KeyEvent* event) override;
  void OnMouseEvent(MouseEvent* event) override;
  void OnScrollEvent(ScrollEvent* event) override;
  void OnTouchEvent(TouchEvent* event) override;
  void OnGestureEvent(GestureEvent* event) override;

 private:
  // How many events have been received of each type?
  int num_key_events_;
  int num_mouse_events_;
  int num_scroll_events_;
  int num_touch_events_;
  int num_gesture_events_;

  raw_ptr<HandlerSequenceRecorder> recorder_;
  std::string handler_name_;
};

}  // namespace test
}  // namespace ui

#endif // UI_EVENTS_TEST_TEST_EVENT_HANDLER_H_
