// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)

namespace views {

class BubbleDialogDelegateViewInteractiveTest : public test::WidgetTest {
 public:
  BubbleDialogDelegateViewInteractiveTest() = default;

  ~BubbleDialogDelegateViewInteractiveTest() override = default;

  void SetUp() override {
    SetUpForInteractiveTests();
    test::WidgetTest::SetUp();
    original_nw_factory_ =
        ViewsDelegate::GetInstance()->native_widget_factory();
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    ViewsDelegate::GetInstance()->set_native_widget_factory(
        base::BindRepeating(CreateNativeWidget));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void TearDown() override {
    ViewsDelegate::GetInstance()->set_native_widget_factory(
        original_nw_factory_);
    test::WidgetTest::TearDown();
  }

 private:
  static NativeWidget* CreateNativeWidget(
      const Widget::InitParams& params,
      internal::NativeWidgetDelegate* delegate) {
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
    // Create DesktopNativeWidgetAura for toplevel widgets, NativeWidgetAura
    // otherwise.
    if (!params.parent)
      return new DesktopNativeWidgetAura(delegate);
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)
    return new NativeWidgetAura(delegate);
  }

  ViewsDelegate::NativeWidgetFactory original_nw_factory_;
};

TEST_F(BubbleDialogDelegateViewInteractiveTest,
       BubbleAndParentNotActiveSimultaneously) {
  WidgetAutoclosePtr anchor_widget(CreateTopLevelNativeWidget());
  View* anchor_view = anchor_widget->GetContentsView();
  anchor_widget->LayoutRootViewIfNecessary();

  anchor_widget->Show();
  test::WaitForWidgetActive(anchor_widget.get(), true);
  EXPECT_TRUE(anchor_widget->IsActive());
  EXPECT_TRUE(anchor_widget->GetNativeWindow()->HasFocus());

  auto bubble = std::make_unique<BubbleDialogDelegateView>(
      anchor_view, BubbleBorder::Arrow::TOP_CENTER);
  bubble->set_close_on_deactivate(false);
  WidgetAutoclosePtr bubble_widget(
      BubbleDialogDelegateView::CreateBubble(std::move(bubble)));

  bubble_widget->Show();
  EXPECT_FALSE(anchor_widget->IsActive());
  EXPECT_TRUE(bubble_widget->IsActive());

  // TODO(crbug.com/40183517): We are not checking anchor_widget's
  // aura::Window because it might not get focus. This happens in test
  // suites that don't use FocusController on NativeWidgetAura.

  anchor_widget->Activate();
  EXPECT_TRUE(anchor_widget->IsActive());
  EXPECT_FALSE(bubble_widget->IsActive());

  // Check the backing aura::Windows as well.
  EXPECT_TRUE(anchor_widget->GetNativeWindow()->HasFocus());
  EXPECT_FALSE(bubble_widget->GetNativeWindow()->HasFocus());
}

}  // namespace views
