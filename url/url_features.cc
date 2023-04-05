// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_features.h"

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for crbug.com/1220361.
BASE_FEATURE(kResolveBareFragmentWithColonOnNonHierarchical,
             "ResolveBareFragmentWithColonOnNonHierarchical",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUsingIDNA2008NonTransitional() {
  return base::FeatureList::IsEnabled(kUseIDNA2008NonTransitional);
}

bool IsRecordingIDNA2008Metrics() {
  return base::FeatureList::IsEnabled(kRecordIDNA2008Metrics);
}

}  // namespace url
