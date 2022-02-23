// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/native_widget_factory.h"

#include "build/build_config.h"
#include "ui/views/test/test_platform_native_widget.h"

#if defined(USE_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"
#elif BUILDFLAG(IS_MAC)
#include "ui/views/widget/native_widget_mac.h"
#endif

namespace views {
namespace test {

NativeWidget* CreatePlatformNativeWidgetImpl(
    Widget* widget,
    uint32_t type,
    bool* destroyed) {
#if BUILDFLAG(IS_MAC)
  return new TestPlatformNativeWidget<NativeWidgetMac>(
      widget, type == kStubCapture, destroyed);
#else
  return new TestPlatformNativeWidget<NativeWidgetAura>(
      widget, type == kStubCapture, destroyed);
#endif
}

}  // namespace test
}  // namespace views
