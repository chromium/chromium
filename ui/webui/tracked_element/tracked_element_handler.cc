// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/tracked_element_handler.h"

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace ui {

TrackedElementHandler::TrackedElementHandler(
    content::WebContents* web_contents,
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        receiver,
    ui::ElementContext context,
    const std::vector<ui::ElementIdentifier>& identifiers)
    : context_(context),
      web_contents_(web_contents),
      receiver_(this, std::move(receiver)) {
  for (const ui::ElementIdentifier& id : identifiers) {
    elements_[id] = std::make_unique<TrackedElementWebUI>(this, id, context);
  }
}

TrackedElementHandler::~TrackedElementHandler() = default;

void TrackedElementHandler::TrackedElementVisibilityChanged(
    const std::string& identifier_name,
    bool visible,
    const gfx::RectF& rect) {
  TrackedElementWebUI* element = GetElement(identifier_name);
  if (!element) {
    return;
  }
  element->SetVisible(visible, rect);
}

void TrackedElementHandler::TrackedElementActivated(
    const std::string& identifier_name) {
  TrackedElementWebUI* const element = GetElement(identifier_name);
  if (!element) {
    return;
  }
  if (!element->visible()) {
    receiver_.ReportBadMessage(
        base::StringPrintf("TrackedElementActivated message received for "
                           "anchor element \"%s\" but element was not visible.",
                           identifier_name.c_str()));
    return;
  }
  element->Activate();
}

void TrackedElementHandler::TrackedElementCustomEvent(
    const std::string& identifier_name,
    const std::string& event_name) {
  TrackedElementWebUI* const element = GetElement(identifier_name);
  if (!element) {
    return;
  }
  if (!element->visible()) {
    receiver_.ReportBadMessage(
        base::StringPrintf("TrackedElementCustomEvent message received for "
                           "anchor element \"%s\" but element was not visible.",
                           identifier_name.c_str()));
    return;
  }
  const ui::CustomElementEventType event_type =
      ui::CustomElementEventType::FromName(event_name.c_str());
  if (!event_type) {
    return;
  }
  element->CustomEvent(event_type);
}

TrackedElementWebUI* TrackedElementHandler::GetElement(
    const std::string& identifier_name) {
  for (const auto& [id, element] : elements_) {
    if (id.GetName() == identifier_name) {
      return element.get();
    }
  }

  LOG(ERROR) << "TrackedElement message received for element \""
             << identifier_name
             << "\" but element was not known to the handler.";
  return nullptr;
}

}  // namespace ui
