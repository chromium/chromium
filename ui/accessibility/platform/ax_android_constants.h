// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_ANDROID_CONSTANTS_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_ANDROID_CONSTANTS_H_

#include "base/component_export.h"

namespace ui {

// Classnames.

COMPONENT_EXPORT(AX_PLATFORM)
extern const char kAXAutoCompleteTextViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXAbsListViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXButtonClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXCheckBoxClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXCompoundButtonClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXCheckedTextViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXDialogClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXEditTextClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXGridViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM)
extern const char kAXHorizontalScrollViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXImageClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXImageButtonClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXImageViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXListViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXMenuItemClassname[];
COMPONENT_EXPORT(AX_PLATFORM)
extern const char kAXMultiAutoCompleteTextViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXPagerClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXProgressBarClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXRadioButtonClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXRadioGroupClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXScrollViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXSeekBarClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXSwitchClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXSpinnerClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXTabWidgetClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXTextViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXToggleButtonClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXViewGroupClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char kAXWebViewClassname[];
COMPONENT_EXPORT(AX_PLATFORM) extern const char16_t kSecurePasswordBullet;

// View constants.

COMPONENT_EXPORT(AX_PLATFORM) extern const int kAXAndroidInvalidViewId;

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_ANDROID_CONSTANTS_H_
