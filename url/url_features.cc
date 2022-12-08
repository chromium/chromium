// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_features.h"

namespace url {

BASE_FEATURE(kUseIDNA2008NonTransitional,
             "UseIDNA2008NonTransitional",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUsingIDNA2008NonTransitional() {
  return base::FeatureList::IsEnabled(kUseIDNA2008NonTransitional);
}
}  // namespace url
