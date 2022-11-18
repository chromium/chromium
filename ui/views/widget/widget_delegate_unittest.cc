// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_delegate.h"

#include <utility>

#include "base/test/bind.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

namespace views {
namespace {

using WidgetDelegateTest = views::ViewsTestBase;

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

TEST_F(WidgetDelegateTest, ClientViewFactoryCanReplaceClientView) {
  ViewTracker tracker;

  auto delegate = std::make_unique<WidgetDelegate>();
  delegate->SetClientViewFactory(
      base::BindLambdaForTesting([&tracker](Widget* widget) {
        auto view = std::make_unique<ClientView>(widget, nullptr);
        tracker.SetView(view.get());
        return view;
      }));

  auto client =
      base::WrapUnique<ClientView>(delegate->CreateClientView(nullptr));
  EXPECT_EQ(tracker.view(), client.get());
}

TEST_F(WidgetDelegateTest, OverlayViewFactoryCanReplaceOverlayView) {
  ViewTracker tracker;

  auto delegate = std::make_unique<WidgetDelegate>();
  delegate->SetOverlayViewFactory(base::BindLambdaForTesting([&tracker]() {
    auto view = std::make_unique<View>();
    tracker.SetView(view.get());
    return view;
  }));

  auto overlay = base::WrapUnique<View>(delegate->CreateOverlayView());
  EXPECT_EQ(tracker.view(), overlay.get());
}

TEST_F(WidgetDelegateTest, AppIconCanDifferFromWindowIcon) {
  auto delegate = std::make_unique<WidgetDelegate>();

  gfx::ImageSkia window_icon = gfx::test::CreateImageSkia(16, 16);
  delegate->SetIcon(ui::ImageModel::FromImageSkia(window_icon));
  gfx::ImageSkia app_icon = gfx::test::CreateImageSkia(48, 48);
  delegate->SetAppIcon(ui::ImageModel::FromImageSkia(app_icon));
  EXPECT_TRUE(delegate->GetWindowIcon().Rasterize(nullptr).BackedBySameObjectAs(
      window_icon));
  EXPECT_TRUE(
      delegate->GetWindowAppIcon().Rasterize(nullptr).BackedBySameObjectAs(
          app_icon));
}

TEST_F(WidgetDelegateTest, AppIconFallsBackToWindowIcon) {
  auto delegate = std::make_unique<WidgetDelegate>();

  gfx::ImageSkia window_icon = gfx::test::CreateImageSkia(16, 16);
  delegate->SetIcon(ui::ImageModel::FromImageSkia(window_icon));
  // Don't set an independent app icon.
  EXPECT_TRUE(
      delegate->GetWindowAppIcon().Rasterize(nullptr).BackedBySameObjectAs(
          window_icon));
}

}  // namespace
}  // namespace views
