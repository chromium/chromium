// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_UTIL_ANDROID_H_
#define UI_BASE_L10N_L10N_UTIL_ANDROID_H_

#include <jni.h>

#include <string>

#include "base/component_export.h"

namespace l10n_util {

COMPONENT_EXPORT(UI_BASE)
std::u16string GetDisplayNameForLocale(const std::string& locale,
                                       const std::string& display_locale);

COMPONENT_EXPORT(UI_BASE) bool IsLayoutRtl();
COMPONENT_EXPORT(UI_BASE) bool ShouldMirrorBackForwardGestures();
COMPONENT_EXPORT(UI_BASE) void SetRtlForTesting(bool is_rtl);

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_ANDROID_H_
