// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/features.h"

namespace ui {

// Overlay delegation is required for delegated compositing. This doesn't enable
// delegated compositing, but rather allows usage of subsurfaces for delegation.
BASE_FEATURE(kWaylandOverlayDelegation,
             "WaylandOverlayDelegation",
             base::FEATURE_DISABLED_BY_DEFAULT
);

// Controls whether support for the fractional_scale_v1 protocol should be
// enabled.
BASE_FEATURE(kWaylandFractionalScaleV1,
             "WaylandFractionalScaleV1",
             base::FEATURE_ENABLED_BY_DEFAULT
);

// Controls whether support for the xdg-toplevel-drag protocol should be
// enabled. On Lacros it will then be used even if the Exo-only extended-drag
// protocol is supported.
BASE_FEATURE(kWaylandXdgToplevelDrag,
             "WaylandXdgToplevelDrag",
             base::FEATURE_ENABLED_BY_DEFAULT
);

// This debug/dev flag pretty-prints DRM modeset configuration logs for ease
// of reading. For more information, see: http://crbug.com/233006802
BASE_FEATURE(kPrettyPrintDrmModesetConfigLogs,
             "PrettyPrintDrmModesetConfigLogs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Chromium will try to use the smallest valid size for the cursor
// plane that fits the cursor bitmap.
BASE_FEATURE(kUseDynamicCursorSize,
             "UseDynamicCursorSize",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/40235357): Remove this and dispatch all ui events on
// corresponding `wl_pointer.frame` calls when when linux compositors comply
// with the protocol.
BASE_FEATURE(kDispatchPointerEventsOnFrameEvent,
             "DispatchPointerEventsOnFrameEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/40235357): Remove this and dispatch all ui events on
// corresponding `wl_touch.frame` calls when when linux compositors comply with
// the protocol. For instance, on Gnome/Wayland, KDE and Weston compositors a
// wl_touch.up does not come accompanied by a respective wl_touch.frame event.
// On these scenarios be conservative and always dispatch the events
// immediately.
BASE_FEATURE(kDispatchTouchEventsOnFrameEvent,
             "DispatchTouchEventsOnFrameEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsWaylandOverlayDelegationEnabled() {
  return base::FeatureList::IsEnabled(kWaylandOverlayDelegation);
}

bool IsWaylandFractionalScaleV1Enabled() {
  return base::FeatureList::IsEnabled(kWaylandFractionalScaleV1);
}

bool IsWaylandXdgToplevelDragEnabled() {
  return base::FeatureList::IsEnabled(kWaylandXdgToplevelDrag);
}

bool IsPrettyPrintDrmModesetConfigLogsEnabled() {
  return base::FeatureList::IsEnabled(kPrettyPrintDrmModesetConfigLogs);
}

bool IsUseDynamicCursorSizeEnabled() {
  return base::FeatureList::IsEnabled(kUseDynamicCursorSize);
}

bool IsDispatchPointerEventsOnFrameEventEnabled() {
  return base::FeatureList::IsEnabled(kDispatchPointerEventsOnFrameEvent);
}

bool IsDispatchTouchEventsOnFrameEventEnabled() {
  return base::FeatureList::IsEnabled(kDispatchTouchEventsOnFrameEvent);
}

}  // namespace ui
