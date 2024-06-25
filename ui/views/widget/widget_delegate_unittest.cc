// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_delegate.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#endif

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

class TestWidgetDelegate : public WidgetDelegate {
 public:
  TestWidgetDelegate() = default;
  TestWidgetDelegate(const TestWidgetDelegate&) = delete;
  TestWidgetDelegate operator=(const TestWidgetDelegate&) = delete;
  ~TestWidgetDelegate() override = default;

  // WidgetDelegate:
  void GetAccessiblePanes(std::vector<View*>* panes) override {
    base::ranges::copy(accessible_panes_, std::back_inserter(*panes));
  }

  void SetAccessiblePanes(
      const std::vector<raw_ptr<View, VectorExperimental>>& panes) {
    accessible_panes_ = panes;
  }

 private:
  std::vector<raw_ptr<View, VectorExperimental>> accessible_panes_;
};

TEST_F(WidgetDelegateTest, RotatePaneFocusFromView) {
  // Ordering matters, delegate must outlive widget.
  TestWidgetDelegate delegate;
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.delegate = &delegate;
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  widget->Init(std::move(params));

  auto* pane1 = widget->GetContentsView()->AddChildView(
      std::make_unique<AccessiblePaneView>());

  auto* v1 = pane1->AddChildView(std::make_unique<View>());
  v1->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  auto* v2 = pane1->AddChildView(std::make_unique<View>());
  v2->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  auto* pane2 = widget->GetContentsView()->AddChildView(
      std::make_unique<AccessiblePaneView>());

  auto* v3 = pane2->AddChildView(std::make_unique<View>());
  v3->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  auto* v4 = pane2->AddChildView(std::make_unique<View>());
  v4->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  std::vector<raw_ptr<views::View, VectorExperimental>> panes;
  panes.push_back(pane1);
  panes.push_back(pane2);
  delegate.SetAccessiblePanes(panes);

  widget->Show();

  auto* focus_manager = widget->GetFocusManager();
  auto get_focused_view = [focus_manager] {
    return focus_manager->GetFocusedView();
  };

  // Start rotating from no focus. This should set the focus on the first pane.
  EXPECT_TRUE(delegate.RotatePaneFocusFromView(nullptr, true, true));
  EXPECT_EQ(v1, get_focused_view());

  // Attempt to rotate with a view that is not contained by the widget without
  // wrapping enabled, this should result in no rotation.
  EXPECT_FALSE(delegate.RotatePaneFocusFromView(nullptr, true, false));

  // Rotate to the next.
  EXPECT_TRUE(
      delegate.RotatePaneFocusFromView(get_focused_view(), true, false));
  EXPECT_EQ(v3, get_focused_view());

  // Attempting to rotate again at the end should result in a non-rotation.
  EXPECT_FALSE(
      delegate.RotatePaneFocusFromView(get_focused_view(), true, false));

  // Now attempt to rotate again with wrapping enabled. It should wrap around.
  EXPECT_TRUE(delegate.RotatePaneFocusFromView(get_focused_view(), true, true));
  EXPECT_EQ(v1, get_focused_view());

  // Attempt to rotate backwards without wrapping. This should fail.
  EXPECT_FALSE(
      delegate.RotatePaneFocusFromView(get_focused_view(), false, false));
  EXPECT_EQ(v1, get_focused_view());

  // Now wrap around and rotate to the end again.
  EXPECT_TRUE(
      delegate.RotatePaneFocusFromView(get_focused_view(), false, true));
  EXPECT_EQ(v3, get_focused_view());

  EXPECT_TRUE(
      delegate.RotatePaneFocusFromView(get_focused_view(), false, false));
  EXPECT_EQ(v1, get_focused_view());

  // Now clear the view and try to start again from the back.
  focus_manager->ClearFocus();
  EXPECT_EQ(nullptr, get_focused_view());
  EXPECT_TRUE(
      delegate.RotatePaneFocusFromView(get_focused_view(), false, true));

  // Set the focus to the first pane then attempt to rotate the focus. There
  // should be logic to prevent the focus from cycling infinitely.
  focus_manager->SetFocusedView(v1);
  pane2->SetVisible(false);
  EXPECT_FALSE(
      delegate.RotatePaneFocusFromView(get_focused_view(), true, true));

  widget->Close();
}

TEST_F(WidgetDelegateTest, SetCanFullscreen) {
  TestWidgetDelegate delegate;
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.delegate = &delegate;
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  widget->Init(std::move(params));

  auto CheckCanFullscreen = [&](bool expected) {
    EXPECT_EQ(delegate.CanFullscreen(), expected);

#if defined(USE_AURA)
    EXPECT_EQ((widget->GetNativeWindow()->GetProperty(
                   aura::client::kResizeBehaviorKey) &
               aura::client::kResizeBehaviorCanFullscreen) > 0,
              expected);
#endif
  };

  CheckCanFullscreen(false);
  delegate.SetCanFullscreen(true);
  CheckCanFullscreen(true);
}

TEST_F(WidgetDelegateTest, SetCanResize) {
  TestWidgetDelegate delegate;
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.delegate = &delegate;
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  widget->Init(std::move(params));

  auto CheckCanResize = [&](bool expected) {
    EXPECT_EQ(delegate.CanResize(), expected);

#if defined(USE_AURA)
    EXPECT_EQ((widget->GetNativeWindow()->GetProperty(
                   aura::client::kResizeBehaviorKey) &
               aura::client::kResizeBehaviorCanResize) > 0,
              expected);
#endif
  };

  CheckCanResize(false);
  delegate.SetCanResize(true);
  CheckCanResize(true);
}

TEST_F(WidgetDelegateTest, CanMaximize) {
  TestWidgetDelegate delegate;
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.delegate = &delegate;
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  widget->Init(std::move(params));

  auto CheckCanMaximize = [&](bool expected) {
    EXPECT_EQ(delegate.CanMaximize(), expected);

#if defined(USE_AURA)
    EXPECT_EQ((widget->GetNativeWindow()->GetProperty(
                   aura::client::kResizeBehaviorKey) &
               aura::client::kResizeBehaviorCanMaximize) > 0,
              expected);
#endif
  };

  CheckCanMaximize(false);
  delegate.SetCanMaximize(true);
  CheckCanMaximize(true);
}

TEST_F(WidgetDelegateTest, CanMinimize) {
  TestWidgetDelegate delegate;
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.delegate = &delegate;
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  widget->Init(std::move(params));

  auto CheckCanMinimize = [&](bool expected) {
    EXPECT_EQ(delegate.CanMinimize(), expected);

#if defined(USE_AURA)
    EXPECT_EQ((widget->GetNativeWindow()->GetProperty(
                   aura::client::kResizeBehaviorKey) &
               aura::client::kResizeBehaviorCanMinimize) > 0,
              expected);
#endif
  };

  CheckCanMinimize(false);
  delegate.SetCanMinimize(true);
  CheckCanMinimize(true);
}

}  // namespace
}  // namespace views
