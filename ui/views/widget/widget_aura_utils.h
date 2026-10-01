// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_AURA_UTILS_H_
#define UI_VIEWS_WIDGET_WIDGET_AURA_UTILS_H_

#include "ui/aura/client/window_types.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"

// Functions shared by native_widget_aura.cc and desktop_native_widget_aura.cc:

namespace views {

aura::client::WindowType GetAuraWindowTypeForWidgetType(
    Widget::InitParams::Type type);

// Returns whether drag and drop should be enabled for a widget with `params`.
VIEWS_EXPORT bool ShouldEnableDragDrop(const Widget::InitParams& params);

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_AURA_UTILS_H_
