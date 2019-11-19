// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_EVENT_H_
#define UI_ACCESSIBILITY_AX_EVENT_H_

#include <string>

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

struct AX_EXPORT AXEvent {
  AXEvent();
  virtual ~AXEvent();

  // The type of event.
  ax::mojom::Event event_type;

  // The id of the node in the AXTree that the event should be fired on.
  int id = -1;

  // The source of the event.
  ax::mojom::EventFrom event_from;

  // The action request ID that was passed in if this event was fired in
  // direct response to a ax::mojom::Action.
  int action_request_id = -1;

  // Return a string representation of this data, for debugging.
  std::string ToString() const;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_EVENT_H_
