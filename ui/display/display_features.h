// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_FEATURES_H_
#define UI_DISPLAY_DISPLAY_FEATURES_H_

#include "base/feature_list.h"
#include "ui/display/display_export.h"

namespace display {
namespace features {

#if defined(OS_CHROMEOS)
DISPLAY_EXPORT extern const base::Feature kUseMonitorColorSpace;
#endif

DISPLAY_EXPORT extern const base::Feature kListAllDisplayModes;

DISPLAY_EXPORT bool IsListAllDisplayModesEnabled();

}  // namespace features
}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_FEATURES_H_
