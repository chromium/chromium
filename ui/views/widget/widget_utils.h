// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_UTILS_H_
#define UI_VIEWS_WIDGET_WIDGET_UTILS_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace views {
class Widget;

// Returns the root window for |widget|.  On non-Aura, this is equivalent to
// widget->GetNativeWindow().
VIEWS_EXPORT gfx::NativeWindow GetRootWindow(const Widget* widget);

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_UTILS_H_
