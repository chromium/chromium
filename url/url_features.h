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

}  // namespace url

#endif  // URL_URL_FEATURES_H_
