// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/base.

#ifndef UI_BASE_UI_BASE_SWITCHES_H_
#define UI_BASE_UI_BASE_SWITCHES_H_

#include "build/build_config.h"
#include "ui/base/ui_base_export.h"

namespace switches {

#if defined(OS_MACOSX) && !defined(OS_IOS)
UI_BASE_EXPORT extern const char kDisableAVFoundationOverlays[];
UI_BASE_EXPORT extern const char kDisableMacOverlays[];
UI_BASE_EXPORT extern const char kDisableModalAnimations[];
UI_BASE_EXPORT extern const char kDisableRemoteCoreAnimation[];
UI_BASE_EXPORT extern const char kShowMacOverlayBorders[];
#endif

UI_BASE_EXPORT extern const char kAnimationDurationScale[];
UI_BASE_EXPORT extern const char kDisableCompositedAntialiasing[];
UI_BASE_EXPORT extern const char kDisableDwmComposition[];
UI_BASE_EXPORT extern const char kDisableTouchAdjustment[];
UI_BASE_EXPORT extern const char kDisableTouchDragDrop[];
UI_BASE_EXPORT extern const char kEnableTouchDragDrop[];
UI_BASE_EXPORT extern const char kForceCaptionStyle[];
UI_BASE_EXPORT extern const char kForceDarkMode[];
UI_BASE_EXPORT extern const char kForceHighContrast[];
UI_BASE_EXPORT extern const char kLang[];
UI_BASE_EXPORT extern const char kShowOverdrawFeedback[];
UI_BASE_EXPORT extern const char kSlowDownCompositingScaleFactor[];
UI_BASE_EXPORT extern const char kTintGlCompositedContent[];
UI_BASE_EXPORT extern const char kTopChromeTouchUi[];
UI_BASE_EXPORT extern const char kTopChromeTouchUiAuto[];
UI_BASE_EXPORT extern const char kTopChromeTouchUiDisabled[];
UI_BASE_EXPORT extern const char kTopChromeTouchUiEnabled[];
UI_BASE_EXPORT extern const char kUIDisablePartialSwap[];
UI_BASE_EXPORT extern const char kUseSystemClipboard[];

// Test related.
UI_BASE_EXPORT extern const char kDisallowNonExactResourceReuse[];
UI_BASE_EXPORT extern const char kMangleLocalizedStrings[];

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_H_
