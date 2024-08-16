// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/web_dialog_view.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/test/test_web_contents_handler.h"
#include "ui/web_dialogs/test/test_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"
#include "url/gurl.h"

namespace views {

// Testing delegate configured for use in this test.
class TestWebDialogViewWebDialogDelegate
    : public ui::test::TestWebDialogDelegate {
 public:
  TestWebDialogViewWebDialogDelegate()
      : ui::test::TestWebDialogDelegate(GURL()) {}
  void set_close_on_escape(bool close_on_escape) {
    close_on_escape_ = close_on_escape;
  }

  // ui::WebDialogDelegate
  bool OnDialogCloseRequested() override { return true; }
  bool ShouldCloseDialogOnEscape() const override { return close_on_escape_; }
  ui::mojom::ModalType GetDialogModalType() const override {
    return ui::mojom::ModalType::kWindow;
  }

 private:
  bool close_on_escape_ = true;
};

// Provides functionality to test a WebDialogView.
class WebDialogViewUnitTest : public views::test::WidgetTest {
 public:
  WebDialogViewUnitTest()
      : views::test::WidgetTest(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {}

  WebDialogViewUnitTest(const WebDialogViewUnitTest&) = delete;
  WebDialogViewUnitTest& operator=(const WebDialogViewUnitTest&) = delete;

  ~WebDialogViewUnitTest() override = default;

  // testing::Test
  void SetUp() override {
    views::test::WidgetTest::SetUp();

    browser_context_ = std::make_unique<content::TestBrowserContext>();

    // Set the test content browser client to avoid pulling in needless
    // dependencies from content.
    SetBrowserClientForTesting(&test_browser_client_);

    web_dialog_delegate_ =
        std::make_unique<TestWebDialogViewWebDialogDelegate>();
    web_contents_ = CreateWebContents();
    web_dialog_view_ = new WebDialogView(
        web_contents_->GetBrowserContext(), web_dialog_delegate_.get(),
        std::make_unique<ui::test::TestWebContentsHandler>());

    // This prevents the initialization of the dialog from navigating
    // to the URL in the WebUI.  This is needed because the WebUI
    // loading code that would otherwise attempt to use our TestBrowserContext,
    // but will fail because TestBrowserContext not an instance of
    // Profile (i.e. TestingProfile).  We cannot, however, create an instance of
    // TestingProfile in this test due to dependency restrictions between the
    // views code and the location of TestingProfile.
    web_dialog_view_->disable_url_load_for_test_ = true;

    widget_ = views::DialogDelegate::CreateDialogWidget(web_dialog_view_,
                                                        GetContext(), nullptr);
    widget_->Show();
    EXPECT_FALSE(widget_is_closed());
  }

  void TearDown() override {
    web_dialog_view_ = nullptr;
    widget_.ExtractAsDangling()->CloseNow();
    views::test::WidgetTest::TearDown();
  }

  bool widget_is_closed() { return widget_->IsClosed(); }

  WebDialogView* web_dialog_view() { return web_dialog_view_; }

  views::WebView* web_view() {
    return web_dialog_view_ ? web_dialog_view_->web_view_.get() : nullptr;
  }

  ui::WebDialogDelegate* web_view_delegate() {
    return web_dialog_view_ ? web_dialog_view_->delegate_.get() : nullptr;
  }

  TestWebDialogViewWebDialogDelegate* web_dialog_delegate() {
    return web_dialog_delegate_.get();
  }

  void ResetWebDialogDelegate() { web_dialog_delegate_.reset(); }

 protected:
  std::unique_ptr<content::TestWebContents> CreateWebContents() const {
    return base::WrapUnique<content::TestWebContents>(
        content::TestWebContents::Create(
            content::WebContents::CreateParams(browser_context_.get())));
  }

  void SimulateKeyEvent(const ui::KeyEvent& event) {
    ASSERT_TRUE(web_dialog_view_->GetFocusManager() != nullptr);
    ASSERT_TRUE(widget_ != nullptr);
    ui::KeyEvent event_copy = event;
    if (web_dialog_view_->GetFocusManager()->OnKeyEvent(event_copy))
      widget_->OnKeyEvent(&event_copy);
  }

 private:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  content::TestContentBrowserClient test_browser_client_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  // These are raw pointers (vs unique pointers) because the views
  // system does its own internal memory management.
  raw_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<WebDialogView> web_dialog_view_ = nullptr;
  base::RepeatingClosure quit_closure_;

  std::unique_ptr<TestWebDialogViewWebDialogDelegate> web_dialog_delegate_;
  std::unique_ptr<content::TestWebContents> web_contents_;
};

TEST_F(WebDialogViewUnitTest, WebDialogViewClosedOnEscape) {
  web_dialog_delegate()->set_close_on_escape(true);
  const ui::KeyEvent escape_event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                                  ui::EF_NONE);
  SimulateKeyEvent(escape_event);

  // The Dialog Widget should be closed when escape is pressed.
  EXPECT_TRUE(widget_is_closed());
}

TEST_F(WebDialogViewUnitTest, WebDialogViewNotClosedOnEscape) {
  web_dialog_delegate()->set_close_on_escape(false);
  const ui::KeyEvent escape_event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                                  ui::EF_NONE);
  SimulateKeyEvent(escape_event);

  // The Dialog Widget should not be closed when escape is pressed.
  EXPECT_FALSE(widget_is_closed());
}

TEST_F(WebDialogViewUnitTest, ObservableWebViewOnWebDialogViewClosed) {
  // Close the widget by pressing ESC key.
  web_dialog_delegate()->set_close_on_escape(true);
  const ui::KeyEvent escape_event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                                  ui::EF_NONE);
  SimulateKeyEvent(escape_event);

  // The Dialog Widget should be closed .
  ASSERT_TRUE(widget_is_closed());
  // The delegate should be nullified so no further communication with it.
  EXPECT_FALSE(web_view_delegate());

  ResetWebDialogDelegate();
}

TEST_F(WebDialogViewUnitTest, MetadataTest) {
  test::TestViewMetadata(web_dialog_view());
}

}  // namespace views
