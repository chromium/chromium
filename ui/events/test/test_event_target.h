// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_TARGET_H_
#define UI_EVENTS_TEST_TEST_EVENT_TARGET_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/events/event_target.h"
#include "ui/events/types/event_type.h"

typedef std::vector<std::string> HandlerSequenceRecorder;

namespace ui {
namespace test {

class TestEventTarget : public EventTarget,
                        public EventHandler {
 public:
  TestEventTarget();

  TestEventTarget(const TestEventTarget&) = delete;
  TestEventTarget& operator=(const TestEventTarget&) = delete;

  ~TestEventTarget() override;

  void AddChild(std::unique_ptr<TestEventTarget> child);
  std::unique_ptr<TestEventTarget> RemoveChild(TestEventTarget* child);

  TestEventTarget* parent() { return parent_; }

  void set_mark_events_as_handled(bool handle) {
    mark_events_as_handled_ = handle;
  }

  TestEventTarget* child_at(int index) { return children_[index].get(); }
  size_t child_count() const { return children_.size(); }

  void SetEventTargeter(std::unique_ptr<EventTargeter> targeter);

  bool DidReceiveEvent(ui::EventType type) const;
  void ResetReceivedEvents();

  void set_recorder(HandlerSequenceRecorder* recorder) {
    recorder_ = recorder;
  }
  void set_target_name(const std::string& target_name) {
    target_name_ = target_name;
  }

  // EventTarget:
  EventTargeter* GetEventTargeter() override;

 protected:
  bool Contains(TestEventTarget* target) const;

  // EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override;
  EventTarget* GetParentTarget() override;
  std::unique_ptr<EventTargetIterator> GetChildIterator() const override;

  // EventHandler:
  void OnEvent(Event* event) override;

 private:
  void set_parent(TestEventTarget* parent) { parent_ = parent; }

  raw_ptr<TestEventTarget> parent_;
  std::vector<std::unique_ptr<TestEventTarget>> children_;
  std::unique_ptr<EventTargeter> targeter_;
  bool mark_events_as_handled_;

  std::set<ui::EventType> received_;

  raw_ptr<HandlerSequenceRecorder> recorder_;
  std::string target_name_;
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_TEST_EVENT_TARGET_H_
