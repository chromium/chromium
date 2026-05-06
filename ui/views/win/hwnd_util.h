// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_UTIL_H_
#define UI_VIEWS_WIN_HWND_UTIL_H_

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_ui_types.h"
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
    View* view,
    const gfx::Rect& client_bounds);

// Returns the headless window bounds for the specified HWND.
VIEWS_EXPORT gfx::Rect GetHeadlessWindowBounds(HWND window);

// Shows |window|'s system menu (at a specified |point| in screen physical
// coordinates).
VIEWS_EXPORT void ShowSystemMenuAtScreenPixelLocation(HWND window,
                                                      const gfx::Point& point);

// Returns the IAccessible* for the parent HWND of a View. The returned pointer
// is valid only for the lifetime of the WindowTreeHost in which the View
// resides.
VIEWS_EXPORT gfx::NativeViewAccessible HWNDNativeViewAccessibleForView(
    const View* view);

// Returns the IAccessible* for the parent HWND of a Widget. The returned
// pointer is valid only for the lifetime of the WindowTreeHost in which the
// Widget resides.
VIEWS_EXPORT gfx::NativeViewAccessible HWNDNativeViewAccessibleForWidget(
    const Widget* widget);

// Inflates client-area size constraints by the window's frame border/insets
// so they can be applied to the HWND.
VIEWS_EXPORT void InflateClientSizeConstraintsInPixels(HWND hwnd,
                                                       gfx::Size& min,
                                                       gfx::Size& max);

}  // namespace views

#endif  // UI_VIEWS_WIN_HWND_UTIL_H_
