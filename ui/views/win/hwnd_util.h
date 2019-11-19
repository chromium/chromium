// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_UTIL_H_
#define UI_VIEWS_WIN_HWND_UTIL_H_

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class Widget;

// Returns the HWND for the specified View.
VIEWS_EXPORT HWND HWNDForView(const View* view);

// Returns the HWND for the specified Widget.
VIEWS_EXPORT HWND HWNDForWidget(const Widget* widget);

// Returns the HWND for the specified NativeView.
VIEWS_EXPORT HWND HWNDForNativeView(const gfx::NativeView view);

// Returns the HWND for the specified NativeWindow.
VIEWS_EXPORT HWND HWNDForNativeWindow(const gfx::NativeWindow window);

VIEWS_EXPORT gfx::Rect GetWindowBoundsForClientBounds(
    View* view, const gfx::Rect& client_bounds);

// Shows |window|'s system menu (at a specified |point| in screen physical
// coordinates).
VIEWS_EXPORT void ShowSystemMenuAtScreenPixelLocation(HWND window,
                                                      const gfx::Point& point);

}  // namespace views

#endif  // UI_VIEWS_WIN_HWND_UTIL_H_
