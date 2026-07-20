// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_CHROMIUM_LANGUAGE_MATCHER_H_
#define UI_BASE_L10N_CHROMIUM_LANGUAGE_MATCHER_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"

namespace ui_l10n {

// Returns the list of LanguageTags that are accepted by Chromium.
COMPONENT_EXPORT(UI_BASE)
const std::vector<base::i18n::LanguageTag>& GetAcceptLanguageTags();

// Returns the LanguageTagMatcher initialized with the accepted language tags.
COMPONENT_EXPORT(UI_BASE)
const base::i18n::LanguageTagMatcherWithDefault& GetAcceptLanguageMatcher();

// The list of locales that are expected on the current platform, generated from
// the `locales` variable in GN (defined in build/config/locales.gni). This is
// equivalently the list of locales that we expect to have translation strings
// for on the current platform. Guaranteed to be in sorted order and guaranteed
// to have no duplicates.
//
// Note that this could have false positives at runtime on Android and iOS:
// - On Android, locale files are dynamically shipped in app bundles which are
//   only downloaded when needed - so the |locales| variable does not accurately
//   reflect the UI strings that are currently available on disk.
//   See the comment at the top of `LoadLocaleResources` in
//   ui/base/resource/resource_bundle_android.cc for more information.
// - On iOS, some locales aren't shipped (`ios_unsupported_locales`) as they are
//   not supported by the operating system. These locales are included in this
//   variable.
//
// To avoid false positives on these platforms, use
// ui::ResourceBundle::LocaleDataPakExists() to check whether the locales exist
// on disk instead (requires I/O).
COMPONENT_EXPORT(UI_BASE)
base::span<const base::i18n::LanguageTag> GetPlatformLanguageTags();

// Returns the LanguageTagMatcher initialized with the platform language tags.
COMPONENT_EXPORT(UI_BASE)
const base::i18n::LanguageTagMatcherWithDefault& GetPlatformLanguageMatcher();

}  // namespace ui_l10n

#endif  // UI_BASE_L10N_CHROMIUM_LANGUAGE_MATCHER_H_
