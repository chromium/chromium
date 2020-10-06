// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_delegate.h"

#include <utility>

#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

namespace views {
namespace {

using WidgetDelegateTest = views::ViewsTestBase;

TEST_F(WidgetDelegateTest, ClientOwnedContentsViewOwnershipNotHeld) {
  std::unique_ptr<View> view = std::make_unique<View>();
  view->set_owned_by_client();
  ViewTracker tracker(view.get());

  auto delegate = std::make_unique<WidgetDelegate>();
  delegate->SetContentsView(view.get());
  delegate.reset();

  ASSERT_TRUE(tracker.view());
  view.reset();
  EXPECT_FALSE(tracker.view());
}

TEST_F(WidgetDelegateTest, ContentsViewOwnershipHeld) {
  std::unique_ptr<View> view = std::make_unique<View>();
  ViewTracker tracker(view.get());

  auto delegate = std::make_unique<WidgetDelegate>();
  delegate->SetContentsView(std::move(view));
  delegate.reset();

  EXPECT_FALSE(tracker.view());
}

TEST_F(WidgetDelegateTest, ContentsViewOwnershipTransferredToCaller) {
  std::unique_ptr<View> view = std::make_unique<View>();
  ViewTracker tracker(view.get());
  std::unique_ptr<View> same_view;

  auto delegate = std::make_unique<WidgetDelegate>();
  delegate->SetContentsView(std::move(view));
  same_view = base::WrapUnique(delegate->TransferOwnershipOfContentsView());
  EXPECT_EQ(tracker.view(), same_view.get());
  delegate.reset();

  EXPECT_TRUE(tracker.view());
}

TEST_F(WidgetDelegateTest, GetContentsViewDoesNotTransferOwnership) {
  std::unique_ptr<View> view = std::make_unique<View>();
  ViewTracker tracker(view.get());

  auto delegate = std::make_unique<WidgetDelegate>();
  delegate->SetContentsView(std::move(view));
  EXPECT_EQ(delegate->GetContentsView(), tracker.view());
  delegate.reset();

  EXPECT_FALSE(tracker.view());
}

}  // namespace
}  // namespace views
