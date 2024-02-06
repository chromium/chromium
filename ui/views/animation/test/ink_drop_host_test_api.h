// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_HOST_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_HOST_TEST_API_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"

namespace views::test {

// Test API to provide internal access to an InkDropHost instance.
class InkDropHostTestApi {
 public:
  // Make the protected enum accessible.
  using InkDropMode = views::InkDropHost::InkDropMode;

  explicit InkDropHostTestApi(InkDropHost* ink_drop_host);

  InkDropHostTestApi(const InkDropHostTestApi&) = delete;
  InkDropHostTestApi& operator=(const InkDropHostTestApi&) = delete;

  ~InkDropHostTestApi();

  void SetInkDropMode(InkDropMode ink_drop_mode);

  void SetInkDrop(std::unique_ptr<InkDrop> ink_drop,
                  bool handles_gesture_events);
  void SetInkDrop(std::unique_ptr<InkDrop> ink_drop);

  InkDrop* ink_drop() { return ink_drop_host_->ink_drop_.get(); }

  // Wrapper for InkDropHost::HasInkDrop().
  bool HasInkDrop() const;

  // Wrapper for InkDropHost::GetInkDrop() which lazily creates the ink drop
  // instance if it doesn't already exist. If you need direct access to
  // InkDropHost::ink_drop_ use ink_drop() instead.
  InkDrop* GetInkDrop();

  bool HasInkdropEventHandler() const;

  // Wrapper for InkDropHost::AnimateToState().
  void AnimateToState(InkDropState state, const ui::LocatedEvent* event);

  InkDropMode ink_drop_mode() const { return ink_drop_host_->ink_drop_mode_; }

  void RemoveInkDropMask();

 private:
  // The InkDropHost to provide internal access to.
  const raw_ref<InkDropHost> ink_drop_host_;
};

}  // namespace views::test

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_HOST_TEST_API_H_
