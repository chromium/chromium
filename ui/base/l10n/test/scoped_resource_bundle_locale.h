// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_TEST_SCOPED_RESOURCE_BUNDLE_LOCALE_H_
#define UI_BASE_L10N_TEST_SCOPED_RESOURCE_BUNDLE_LOCALE_H_

#include <string>

#include "base/i18n/language_tag.h"

namespace ui::test {

// Test helper that reloads the `ui::ResourceBundle` localized resources (e.g.
// strings returned by `l10n_util::GetStringUTF16()`) for a locale during its
// lifetime, and restores the previously loaded locale resources upon
// destruction. It does not change the ICU default locale; use
// `ScopedApplicationLocale` to change both.
//
// If the resources of the locale are not bundled with the test binary, the
// previously loaded locale resources are kept.
//
// TODO(crbug.com/582341363): Pass the locale to `ui::ResourceBundle` as a
// `base::i18n::LanguageTag` once `ui::ResourceBundle` receives
// `base::i18n::LanguageTag`.
class ScopedResourceBundleLocale {
 public:
  explicit ScopedResourceBundleLocale(const base::i18n::LanguageTag& locale);
  ~ScopedResourceBundleLocale();

  ScopedResourceBundleLocale(const ScopedResourceBundleLocale&) = delete;
  ScopedResourceBundleLocale& operator=(const ScopedResourceBundleLocale&) =
      delete;

 private:
  // The locale of the resources loaded before construction, to restore upon
  // destruction. Empty if there is nothing to restore.
  std::string prev_locale_;
};

}  // namespace ui::test

#endif  // UI_BASE_L10N_TEST_SCOPED_RESOURCE_BUNDLE_LOCALE_H_
