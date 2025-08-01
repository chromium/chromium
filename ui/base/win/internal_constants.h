// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_INTERNAL_CONSTANTS_H_
#define UI_BASE_WIN_INTERNAL_CONSTANTS_H_

#include "base/component_export.h"

namespace ui {

// This window property if set on the window does not activate the window for a
// touch based WM_MOUSEACTIVATE message.
COMPONENT_EXPORT(UI_BASE)
extern const wchar_t kIgnoreTouchMouseActivateForWindow[];

// This class name is assigned to legacy windows created for screen readers that
// expect each web content container to be in its own HWNDs.
COMPONENT_EXPORT(UI_BASE) extern const wchar_t kLegacyRenderWidgetHostHwnd[];

// This property is put on an HWND so the compositor output knows to treat it
// as transparent and draw to it using WS_EX_LAYERED (if using the software
// compositor).
COMPONENT_EXPORT(UI_BASE) extern const wchar_t kWindowTranslucent[];

}  // namespace ui

#endif  // UI_BASE_WIN_INTERNAL_CONSTANTS_H_


