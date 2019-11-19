// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop_highlight_observer.h"

#include "ui/views/animation/ink_drop_highlight.h"

namespace views {
namespace test {

TestInkDropHighlightObserver::TestInkDropHighlightObserver() = default;

void TestInkDropHighlightObserver::AnimationStarted(
    InkDropHighlight::AnimationType animation_type) {
  ObserverHelper::OnAnimationStarted(animation_type);
}

void TestInkDropHighlightObserver::AnimationEnded(
    InkDropHighlight::AnimationType animation_type,
    InkDropAnimationEndedReason reason) {
  ObserverHelper::OnAnimationEnded(animation_type, reason);
}

}  // namespace test
}  // namespace views
