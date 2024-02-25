// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"

namespace views {

// A non-functional implementation of an View with an ink drop that can be used
// during tests.  Tracks the number of hosted ink drop layers.
class TestInkDropHost : public View {
 public:
  explicit TestInkDropHost(InkDropImpl::AutoHighlightMode auto_highlight_mode =
                               InkDropImpl::AutoHighlightMode::NONE);

  TestInkDropHost(const TestInkDropHost&) = delete;
  TestInkDropHost& operator=(const TestInkDropHost&) = delete;

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

  void set_disable_timers_for_test(bool disable_timers_for_test) {
    disable_timers_for_test_ = disable_timers_for_test;
  }

  // View:
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;

 private:
  int num_ink_drop_layers_added_ = 0;
  int num_ink_drop_layers_removed_ = 0;

  // CreateInkDrop{Ripple,Highlight} are const, so these members must be
  // mutable.
  mutable int num_ink_drop_ripples_created_ = 0;
  mutable int num_ink_drop_highlights_created_ = 0;

  // When true, the InkDropRipple/InkDropHighlight instances will have their
  // timers disabled after creation.
  bool disable_timers_for_test_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_HOST_H_
