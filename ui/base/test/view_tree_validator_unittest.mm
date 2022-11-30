// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/view_tree_validator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/cocoa_helper.h"

using ViewTreeValidatorTest = ui::CocoaTest;

namespace {

NSView* AutoreleasedButtonWithFrame(int x, int y, int w, int h) {
  return [[[NSButton alloc] initWithFrame:NSMakeRect(x, y, w, h)] autorelease];
}

NSView* AutoreleasedViewWithFrame(int x, int y, int w, int h) {
  return [[[NSView alloc] initWithFrame:NSMakeRect(x, y, w, h)] autorelease];
}

void AdjustWidth(NSView* view, int delta) {
  NSRect r = view.frame;
  r.size.width += delta;
  [view setFrame:r];
}

void AdjustHeight(NSView* view, int delta) {
  NSRect r = view.frame;
  r.size.height += delta;
  [view setFrame:r];
}

}  // namespace

TEST_F(ViewTreeValidatorTest, CorrectnessTest) {
  NSWindow* window = test_window();
  int width = NSWidth(window.contentView.frame);
  int height = NSHeight(window.contentView.frame);
  NSView* view_1 = AutoreleasedViewWithFrame(0, 0, width / 2, height);
  NSView* view_2 = AutoreleasedButtonWithFrame(width / 2, 0, width / 2, height);
  NSView* view_3 = AutoreleasedButtonWithFrame(0, 0, width / 2, height / 2);
  NSView* view_4 = AutoreleasedViewWithFrame(0, 0, width / 2, height / 2);
  NSView* view_5 =
      AutoreleasedViewWithFrame(0, height / 2, width / 2, height / 2);

  [view_2 addSubview:view_4];
  [view_2 addSubview:view_5];
  [view_1 addSubview:view_3];
  [window.contentView addSubview:view_1];
  [window.contentView addSubview:view_2];

  {
    // The original layout is well-formed.
    absl::optional<ui::ViewTreeProblemDetails> details =
        ui::ValidateViewTree(window.contentView);
    EXPECT_FALSE(details.has_value());
  }

  {
    // Make view_3 no longer contained within view_1.
    AdjustWidth(view_3, 1);
    absl::optional<ui::ViewTreeProblemDetails> details =
        ui::ValidateViewTree(window.contentView);
    ASSERT_TRUE(details.has_value());
    EXPECT_EQ(details->type, ui::ViewTreeProblemDetails::VIEW_OUTSIDE_PARENT);
    EXPECT_EQ(details->view_a, view_3);
    EXPECT_EQ(details->view_b, view_1);
    AdjustWidth(view_3, -1);
  }

  {
    // Make view_1 overlap view_2.
    AdjustWidth(view_1, 1);
    absl::optional<ui::ViewTreeProblemDetails> details =
        ui::ValidateViewTree(window.contentView);
    ASSERT_TRUE(details.has_value());
    EXPECT_EQ(details->type, ui::ViewTreeProblemDetails::VIEWS_OVERLAP);

    // Since there's no specified order for |view_a| and |view_b| for
    // VIEWS_OVERLAP, check that |view_1| and |view_2| both appear exactly once
    // in |view_a| and |view_b|.
    EXPECT_TRUE(details->view_a == view_1 || details->view_a == view_2);
    EXPECT_TRUE(details->view_b == view_1 || details->view_b == view_2);
    EXPECT_NE(details->view_a, details->view_b);
    AdjustWidth(view_1, -1);
  }

  {
    // Make view_4 overlap view_5. Since they're both not localizable, this
    // isn't an error.
    AdjustHeight(view_4, 1);
    absl::optional<ui::ViewTreeProblemDetails> details =
        ui::ValidateViewTree(window.contentView);
    EXPECT_FALSE(details.has_value());
    AdjustHeight(view_4, -1);
  }
}
