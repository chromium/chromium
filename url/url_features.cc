// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_features.h"
#include "base/feature_list.h"

namespace url {

BASE_FEATURE(kUseIDNA2008NonTransitional,
             "UseIDNA2008NonTransitional",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for crbug.com/1362507.
BASE_FEATURE(kRecordIDNA2008Metrics,
             "RecordIDNA2008Metrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStrictIPv4EmbeddedIPv6AddressParsing,
             "StrictIPv4EmbeddedIPv6AddressParsing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for crbug.com/1220361.
BASE_FEATURE(kResolveBareFragmentWithColonOnNonHierarchical,
             "ResolveBareFragmentWithColonOnNonHierarchical",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for crbug.com/1252531.
BASE_FEATURE(kDontDecodeAsciiPercentEncodedURLPath,
             "DontDecodeAsciiPercentEncodedURLPath",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUsingIDNA2008NonTransitional() {
  // If the FeatureList isn't available yet, fall back to the feature's default
  // state. This may happen during early startup, see crbug.com/1441956.
  if (!base::FeatureList::GetInstance()) {
    return kUseIDNA2008NonTransitional.default_state ==
           base::FEATURE_ENABLED_BY_DEFAULT;
  }

  return base::FeatureList::IsEnabled(kUseIDNA2008NonTransitional);
}

bool IsUsingDontDecodeAsciiPercentEncodedURLPath() {
  // If the FeatureList isn't available yet, fall back to the feature's default
  // state. This may happen during early startup, see https://crbug.com/1478960.
  if (!base::FeatureList::GetInstance()) {
    return kDontDecodeAsciiPercentEncodedURLPath.default_state ==
           base::FEATURE_ENABLED_BY_DEFAULT;
  }

  return base::FeatureList::IsEnabled(kDontDecodeAsciiPercentEncodedURLPath);
}

bool IsRecordingIDNA2008Metrics() {
  return base::FeatureList::IsEnabled(kRecordIDNA2008Metrics);
}

}  // namespace url
