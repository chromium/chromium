// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_FEATURES_H_
#define URL_URL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace url {

COMPONENT_EXPORT(URL) BASE_DECLARE_FEATURE(kUseIDNA2008NonTransitional);

// Returns true if Chrome is using IDNA 2008 in Non-Transitional mode.
COMPONENT_EXPORT(URL) bool IsUsingIDNA2008NonTransitional();

// Returns true if Chrome is recording IDNA 2008 related metrics.
COMPONENT_EXPORT(URL) bool IsRecordingIDNA2008Metrics();

}  // namespace url

#endif  // URL_URL_FEATURES_H_
