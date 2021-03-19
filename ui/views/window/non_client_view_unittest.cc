// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/non_client_view.h"

#include <memory>

#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/native_frame_view.h"

namespace views {
namespace test {

namespace {

class NonClientFrameTestView : public NativeFrameView {
 public:
  using NativeFrameView::NativeFrameView;
  int layout_count() const { return layout_count_; }

  // NativeFrameView:
  void Layout() override {
    NativeFrameView::Layout();
    ++layout_count_;
  }

 private:
  int layout_count_ = 0;
};

class ClientTestView : public ClientView {
 public:
  using ClientView::ClientView;
  int layout_count() const { return layout_count_; }

  // ClientView:
  void Layout() override {
    ClientView::Layout();
    ++layout_count_;
  }

 private:
  int layout_count_ = 0;
};

class TestWidgetDelegate : public WidgetDelegateView {
 public:
  // WidgetDelegateView:
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView(
      Widget* widget) override {
    return std::make_unique<NonClientFrameTestView>(widget);
  }

  views::ClientView* CreateClientView(Widget* widget) override {
    return new ClientTestView(widget, this);
  }
};

class NonClientViewTest : public ViewsTestBase {
 public:
  Widget::InitParams CreateParams(Widget::InitParams::Type type) override {
    Widget::InitParams params = ViewsTestBase::CreateParams(type);
    params.delegate = new TestWidgetDelegate;
    return params;
  }
};

}  // namespace

// Ensure Layout() is not called excessively on a ClientView when Widget bounds
// are changing.
TEST_F(NonClientViewTest, OnlyLayoutChildViewsOnce) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(Widget::InitParams::TYPE_WINDOW);

  NonClientView* non_client_view = widget->non_client_view();
  non_client_view->Layout();

  auto* frame_view =
      static_cast<NonClientFrameTestView*>(non_client_view->frame_view());
  auto* client_view =
      static_cast<ClientTestView*>(non_client_view->client_view());

  int initial_frame_view_layouts = frame_view->layout_count();
  int initial_client_view_layouts = client_view->layout_count();

  // Make sure it does no layout when nothing has changed.
  non_client_view->Layout();
  EXPECT_EQ(frame_view->layout_count(), initial_frame_view_layouts);
  EXPECT_EQ(client_view->layout_count(), initial_client_view_layouts);

  // Ensure changing bounds triggers a (single) layout.
  widget->SetBounds(gfx::Rect(0, 0, 161, 100));
  EXPECT_EQ(frame_view->layout_count(), initial_frame_view_layouts + 1);
  EXPECT_EQ(client_view->layout_count(), initial_client_view_layouts + 1);
}

}  // namespace test
}  // namespace views
