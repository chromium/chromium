// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_FEATURES_H_
#define UI_OZONE_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace ui {

extern const base::Feature kWaylandOverlayDelegation;
bool IsWaylandOverlayDelegationEnabled();

}  // namespace ui

#endif  // UI_OZONE_COMMON_FEATURES_H_
