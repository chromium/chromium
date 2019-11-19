// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_

#include "base/macros.h"
#include "ui/views/animation/ink_drop_host_view.h"

namespace views {

// A non-functional implementation of an InkDropHost that can be used during
// tests.  Tracks the number of hosted ink drop layers.
//
// Note that CreateInkDrop() is not supported.
class TestInkDropHost : public InkDropHostView {
 public:
  TestInkDropHost();
  ~TestInkDropHost() override;

  int num_ink_drop_layers_added() const { return num_ink_drop_layers_added_; }
  int num_ink_drop_layers() const {
    return num_ink_drop_layers_added_ - num_ink_drop_layers_removed_;
  }

  int num_ink_drop_ripples_created() const {
    return num_ink_drop_ripples_created_;
  }
  int num_ink_drop_highlights_created() const {
    return num_ink_drop_highlights_created_;
  }

  const InkDropRipple* last_ink_drop_ripple() const {
    return last_ink_drop_ripple_;
  }
  const InkDropHighlight* last_ink_drop_highlight() const {
    return last_ink_drop_highlight_;
  }

  void set_disable_timers_for_test(bool disable_timers_for_test) {
    disable_timers_for_test_ = disable_timers_for_test;
  }

  // InkDropHostView:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<InkDropHighlight> CreateInkDropHighlight() const override;

 private:
  int num_ink_drop_layers_added_ = 0;
  int num_ink_drop_layers_removed_ = 0;

  // CreateInkDrop{Ripple,Highlight} are const, so these members must be
  // mutable.
  mutable int num_ink_drop_ripples_created_ = 0;
  mutable int num_ink_drop_highlights_created_ = 0;

  mutable const InkDropRipple* last_ink_drop_ripple_ = nullptr;
  mutable const InkDropHighlight* last_ink_drop_highlight_ = nullptr;

  // When true, the InkDropRipple/InkDropHighlight instances will have their
  // timers disabled after creation.
  bool disable_timers_for_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropHost);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_
