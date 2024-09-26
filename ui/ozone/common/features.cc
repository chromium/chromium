// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/features.h"

#include "build/chromeos_buildflags.h"

namespace ui {

// Overlay delegation is required for delegated compositing.
BASE_FEATURE(kWaylandOverlayDelegation,
             "WaylandOverlayDelegation",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// This feature flag enables a mode where the wayland client would submit
// buffers at a scale of 1 and the server applies the respective scale transform
// to properly composite the buffers. This mode is used to support fractional
// scale factor.
BASE_FEATURE(kWaylandSurfaceSubmissionInPixelCoordinates,
             "WaylandSurfaceSubmissionInPixelCoordinates",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Controls whether support for the fractional_scale_v1 protocol should be
// enabled.
BASE_FEATURE(kWaylandFractionalScaleV1,
             "WaylandFractionalScaleV1",
#if BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Controls whether support for the xdg-toplevel-drag protocol should be
// enabled. On Lacros it will then be used even if the Exo-only extended-drag
// protocol is supported.
BASE_FEATURE(kWaylandXdgToplevelDrag,
             "WaylandXdgToplevelDrag",
#if BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// This debug/dev flag pretty-prints DRM modeset configuration logs for ease
// of reading. For more information, see: http://b/233006802
BASE_FEATURE(kPrettyPrintDrmModesetConfigLogs,
             "PrettyPrintDrmModesetConfigLogs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Chromium will try to use the smallest valid size for the cursor
// plane that fits the cursor bitmap.
BASE_FEATURE(kUseDynamicCursorSize,
             "UseDynamicCursorSize",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsWaylandSurfaceSubmissionInPixelCoordinatesEnabled() {
  return base::FeatureList::IsEnabled(
      kWaylandSurfaceSubmissionInPixelCoordinates);
}

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

}  // namespace ui
