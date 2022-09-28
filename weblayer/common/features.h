// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_COMMON_FEATURES_H_
#define WEBLAYER_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace weblayer {
namespace features {

// Weblayer features in alphabetical order.

BASE_DECLARE_FEATURE(kWebLayerClientSidePhishingDetection);
BASE_DECLARE_FEATURE(kWebLayerSafeBrowsing);

}  // namespace features
}  // namespace weblayer

#endif  // WEBLAYER_COMMON_FEATURES_H_
