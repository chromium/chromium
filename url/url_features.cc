// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_features.h"
#include "base/feature_list.h"

namespace url {

BASE_FEATURE(kDisallowSpaceCharacterInURLHostParsing,
             "DisallowSpaceCharacterInURLHostParsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDisallowingSpaceCharacterInURLHostParsing() {
  // If the FeatureList isn't available yet, fall back to the feature's default
  // state. This may happen during early startup, see crbug.com/1441956.
  if (!base::FeatureList::GetInstance()) {
    return kDisallowSpaceCharacterInURLHostParsing.default_state ==
           base::FEATURE_DISABLED_BY_DEFAULT;
  }
  return base::FeatureList::IsEnabled(kDisallowSpaceCharacterInURLHostParsing);
}

}  // namespace url
