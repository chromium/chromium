// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/web_bubble_dialog_view.h"

#include <memory>
#include <utility>

#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
namespace test {

class WebBubbleDialogViewTest : public ViewsTestBase {
 public:
  WebBubbleDialogViewTest()
      : ViewsTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {}
  WebBubbleDialogViewTest(const WebBubbleDialogViewTest&) = delete;
  WebBubbleDialogViewTest& operator=(const WebBubbleDialogViewTest&) = delete;
  ~WebBubbleDialogViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    browser_context_ = std::make_unique<content::TestBrowserContext>();

    anchor_widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Init(std::move(params));
    auto bubble_view = std::make_unique<WebBubbleDialogView>(
        browser_context_.get(), anchor_widget_->GetContentsView());
    bubble_view_ = bubble_view.get();
    bubble_view_->set_hosted_in_bubble_for_testing();
    bubble_widget_ =
        BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  }
  void TearDown() override {
    bubble_widget_->CloseNow();
    anchor_widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  WebBubbleDialogView* bubble_view() { return bubble_view_; }
  Widget* bubble_widget() { return bubble_widget_; }

 private:
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  views::UniqueWidgetPtr anchor_widget_;
  Widget* bubble_widget_ = nullptr;
  WebBubbleDialogView* bubble_view_ = nullptr;
};

TEST_F(WebBubbleDialogViewTest, TestBubbleResize) {
  views::WebView* const web_view = bubble_view()->web_view();
  constexpr gfx::Size web_view_initial_size(100, 100);
  web_view->SetPreferredSize(gfx::Size(100, 100));
  bubble_view()->OnWebViewSizeChanged();
  const gfx::Size widget_initial_size =
      bubble_widget()->GetWindowBoundsInScreen().size();
  // The bubble should be at least as big as the webview.
  EXPECT_GE(widget_initial_size.width(), web_view_initial_size.width());
  EXPECT_GE(widget_initial_size.height(), web_view_initial_size.height());

  // Resize the webview.
  constexpr gfx::Size web_view_final_size(200, 200);
  web_view->SetPreferredSize(web_view_final_size);
  bubble_view()->OnWebViewSizeChanged();

  // Ensure the bubble resizes as expected.
  const gfx::Size widget_final_size =
      bubble_widget()->GetWindowBoundsInScreen().size();
  EXPECT_LT(widget_initial_size.width(), widget_final_size.width());
  EXPECT_LT(widget_initial_size.height(), widget_final_size.height());
  // The bubble should be at least as big as the webview.
  EXPECT_GE(widget_final_size.width(), web_view_final_size.width());
  EXPECT_GE(widget_final_size.height(), web_view_final_size.height());
}

}  // namespace test
}  // namespace views
