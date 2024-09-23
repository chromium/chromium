// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_DISCARD_KEY_EVENT_REWRITER_H_
#define UI_EVENTS_ASH_DISCARD_KEY_EVENT_REWRITER_H_

#include "ui/events/event_rewriter.h"

namespace ui {

// This rewriter remaps modifier key events based on settings/preferences.
// Also, updates modifier flags along with the remapping not only
// for KeyEvent instances but also motion Event instances, such as mouse
// events and touch events.
class DiscardKeyEventRewriter : public EventRewriter {
 public:
  DiscardKeyEventRewriter();
  DiscardKeyEventRewriter(const DiscardKeyEventRewriter&) = delete;
  DiscardKeyEventRewriter& operator=(const DiscardKeyEventRewriter&) = delete;
  ~DiscardKeyEventRewriter() override;

  // EventRewriter:
  EventDispatchDetails RewriteEvent(const Event& event,
                                    const Continuation continuation) override;
};

}  // namespace ui

#endif  // UI_EVENTS_ASH_DISCARD_KEY_EVENT_REWRITER_H_
