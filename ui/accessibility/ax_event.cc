// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event.h"

#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

// Mojo enums are initialized here so the header can include the much smaller
// mojom-forward.h header.
AXEvent::AXEvent()
    : event_type(ax::mojom::Event::kNone),
      event_from(ax::mojom::EventFrom::kNone) {}

AXEvent::~AXEvent() = default;

std::string AXEvent::ToString() const {
  std::string result = "AXEvent";

  result += ui::ToString(event_type);
  result += " on node id=" + base::NumberToString(id);
  if (event_from != ax::mojom::EventFrom::kNone)
    result += std::string(" from ") + ui::ToString(event_from);
  if (action_request_id)
    result += " action_request_id=" + base::NumberToString(action_request_id);
  return result;
}

}  // namespace ui
