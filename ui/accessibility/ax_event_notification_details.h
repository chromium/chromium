// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_EVENT_NOTIFICATION_DETAILS_H_
#define UI_ACCESSIBILITY_AX_EVENT_NOTIFICATION_DETAILS_H_

#include <vector>

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

struct AX_BASE_EXPORT AXEventNotificationDetails {
 public:
  AXEventNotificationDetails();

  AXEventNotificationDetails(const AXEventNotificationDetails& other) = delete;
  AXEventNotificationDetails& operator=(
      const AXEventNotificationDetails& other) = delete;

  ~AXEventNotificationDetails();

  // The unique ID of the accessibility tree this event bundle applies to.
  ui::AXTreeID ax_tree_id;

  // Zero or more updates to the accessibility tree to apply first.
  std::vector<ui::AXTreeUpdate> updates;

  // Zero or more events to fire after the tree updates have been applied.
  std::vector<ui::AXEvent> events;
};

struct AX_BASE_EXPORT AXLocationChangeNotificationDetails {
 public:
  AXLocationChangeNotificationDetails();
  AXLocationChangeNotificationDetails(
      const AXLocationChangeNotificationDetails& other);
  ~AXLocationChangeNotificationDetails();

  int id;
  ui::AXTreeID ax_tree_id;
  ui::AXRelativeBounds new_location;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_EVENT_NOTIFICATION_DETAILS_H_
