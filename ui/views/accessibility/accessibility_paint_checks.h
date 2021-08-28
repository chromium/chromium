// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_PAINT_CHECKS_H_
#define UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_PAINT_CHECKS_H_

namespace views {

class View;

// This runs CHECKs related to the view's state when painting. Generally, when a
// View is ready to be displayed to the user it should also be accessible.
void RunAccessibilityPaintChecks(View* view);

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_PAINT_CHECKS_H_
