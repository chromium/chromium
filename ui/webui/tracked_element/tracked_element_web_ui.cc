// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/tracked_element_web_ui.h"

#include "base/check.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/native_ui_util.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"

namespace ui {

TrackedElementWebUI::TrackedElementWebUI(TrackedElementHandler* handler,
                                         ui::ElementIdentifier identifier,
                                         ui::ElementContext context)
    : TrackedElement(identifier, context), handler_(handler) {
  DCHECK(handler);
}

TrackedElementWebUI::~TrackedElementWebUI() {
  SetVisible(false);
}

gfx::Rect TrackedElementWebUI::GetScreenBounds() const {
  gfx::Rect result;
  content::WebContents* const contents = handler_->web_contents();
  if (contents) {
    // Use the last known bounds, but if the bounds are empty, make them 1x1 so
    // there's something to anchor to.
    result = gfx::ToRoundedRect(last_known_bounds_);
    if (result.width() < 1) {
      result.set_width(1);
    }
    if (result.height() < 1) {
      result.set_height(1);
    }
    // To get the screen coordinates, have to offset by the coordinates of the
    // viewport.
    result.Offset(contents->GetContainerBounds().OffsetFromOrigin());
  }
  return result;
}

gfx::NativeView TrackedElementWebUI::GetNativeView() const {
  return gfx::GetViewForWindow(
      handler_->web_contents()->GetTopLevelNativeWindow());
}

void TrackedElementWebUI::SetVisible(bool visible, gfx::RectF bounds) {
  if (visible == visible_) {
    if (visible && last_known_bounds_ != bounds) {
      last_known_bounds_ = bounds;
      // This event signals that the bounds of the element have been updated.
      ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          this, kElementBoundsChangedEvent);
    }
    return;
  }

  last_known_bounds_ = bounds;
  visible_ = visible;
  auto* const delegate = ui::ElementTracker::GetFrameworkDelegate();
  if (visible) {
    delegate->NotifyElementShown(this);
  } else {
    delegate->NotifyElementHidden(this);
  }
}

void TrackedElementWebUI::Activate() {
  DCHECK(visible_);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(this);
}

void TrackedElementWebUI::CustomEvent(ui::CustomElementEventType event_type) {
  DCHECK(visible_);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(this,
                                                                event_type);
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedElementWebUI)

}  // namespace ui
