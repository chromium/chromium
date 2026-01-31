// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_FEATURES_H_
#define URL_URL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace url {

// If you add or remove a feature related to URLs, you may need to
// correspondingly update the EarlyAccess allow list in app shims/
// (chrome/app_shim/app_shim_controller.mm). See https://crbug.com/1520386 for
// more details.

// Returns true if space characters should be treated as invalid in URL host
// parsing.
COMPONENT_EXPORT(URL) bool IsDisallowingSpaceCharacterInURLHostParsing();

// When enabled, treat space characters as invalid in URL host parsing.
COMPONENT_EXPORT(URL)
BASE_DECLARE_FEATURE(kDisallowSpaceCharacterInURLHostParsing);

// Returns true if IDNA ContextJ rules are applied in URL host parsing.
COMPONENT_EXPORT(URL) bool IsUsingIDNAContextJRules();

// When enabled, apply IDNA ContextJ rules in URL host parsing.
COMPONENT_EXPORT(URL) BASE_DECLARE_FEATURE(kUseIDNAContextJRules);

// Returns true if non-special URLs should handle leading slashes according
// to the URL Standard (backslash is NOT a path separator for non-special URLs).
COMPONENT_EXPORT(URL) bool IsNonSpecialLeadingSlashHandlingEnabled();

// When enabled, handle leading slashes in non-special URL paths according
// to the WHATWG URL Standard.
COMPONENT_EXPORT(URL) BASE_DECLARE_FEATURE(kNonSpecialLeadingSlashHandling);

// Returns true if %2E should be preserved in URL paths instead of being
// decoded to a literal dot.
COMPONENT_EXPORT(URL) bool IsPreservingPercentEncodedDotInPath();

// When enabled, preserve %2E encoding in URL paths to comply with the
// WHATWG URL Standard.
COMPONENT_EXPORT(URL) BASE_DECLARE_FEATURE(kPreservePercentEncodedDotInPath);

}  // namespace url

#endif  // URL_URL_FEATURES_H_
