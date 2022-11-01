// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_NATIVE_WIDGET_FACTORY_H_
#define UI_VIEWS_TEST_NATIVE_WIDGET_FACTORY_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"
#include "ui/views/widget/widget.h"

namespace views {

class NativeWidget;

namespace test {

// Values supplied to |behavior|.
// NativeWidget implementation is not customized in anyway.
constexpr uint32_t kDefault = 0u;
// Indicates capture should be mocked out and not interact with the system.
constexpr uint32_t kStubCapture = 1 << 0;

// Creates the appropriate platform specific non-desktop NativeWidget
// implementation. If |destroyed| is non-null it it set to true from the
// destructor of the NativeWidget.
NativeWidget* CreatePlatformNativeWidgetImpl(
    Widget* widget,
    uint32_t behavior,
    bool* destroyed);

// Creates the appropriate platform specific non-desktop NativeWidget
// implementation. Creates the appropriate platform specific desktop
// NativeWidget implementation. `destroyed_callback` is called from the
// destructor of the NativeWidget.
NativeWidget* CreatePlatformNativeWidgetImpl(
    Widget* widget,
    uint32_t behavior,
    base::OnceClosure destroyed_callback);

// Creates the appropriate platform specific desktop NativeWidget
// implementation. `destroyed_callback` is called from the destructor of the
// NativeWidget.
NativeWidget* CreatePlatformDesktopNativeWidgetImpl(
    Widget* widget,
    uint32_t behavior,
    base::OnceClosure destroyed_callback);

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_NATIVE_WIDGET_FACTORY_H_
