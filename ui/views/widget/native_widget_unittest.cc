// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
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
      : native_widget_(native_widget) {}

  ScopedTestWidget(const ScopedTestWidget&) = delete;
  ScopedTestWidget& operator=(const ScopedTestWidget&) = delete;

  ~ScopedTestWidget() {
    // `CloseNow` deletes both `native_widget_` and its associated `Widget`.
    native_widget_.ExtractAsDangling()->GetWidget()->CloseNow();
  }

  internal::NativeWidgetPrivate* operator->() const { return native_widget_; }
  internal::NativeWidgetPrivate* get() const { return native_widget_; }

 private:
  raw_ptr<internal::NativeWidgetPrivate> native_widget_;
};

class NativeWidgetTest : public ViewsTestBase {
 public:
  NativeWidgetTest() = default;

  NativeWidgetTest(const NativeWidgetTest&) = delete;
  NativeWidgetTest& operator=(const NativeWidgetTest&) = delete;

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
    NativeViewHost* child_host = toplevel_widget->GetWidget()->SetContentsView(
        std::make_unique<NativeViewHost>());

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

// GetTopLevelNativeWidget() finds the closest top-level widget.
TEST_F(NativeWidgetTest, GetTopLevelNativeWidget3) {
  ScopedTestWidget root_widget(CreateNativeWidget());
  ScopedTestWidget child_widget(CreateNativeWidget());
  ScopedTestWidget grandchild_widget(CreateNativeSubWidget());
  ASSERT_TRUE(root_widget->GetWidget()->is_top_level());
  ASSERT_TRUE(child_widget->GetWidget()->is_top_level());
  ASSERT_FALSE(grandchild_widget->GetWidget()->is_top_level());

  Widget::ReparentNativeView(grandchild_widget->GetNativeView(),
                             child_widget->GetNativeView());
  Widget::ReparentNativeView(child_widget->GetNativeView(),
                             root_widget->GetNativeView());

  EXPECT_EQ(child_widget.get(),
            internal::NativeWidgetPrivate::GetTopLevelNativeWidget(
                grandchild_widget->GetWidget()->GetNativeView()));
}

}  // namespace views
