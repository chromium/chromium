// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_REWRITER_H_
#define UI_EVENTS_TEST_TEST_EVENT_REWRITER_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/event_rewriter.h"

namespace ui {
namespace test {

// Counts number of events observed.
class TestEventRewriter : public ui::EventRewriter {
 public:
  TestEventRewriter();
  ~TestEventRewriter() override;

  void clear_events_seen() { events_seen_ = 0; }
  int events_seen() const { return events_seen_; }
  const ui::Event* last_event() const { return last_event_.get(); }
  void ResetLastEvent() { last_event_.reset(); }

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  int events_seen_ = 0;
  std::unique_ptr<ui::Event> last_event_;

  DISALLOW_COPY_AND_ASSIGN(TestEventRewriter);
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_TEST_EVENT_REWRITER_H_
