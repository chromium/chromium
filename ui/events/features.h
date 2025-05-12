// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FEATURES_H_
#define UI_EVENTS_FEATURES_H_

#include "base/feature_list.h"
#include "ui/events/events_export.h"

namespace ui {

// Until recently, Chrome on most platforms relied solely on a heuristic in
// ui::KeyEvent construction to determine if they are repeat key events. Chrome
// recently shifted to reading key repeat information from the native OS key
// event on all platforms (see https://crbug.com/40940886), which theoretically
// makes this heuristic redundant.
//
// This feature flag is used to gradually turn off this heuristic and also
// serves as an emergency killswitch in case turning it off causes major
// issues. See tracking bug https://crbug.com/411681432 for more info.
EVENTS_EXPORT
BASE_DECLARE_FEATURE(kLegacyKeyRepeatSynthesis);

}  // namespace ui

#endif  // UI_EVENTS_FEATURES_H_
