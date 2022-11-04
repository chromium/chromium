// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/native_widget_factory.h"

#include <utility>

#include "build/build_config.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/test_platform_native_widget.h"

#if defined(USE_AURA)
#include "ui/views/widget/native_widget_aura.h"
#elif BUILDFLAG(IS_MAC)
#include "ui/views/widget/native_widget_mac.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

namespace views::test {

#if BUILDFLAG(IS_MAC)
using PlatformNativeWidget = NativeWidgetMac;
using PlatformDesktopNativeWidget = NativeWidgetMac;
#else
using PlatformNativeWidget = NativeWidgetAura;
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
using PlatformDesktopNativeWidget = DesktopNativeWidgetAura;
#endif
#endif

NativeWidget* CreatePlatformNativeWidgetImpl(
    Widget* widget,
    uint32_t type,
    bool* destroyed) {
  return new TestPlatformNativeWidget<PlatformNativeWidget>(
      widget, type == kStubCapture, destroyed);
}

NativeWidget* CreatePlatformNativeWidgetImpl(
    Widget* widget,
    uint32_t type,
    base::OnceClosure destroyed_callback) {
  return new TestPlatformNativeWidget<PlatformNativeWidget>(
      widget, type == kStubCapture, std::move(destroyed_callback));
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
NativeWidget* CreatePlatformDesktopNativeWidgetImpl(
    Widget* widget,
    uint32_t type,
    base::OnceClosure destroyed_callback) {
  return new TestPlatformNativeWidget<PlatformDesktopNativeWidget>(
      widget, type == kStubCapture, std::move(destroyed_callback));
}
#endif

}  // namespace views::test
