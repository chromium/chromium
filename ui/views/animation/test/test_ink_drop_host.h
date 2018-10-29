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

  void set_disable_timers_for_test(bool disable_timers_for_test) {
    disable_timers_for_test_ = disable_timers_for_test;
  }

  // InkDropHostView:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<InkDropHighlight> CreateInkDropHighlight() const override;

 private:
  int num_ink_drop_layers_added_;
  int num_ink_drop_layers_removed_;

  // When true, the InkDropRipple/InkDropHighlight instances will have their
  // timers disabled after creation.
  bool disable_timers_for_test_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropHost);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_
