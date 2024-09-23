// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_FEATURES_H_
#define UI_OZONE_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace ui {

BASE_DECLARE_FEATURE(kWaylandSurfaceSubmissionInPixelCoordinates);
BASE_DECLARE_FEATURE(kWaylandOverlayDelegation);
BASE_DECLARE_FEATURE(kWaylandFractionalScaleV1);
BASE_DECLARE_FEATURE(kPrettyPrintDrmModesetConfigLogs);
BASE_DECLARE_FEATURE(kUseDynamicCursorSize);

bool IsWaylandSurfaceSubmissionInPixelCoordinatesEnabled();
bool IsWaylandOverlayDelegationEnabled();
bool IsWaylandFractionalScaleV1Enabled();
bool IsWaylandXdgToplevelDragEnabled();
bool IsPrettyPrintDrmModesetConfigLogsEnabled();
bool IsUseDynamicCursorSizeEnabled();

}  // namespace ui

#endif  // UI_OZONE_COMMON_FEATURES_H_
