// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_aura_utils.h"

#include "base/notreached.h"

namespace views {

aura::client::WindowType GetAuraWindowTypeForWidgetType(
    Widget::InitParams::Type type) {
  switch (type) {
    case Widget::InitParams::TYPE_WINDOW:
      return aura::client::WINDOW_TYPE_NORMAL;
    case Widget::InitParams::TYPE_CONTROL:
      return aura::client::WINDOW_TYPE_CONTROL;
    case Widget::InitParams::TYPE_WINDOW_FRAMELESS:
    case Widget::InitParams::TYPE_POPUP:
    case Widget::InitParams::TYPE_BUBBLE:
    case Widget::InitParams::TYPE_DRAG:
      return aura::client::WINDOW_TYPE_POPUP;
    case Widget::InitParams::TYPE_MENU:
      return aura::client::WINDOW_TYPE_MENU;
    case Widget::InitParams::TYPE_TOOLTIP:
      return aura::client::WINDOW_TYPE_TOOLTIP;
    default:
      NOTREACHED() << "Unhandled widget type " << type;
  }
}

}  // namespace views
