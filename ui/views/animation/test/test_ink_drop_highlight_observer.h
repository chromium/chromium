// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HIGHLIGHT_OBSERVER_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HIGHLIGHT_OBSERVER_H_

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_highlight_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/test/test_ink_drop_animation_observer_helper.h"

namespace views {
namespace test {

// Simple InkDropHighlightObserver test double that tracks if
// InkDropHighlightObserver methods are invoked and the parameters used for the
// last invocation.
class TestInkDropHighlightObserver : public InkDropHighlightObserver,
                                     public TestInkDropAnimationObserverHelper<
                                         InkDropHighlight::AnimationType> {
 public:
  TestInkDropHighlightObserver();
  ~TestInkDropHighlightObserver() override = default;

  void set_ink_drop_highlight(InkDropHighlight* ink_drop_highlight) {
    ink_drop_highlight_ = ink_drop_highlight;
  }

  // InkDropHighlightObserver:
  void AnimationStarted(
      InkDropHighlight::AnimationType animation_type) override;
  void AnimationEnded(InkDropHighlight::AnimationType animation_type,
                      InkDropAnimationEndedReason reason) override;

 private:
  // The type this inherits from. Reduces verbosity in .cc file.
  using ObserverHelper =
      TestInkDropAnimationObserverHelper<InkDropHighlight::AnimationType>;

  // An InkDropHighlight to spy info from when notifications are handled.
  InkDropHighlight* ink_drop_highlight_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropHighlightObserver);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HIGHLIGHT_OBSERVER_H_
