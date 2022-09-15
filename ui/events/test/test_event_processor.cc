// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/test_event_processor.h"

#include <utility>

#include "ui/events/event_target.h"

namespace ui {
namespace test {

TestEventProcessor::TestEventProcessor()
    : should_processing_occur_(true),
      num_times_processing_started_(0),
      num_times_processing_finished_(0) {
}

TestEventProcessor::~TestEventProcessor() {}

EventTarget* TestEventProcessor::GetRoot() {
  return root_.get();
}

void TestEventProcessor::SetRoot(std::unique_ptr<EventTarget> root) {
  root_ = std::move(root);
}

void TestEventProcessor::Reset() {
  should_processing_occur_ = true;
  num_times_processing_started_ = 0;
  num_times_processing_finished_ = 0;
}

bool TestEventProcessor::CanDispatchToTarget(EventTarget* target) {
  return true;
}

EventTarget* TestEventProcessor::GetRootForEvent(Event* event) {
  return root_.get();
}

EventTargeter* TestEventProcessor::GetDefaultEventTargeter() {
  return root_->GetEventTargeter();
}

EventDispatchDetails TestEventProcessor::OnEventFromSource(Event* event) {
  return EventProcessor::OnEventFromSource(event);
}

void TestEventProcessor::OnEventProcessingStarted(Event* event) {
  num_times_processing_started_++;
  if (!should_processing_occur_)
    event->SetHandled();
}

void TestEventProcessor::OnEventProcessingFinished(Event* event) {
  num_times_processing_finished_++;
}

}  // namespace test
}  // namespace ui
