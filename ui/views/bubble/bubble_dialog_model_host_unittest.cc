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
// WeakPtrs to this delegate is used to infer when DialogModel is destroyed.
class WeakDialogModelDelegate : public ui::DialogModelDelegate {
 public:
  base::WeakPtr<WeakDialogModelDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<WeakDialogModelDelegate> weak_ptr_factory_{this};
};

}  // namespace

TEST_F(BubbleDialogModelHostTest, CloseIsSynchronousAndCallsWindowClosing) {
  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::TYPE_WINDOW);

  auto delegate = std::make_unique<WeakDialogModelDelegate>();
  auto weak_delegate = delegate->GetWeakPtr();

  int window_closing_count = 0;
  auto host = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder(std::move(delegate))
          .SetWindowClosingCallback(base::BindOnce(base::BindOnce(
              [](int* window_closing_count) { ++(*window_closing_count); },
              &window_closing_count)))
          .Build(),
      anchor_widget->GetContentsView(), BubbleBorder::Arrow::TOP_RIGHT);
  auto* host_ptr = host.get();

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(host.release());
  test::WidgetDestroyedWaiter waiter(bubble_widget);

  EXPECT_EQ(0, window_closing_count);
  DCHECK_EQ(host_ptr, weak_delegate->dialog_model()->host());
  weak_delegate->dialog_model()->host()->Close();
  EXPECT_EQ(1, window_closing_count);

  // The model (and hence delegate) should destroy synchronously, so the
  // WeakPtr should disappear before waiting for the views Widget to close.
  EXPECT_FALSE(weak_delegate);

  waiter.Wait();
}

}  // namespace views
