// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event.h"

#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_enum_util.h"

namespace ui {

AXEvent::AXEvent() = default;

AXEvent::AXEvent(AXNodeData::AXID id,
                 ax::mojom::Event event_type,
                 ax::mojom::EventFrom event_from,
                 ax::mojom::Action event_from_action,
                 const std::vector<AXEventIntent>& event_intents,
                 int action_request_id)
    : id(id),
      event_type(event_type),
      event_from(event_from),
      event_from_action(event_from_action),
      event_intents(event_intents),
      action_request_id(action_request_id) {}

AXEvent::~AXEvent() = default;

AXEvent::AXEvent(AXEvent&& other) = default;

AXEvent& AXEvent::operator=(AXEvent&& other) = default;

AXEvent::AXEvent(const AXEvent& event) = default;

AXEvent& AXEvent::operator=(const AXEvent& event) = default;

std::string AXEvent::ToString() const {
  std::string result = "AXEvent ";

  result += ui::ToString(event_type);
  result += " on node id=" + base::NumberToString(id);
  if (event_from != ax::mojom::EventFrom::kNone)
    result += std::string(" from ") + ui::ToString(event_from);
  if (event_from_action != ax::mojom::Action::kNone)
    result += std::string(" from accessibility action ") +
              ui::ToString(event_from_action);
  if (!event_intents.empty()) {
    result += " caused by [ ";
    for (const AXEventIntent& intent : event_intents) {
      result += intent.ToString() + ' ';
    }
    result += ']';
  }
  if (action_request_id)
    result += " action_request_id=" + base::NumberToString(action_request_id);
  return result;
}

}  // namespace ui
