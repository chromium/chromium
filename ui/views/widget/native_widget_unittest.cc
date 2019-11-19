// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

namespace views {

class ScopedTestWidget {
 public:
  explicit ScopedTestWidget(internal::NativeWidgetPrivate* native_widget)
      : native_widget_(native_widget) {
  }
  ~ScopedTestWidget() {
    // |CloseNow| deletes both |native_widget_| and its associated
    // |Widget|.
    native_widget_->GetWidget()->CloseNow();
  }

  internal::NativeWidgetPrivate* operator->() const  {
    return native_widget_;
  }
  internal::NativeWidgetPrivate* get() const { return native_widget_; }

 private:
  internal::NativeWidgetPrivate* native_widget_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTestWidget);
};

class NativeWidgetTest : public ViewsTestBase {
 public:
  NativeWidgetTest() = default;
  ~NativeWidgetTest() override = default;

  internal::NativeWidgetPrivate* CreateNativeWidgetOfType(
      Widget::InitParams::Type type) {
    Widget* widget = new Widget;
    Widget::InitParams params = CreateParams(type);
    params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
    params.bounds = gfx::Rect(10, 10, 200, 200);
    widget->Init(std::move(params));
    return widget->native_widget_private();
  }

  internal::NativeWidgetPrivate* CreateNativeWidget() {
    return CreateNativeWidgetOfType(Widget::InitParams::TYPE_POPUP);
  }

  internal::NativeWidgetPrivate* CreateNativeSubWidget() {
    return CreateNativeWidgetOfType(Widget::InitParams::TYPE_CONTROL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetTest);
};

TEST_F(NativeWidgetTest, CreateNativeWidget) {
  ScopedTestWidget widget(CreateNativeWidget());
  EXPECT_TRUE(widget->GetWidget()->GetNativeView());
}

TEST_F(NativeWidgetTest, GetNativeWidgetForNativeView) {
  ScopedTestWidget widget(CreateNativeWidget());
  EXPECT_EQ(widget.get(),
            internal::NativeWidgetPrivate::GetNativeWidgetForNativeView(
                widget->GetWidget()->GetNativeView()));
}

// |widget| has the toplevel NativeWidget.
TEST_F(NativeWidgetTest, GetTopLevelNativeWidget1) {
  ScopedTestWidget widget(CreateNativeWidget());
  EXPECT_EQ(widget.get(),
            internal::NativeWidgetPrivate::GetTopLevelNativeWidget(
                widget->GetWidget()->GetNativeView()));
}

// |toplevel_widget| has the toplevel NativeWidget.
TEST_F(NativeWidgetTest, GetTopLevelNativeWidget2) {
  internal::NativeWidgetPrivate* child_widget = CreateNativeSubWidget();
  {
    ScopedTestWidget toplevel_widget(CreateNativeWidget());

    // |toplevel_widget| owns |child_host|.
    NativeViewHost* child_host = new NativeViewHost;
    toplevel_widget->GetWidget()->SetContentsView(child_host);

    // |child_host| hosts |child_widget|'s NativeView.
    child_host->Attach(child_widget->GetWidget()->GetNativeView());

    EXPECT_EQ(toplevel_widget.get(),
              internal::NativeWidgetPrivate::GetTopLevelNativeWidget(
                  child_widget->GetWidget()->GetNativeView()));
  }
  // NativeViewHost only had a weak reference to the |child_widget|'s
  // NativeView. Delete it and the associated Widget.
  child_widget->CloseNow();
}

}  // namespace views
