// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_REWRITER_CONTINUATION_H_
#define UI_EVENTS_TEST_TEST_EVENT_REWRITER_CONTINUATION_H_

#include "ui/events/event_rewriter_continuation.h"

namespace ui {
namespace test {

// This interface exposes EventRewriterContinuation interface to
// test code outside of ui/events.
class TestEventRewriterContinuation : public ui::EventRewriterContinuation {
 public:
  TestEventRewriterContinuation() = default;

  TestEventRewriterContinuation(const TestEventRewriterContinuation&) = delete;
  TestEventRewriterContinuation& operator=(
      const TestEventRewriterContinuation&) = delete;

  ~TestEventRewriterContinuation() override = default;
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_TEST_EVENT_REWRITER_CONTINUATION_H_
