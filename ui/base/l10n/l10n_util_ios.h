// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_UTIL_IOS_H_
#define UI_BASE_L10N_L10N_UTIL_IOS_H_

#include <string>

#include "base/component_export.h"

namespace l10n_util {

// Get localized language name using NSLocale Foundation API. If the system
// API returns null or an empty string, ICU's formatting style of an unknown
// language will be used which is "xyz (XYZ)" where the input is parsed into
// language and script by the - token and reformatted as
// "$lowercase_language ($UPPERCASE_SCRIPT)". If the - token is not found, the
// lowercase version of |locale| will be returned.
//
// TODO(jshin): the locale id is not always ll-SSSS, but can be ll-RR,
// ll-SSSS-RR (SSSS: 4 letter script, RR: 2 letter or 3 digit region code).
// There are more complicated forms, too (per BCP 47).
//
// Consider changing this calling the ICU directly instead of trying to
// emulate it in case of a fallback. Though how not sure whether iOS API
// exposes the necessary function from ICU, and iOS is trying to not depend
// on ICU to reduce the package size.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetDisplayNameForLocale(const std::string& locale,
                                       const std::string& display_locale);

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_IOS_H_
