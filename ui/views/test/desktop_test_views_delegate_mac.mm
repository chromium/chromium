// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/desktop_test_views_delegate.h"

#include "ui/views/widget/native_widget_mac.h"

namespace views {

DesktopTestViewsDelegate::DesktopTestViewsDelegate() = default;

DesktopTestViewsDelegate::~DesktopTestViewsDelegate() = default;

void DesktopTestViewsDelegate::OnBeforeWidgetInit(
    Widget::InitParams* params,
    internal::NativeWidgetDelegate* delegate) {
  // If we already have a native_widget, we don't have to try to come
  // up with one.
  if (params->native_widget)
    return;

  if (params->parent && params->type != views::Widget::InitParams::TYPE_MENU) {
    params->native_widget = new NativeWidgetMac(delegate);
  } else if (!params->parent && !params->context) {
    NOTIMPLEMENTED();
    // TODO(tapted): Implement DesktopNativeWidgetAura.
    params->native_widget = new NativeWidgetMac(delegate);
  }
}

}  // namespace views
