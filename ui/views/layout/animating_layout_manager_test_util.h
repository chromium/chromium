// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_ANIMATING_LAYOUT_MANAGER_TEST_UTIL_H_
#define UI_VIEWS_LAYOUT_ANIMATING_LAYOUT_MANAGER_TEST_UTIL_H_

namespace views {

class AnimatingLayoutManager;
class View;

namespace test {

// Gets the AnimatingLayoutManager for a View. This assumes that one exists, no
// type checks are performed.
AnimatingLayoutManager* GetAnimatingLayoutManager(View* view);

// Waits for animations to finish and pending tasks to run.
void WaitForAnimatingLayoutManager(AnimatingLayoutManager* layout_manager);
void WaitForAnimatingLayoutManager(View* view);

// Sets animation times to a small but nonzero value to speed up tests.
void ReduceAnimationDuration(AnimatingLayoutManager* layout_manager);
void ReduceAnimationDuration(View* view);

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_LAYOUT_ANIMATING_LAYOUT_MANAGER_TEST_UTIL_H_
