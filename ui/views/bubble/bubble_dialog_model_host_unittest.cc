// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_model_host.h"

#include <memory>
#include <utility>

#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace views {

using BubbleDialogModelHostTest = ViewsTestBase;

// TODO(pbos): Consider moving tests from this file into a test base for
// DialogModel that can be instantiated by any DialogModelHost implementation to
// check its compliance.

namespace {
// TODO(pbos): Consider moving this to a non-views testutil location. This is
// likely usable without/outside views (even if the test suite doesn't move).
class TestModelDelegate : public ui::DialogModelDelegate {
 public:
  struct Stats {
    int window_closing_count = 0;
  };

  explicit TestModelDelegate(Stats* stats) : stats_(stats) {}

  base::WeakPtr<TestModelDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  static std::unique_ptr<ui::DialogModel> BuildModel(
      std::unique_ptr<TestModelDelegate> delegate) {
    auto* delegate_ptr = delegate.get();
    return ui::DialogModel::Builder(std::move(delegate))
        .SetWindowClosingCallback(
            base::BindOnce(&TestModelDelegate::OnWindowClosing,
                           base::Unretained(delegate_ptr)))
        .Build();
  }

  void OnWindowClosing() { ++stats_->window_closing_count; }

 private:
  Stats* const stats_;
  base::WeakPtrFactory<TestModelDelegate> weak_ptr_factory_{this};
};
}  // namespace

TEST_F(BubbleDialogModelHostTest, CloseIsSynchronousAndCallsWindowClosing) {
  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::TYPE_WINDOW);

  TestModelDelegate::Stats stats;
  auto delegate = std::make_unique<TestModelDelegate>(&stats);
  auto weak_delegate = delegate->GetWeakPtr();

  auto host = std::make_unique<BubbleDialogModelHost>(
      TestModelDelegate::BuildModel(std::move(delegate)),
      anchor_widget->GetContentsView(), BubbleBorder::Arrow::TOP_RIGHT);
  auto* host_ptr = host.get();

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(host.release());
  test::WidgetDestroyedWaiter waiter(bubble_widget);

  EXPECT_EQ(0, stats.window_closing_count);
  DCHECK_EQ(host_ptr, weak_delegate->dialog_model()->host());
  weak_delegate->dialog_model()->host()->Close();
  EXPECT_EQ(1, stats.window_closing_count);

  // The model (and hence delegate) should destroy synchronously, so the
  // WeakPtr should disappear before waiting for the views Widget to close.
  EXPECT_FALSE(weak_delegate);

  waiter.Wait();
}

}  // namespace views
