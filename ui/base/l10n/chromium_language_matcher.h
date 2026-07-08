// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_CHROMIUM_LANGUAGE_MATCHER_H_
#define UI_BASE_L10N_CHROMIUM_LANGUAGE_MATCHER_H_

#include <vector>

#include "base/component_export.h"

namespace base::i18n {
class LanguageTag;
class LanguageTagMatcher;
}  // namespace base::i18n

namespace ui_l10n {

// Returns the list of LanguageTags that are accepted by Chromium.
COMPONENT_EXPORT(UI_BASE)
const std::vector<base::i18n::LanguageTag>& GetAcceptLanguageTags();

// Returns the LanguageTagMatcher initialized with the accepted language tags.
COMPONENT_EXPORT(UI_BASE)
const base::i18n::LanguageTagMatcher& GetAcceptLanguageMatcher();

}  // namespace ui_l10n

#endif  // UI_BASE_L10N_CHROMIUM_LANGUAGE_MATCHER_H_
