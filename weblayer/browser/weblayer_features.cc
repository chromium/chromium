// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_features.h"

namespace weblayer {

#if defined(OS_ANDROID)
// Used to disable browser-control animations.
const base::Feature kImmediatelyHideBrowserControlsForTest{
    "ImmediatelyHideBrowserControlsForTest", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace weblayer
