// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_EVENTS_H_
#define UI_BASE_INTERACTION_ELEMENT_EVENTS_H_

#include "ui/base/interaction/element_tracker.h"

namespace ui {

// Event fired when a tracked element's bounds change while visible.
// Currently fired for webui anchors but in future can be used to fire for views
// anchor as well.
DECLARE_EXPORTED_CUSTOM_ELEMENT_EVENT_TYPE(
    COMPONENT_EXPORT(UI_BASE_INTERACTION),
    kElementBoundsChangedEvent);

}  // namespace ui

#endif  // UI_BASE_INTERACTION_ELEMENT_EVENTS_H_
