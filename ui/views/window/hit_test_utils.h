// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_HIT_TEST_UTILS_H_
#define UI_VIEWS_WINDOW_HIT_TEST_UTILS_H_

#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace views {

class View;

// Returns the inner most non-HTNOWHERE |kHitTestComponentKey| value at
// |point_in_widget| within the hierarchy of |view|, otherwise returns
// HTNOWHERE.
VIEWS_EXPORT int GetHitTestComponent(View* view,
                                     const gfx::Point& point_in_widget);

// Sets the |kHitTestComponentKey| property of |view|.
VIEWS_EXPORT void SetHitTestComponent(View* view, int hit_test_id);

}  // namespace views

#endif  // UI_VIEWS_WINDOW_HIT_TEST_UTILS_H_
