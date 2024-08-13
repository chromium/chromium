// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/animating_layout_manager_test_util.h"

#include "base/run_loop.h"
#include "base/time/time.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/view.h"

namespace views::test {

AnimatingLayoutManager* GetAnimatingLayoutManager(View* view) {
  return static_cast<AnimatingLayoutManager*>(view->GetLayoutManager());
}

void WaitForAnimatingLayoutManager(AnimatingLayoutManager* layout_manager) {
  base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
  layout_manager->PostOrQueueAction(loop.QuitClosure());
  loop.Run();
}

void WaitForAnimatingLayoutManager(View* view) {
  return WaitForAnimatingLayoutManager(GetAnimatingLayoutManager(view));
}

void ReduceAnimationDuration(AnimatingLayoutManager* layout_manager) {
  static constexpr base::TimeDelta kSmallDuration = base::Milliseconds(1);
  layout_manager->SetAnimationDuration(kSmallDuration);
}

void ReduceAnimationDuration(View* view) {
  ReduceAnimationDuration(GetAnimatingLayoutManager(view));
}

}  // namespace views::test
