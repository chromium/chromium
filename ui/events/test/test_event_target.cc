// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/test_event_target.h"

#include <algorithm>
#include <utility>

#include "ui/events/event.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/event_targeter.h"

namespace ui {
namespace test {

TestEventTarget::TestEventTarget()
    : parent_(nullptr),
      mark_events_as_handled_(false),
      recorder_(nullptr),
      target_name_("unknown") {
  SetTargetHandler(this);
}
TestEventTarget::~TestEventTarget() {}

void TestEventTarget::AddChild(std::unique_ptr<TestEventTarget> child) {
  DCHECK(!child->parent());
  children_.push_back(std::move(child));
  children_.back()->set_parent(this);
}

std::unique_ptr<TestEventTarget> TestEventTarget::RemoveChild(
    TestEventTarget* c) {
  for (auto iter = children_.begin(); iter != children_.end(); ++iter) {
    if (iter->get() == c) {
      std::unique_ptr<TestEventTarget> child = std::move(*iter);
      children_.erase(iter);
      child->set_parent(nullptr);
      return child;
    }
  }
  return nullptr;
}

void TestEventTarget::SetEventTargeter(
    std::unique_ptr<EventTargeter> targeter) {
  targeter_ = std::move(targeter);
}

bool TestEventTarget::DidReceiveEvent(ui::EventType type) const {
  return received_.count(type) > 0;
}

void TestEventTarget::ResetReceivedEvents() {
  received_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// TestEventTarget, protected

bool TestEventTarget::CanAcceptEvent(const ui::Event& event) {
  return true;
}

EventTarget* TestEventTarget::GetParentTarget() {
  return parent_;
}

std::unique_ptr<EventTargetIterator> TestEventTarget::GetChildIterator() const {
  return std::make_unique<EventTargetIteratorUniquePtrImpl<TestEventTarget>>(
      children_);
}

EventTargeter* TestEventTarget::GetEventTargeter() {
  return targeter_.get();
}

void TestEventTarget::OnEvent(Event* event) {
  if (recorder_)
    recorder_->push_back(target_name_);
  received_.insert(event->type());
  EventHandler::OnEvent(event);
  if (!event->handled() && mark_events_as_handled_)
    event->SetHandled();
}

////////////////////////////////////////////////////////////////////////////////
// TestEventTarget, private

bool TestEventTarget::Contains(TestEventTarget* target) const {
  while (target && target != this)
    target = target->parent();
  return target == this;
}

}  // namespace test
}  // namespace ui
