// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/common/features.h"

namespace weblayer {
namespace features {

// Weblayer features in alphabetical order.

// Covers all media router features, i.e. Presentation API, Remote Playback API,
// and Media Fling (automatic casting of html5 videos).
const base::Feature kMediaRouter{"WebLayerMediaRouter",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// Safebrowsing support for weblayer.
const base::Feature kWebLayerSafeBrowsing{"WebLayerSafeBrowsing",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace weblayer
