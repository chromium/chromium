// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_FEATURES_H_
#define WEBLAYER_BROWSER_WEBLAYER_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace weblayer {

#if defined(OS_ANDROID)
extern const base::Feature kImmediatelyHideBrowserControlsForTest;
#endif

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_FEATURES_H_
