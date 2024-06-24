// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/base.

#ifndef UI_BASE_UI_BASE_SWITCHES_H_
#define UI_BASE_UI_BASE_SWITCHES_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace switches {

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(UI_BASE) extern const char kDisableOverscrollEdgeEffect[];
COMPONENT_EXPORT(UI_BASE) extern const char kDisablePullToRefreshEffect[];
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(UI_BASE) extern const char kDisableModalAnimations[];
COMPONENT_EXPORT(UI_BASE) extern const char kShowMacOverlayBorders[];
#endif

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE) extern const char kEnableResourcesFileSharing[];
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE) extern const char kSystemFontFamily[];
#endif

#if BUILDFLAG(IS_LINUX)
COMPONENT_EXPORT(UI_BASE) extern const char kUiToolkitFlag[];
COMPONENT_EXPORT(UI_BASE) extern const char kDisableGtkIme[];
#endif

COMPONENT_EXPORT(UI_BASE) extern const char kDisableCompositedAntialiasing[];
COMPONENT_EXPORT(UI_BASE) extern const char kDisableTouchDragDrop[];
COMPONENT_EXPORT(UI_BASE) extern const char kDRMVirtualConnectorIsExternal[];
COMPONENT_EXPORT(UI_BASE) extern const char kEnableTouchDragDrop[];
COMPONENT_EXPORT(UI_BASE) extern const char kForceCaptionStyle[];
COMPONENT_EXPORT(UI_BASE) extern const char kForceDarkMode[];
COMPONENT_EXPORT(UI_BASE) extern const char kForceHighContrast[];
COMPONENT_EXPORT(UI_BASE) extern const char kLang[];
COMPONENT_EXPORT(UI_BASE) extern const char kShowOverdrawFeedback[];
COMPONENT_EXPORT(UI_BASE) extern const char kSlowDownCompositingScaleFactor[];
COMPONENT_EXPORT(UI_BASE) extern const char kTintCompositedContent[];
COMPONENT_EXPORT(UI_BASE) extern const char kTopChromeTouchUi[];
COMPONENT_EXPORT(UI_BASE) extern const char kTopChromeTouchUiAuto[];
COMPONENT_EXPORT(UI_BASE) extern const char kTopChromeTouchUiDisabled[];
COMPONENT_EXPORT(UI_BASE) extern const char kTopChromeTouchUiEnabled[];
COMPONENT_EXPORT(UI_BASE) extern const char kUIDisablePartialSwap[];
COMPONENT_EXPORT(UI_BASE) extern const char kUseSystemClipboard[];

// Test related.
COMPONENT_EXPORT(UI_BASE) extern const char kDisallowNonExactResourceReuse[];
COMPONENT_EXPORT(UI_BASE) extern const char kMangleLocalizedStrings[];

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_H_
