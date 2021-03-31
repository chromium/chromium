// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_HOST_VIEW_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_HOST_VIEW_TEST_API_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"

namespace views {
namespace test {

// Test API to provide internal access to an InkDropHostView instance.
class InkDropHostViewTestApi {
 public:
  // Make the protected enum accessbile.
  using InkDropMode = InkDropHostView::InkDropMode;

  explicit InkDropHostViewTestApi(InkDropHostView* host_view);
  ~InkDropHostViewTestApi();

  void SetInkDropMode(InkDropMode ink_drop_mode);

  void SetInkDrop(std::unique_ptr<InkDrop> ink_drop,
                  bool handles_gesture_events);
  void SetInkDrop(std::unique_ptr<InkDrop> ink_drop);

  InkDrop* ink_drop() { return host_view_->ink_drop_.get(); }

  // Wrapper for InkDropHostView::HasInkDrop().
  bool HasInkDrop() const;

  // Wrapper for InkDropHostView::GetInkDrop() which lazily creates the ink drop
  // instance if it doesn't already exist. If you need direct access to
  // InkDropHostView::ink_drop_ use ink_drop() instead.
  InkDrop* GetInkDrop();

  bool HasInkdropEventHandler() const;

  // Wrapper for InkDropHostView::GetInkDropCenterBasedOnLastEvent().
  gfx::Point GetInkDropCenterBasedOnLastEvent() const;

  // Wrapper for InkDropHostView::AnimateInkDrop().
  void AnimateInkDrop(InkDropState state, const ui::LocatedEvent* event);

  InkDropMode ink_drop_mode() const { return host_view_->ink_drop_mode_; }

 private:
  // The InkDropHostView to provide internal access to.
  InkDropHostView* host_view_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHostViewTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_HOST_VIEW_TEST_API_H_
