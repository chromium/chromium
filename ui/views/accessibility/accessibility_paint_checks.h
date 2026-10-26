// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_PAINT_CHECKS_H_
#define UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_PAINT_CHECKS_H_

#include "ui/views/views_export.h"

namespace views {

class View;
class Widget;

// This runs DCHECKs related to the view's state when painting. Generally, when
// a View is ready to be displayed to the user it should also be accessible.
VIEWS_EXPORT void RunAccessibilityPaintChecks(View* view);

// Runs the paint checks recursively starting from the Widget's RootView.
VIEWS_EXPORT void RunAccessibilityPaintChecks(Widget* widget);

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_PAINT_CHECKS_H_
