// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_DRAWING_TEST_UTILS_H_
#define UI_VIEWS_TEST_VIEWS_DRAWING_TEST_UTILS_H_

class SkBitmap;

namespace views {
class View;

namespace test {

// Paints the provided View and all its children to an SkBitmap, exactly like
// the view would be painted in actual use. This includes antialiasing of text,
// graphical effects, hover state, focus rings, and so on.
SkBitmap PaintViewToBitmap(View* view);

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_DRAWING_TEST_UTILS_H_
