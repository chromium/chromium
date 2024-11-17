// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_FEATURES_H_
#define URL_URL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace url {

// If you add or remove a feature related to URLs, you may need to
// correspondingly update the EarlyAccess allow list in app shims
// (chrome/app_shim/app_shim_controller.mm). See https://crbug.com/1520386 for
// more details.

COMPONENT_EXPORT(URL) BASE_DECLARE_FEATURE(kUseIDNA2008NonTransitional);

// Returns true if Chrome is using IDNA 2008 in Non-Transitional mode.
COMPONENT_EXPORT(URL) bool IsUsingIDNA2008NonTransitional();

// Returns true if Chrome is recording IDNA 2008 related metrics.
COMPONENT_EXPORT(URL) bool IsRecordingIDNA2008Metrics();

// Returns true if kStandardCompliantNonSpecialSchemeURLParsing feature is
// enabled. See url::kStandardCompliantNonSpecialSchemeURLParsing for details.
COMPONENT_EXPORT(URL) bool IsUsingStandardCompliantNonSpecialSchemeURLParsing();

// Returns true if space characters should be treated as invalid in URL host
// parsing.
COMPONENT_EXPORT(URL) bool IsDisallowingSpaceCharacterInURLHostParsing();

// When enabled, Chrome uses standard-compliant URL parsing for non-special
// scheme URLs. See https://crbug.com/1416006 for details.
COMPONENT_EXPORT(URL)
BASE_DECLARE_FEATURE(kStandardCompliantNonSpecialSchemeURLParsing);

// When enabled, treat space characters as invalid in URL host parsing.
COMPONENT_EXPORT(URL)
BASE_DECLARE_FEATURE(kDisallowSpaceCharacterInURLHostParsing);

}  // namespace url

#endif  // URL_URL_FEATURES_H_
