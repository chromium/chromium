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

// Returns true if IsUsingStandardCompliantHostCharacters feature is enabled.
// See url::kStandardCompliantHostCharacters for details.
COMPONENT_EXPORT(URL) bool IsUsingStandardCompliantHostCharacters();

// Returns true if kStandardCompliantNonSpecialSchemeURLParsing feature is
// enabled. See url::kStandardCompliantNonSpecialSchemeURLParsing for details.
COMPONENT_EXPORT(URL) bool IsUsingStandardCompliantNonSpecialSchemeURLParsing();

// When enabled, allows resolving of a bare fragment containing a colon against
// a non-hierarchical URL. (For example '#foo:bar' against 'about:blank'.)
COMPONENT_EXPORT(URL)
BASE_DECLARE_FEATURE(kResolveBareFragmentWithColonOnNonHierarchical);

// When enabled, Chrome uses URL Standard compliant mode to
// handle punctuation characters in URL host part.
// https://crbug.com/1416013 for details.
COMPONENT_EXPORT(URL)
BASE_DECLARE_FEATURE(kStandardCompliantHostCharacters);

// When enabled, Chrome uses standard-compliant URL parsing for non-special
// scheme URLs. See https://crbug.com/1416006 for details.
COMPONENT_EXPORT(URL)
BASE_DECLARE_FEATURE(kStandardCompliantNonSpecialSchemeURLParsing);

}  // namespace url

#endif  // URL_URL_FEATURES_H_
