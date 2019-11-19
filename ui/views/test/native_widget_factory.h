// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_NATIVE_WIDGET_FACTORY_H_
#define UI_VIEWS_TEST_NATIVE_WIDGET_FACTORY_H_

#include <stdint.h>

#include "ui/views/widget/widget.h"

namespace views {

class NativeWidget;

namespace test {

// Values supplied to |behavior|.
// NativeWidget implementation is not customized in anyway.
constexpr uint32_t kDefault = 0u;
// Indicates capture should be mocked out and not interact with the system.
constexpr uint32_t kStubCapture = 1 << 0;

// Creates the appropriate platform specific NativeWidget implementation.
// If |destroyed| is non-null it it set to true from the destructor of the
// NativeWidget.
NativeWidget* CreatePlatformNativeWidgetImpl(
    const Widget::InitParams& init_params,
    Widget* widget,
    uint32_t behavior,
    bool* destroyed);

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_NATIVE_WIDGET_FACTORY_H_
