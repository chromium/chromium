// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

class TestButtonThatSkipsDefaultKeyEventProcessing : public LabelButton {
  METADATA_HEADER(TestButtonThatSkipsDefaultKeyEventProcessing, LabelButton)

 public:
  TestButtonThatSkipsDefaultKeyEventProcessing()
      : LabelButton(Button::PressedCallback(), u"Cancel") {}
  ~TestButtonThatSkipsDefaultKeyEventProcessing() override = default;

  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override {
    return true;
  }
};

BEGIN_METADATA(TestButtonThatSkipsDefaultKeyEventProcessing)
END_METADATA

}  // namespace

class UnhandledKeyboardEventHandlerTest : public views::test::WidgetTest {
 public:
  UnhandledKeyboardEventHandlerTest()
      : views::test::WidgetTest(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {}

  UnhandledKeyboardEventHandlerTest(const UnhandledKeyboardEventHandlerTest&) =
      delete;
  UnhandledKeyboardEventHandlerTest& operator=(
      const UnhandledKeyboardEventHandlerTest&) = delete;

  ~UnhandledKeyboardEventHandlerTest() override = default;

  std::unique_ptr<content::WebContents> CreateWebContentsForWebView(
      content::BrowserContext* browser_context) {
    return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                             nullptr);
  }

  void SetUp() override {
    SetBrowserClientForTesting(&test_browser_client_);
    rvh_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    WebView::WebContentsCreator creator = base::BindRepeating(
        &UnhandledKeyboardEventHandlerTest::CreateWebContentsForWebView,
        base::Unretained(this));
    scoped_web_contents_creator_ =
        std::make_unique<WebView::ScopedWebContentsCreatorForTesting>(creator);
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    WidgetTest::SetUp();

    top_level_widget_ = CreateTopLevelFramelessPlatformWidget();
    top_level_widget_->SetBounds(gfx::Rect(0, 10, 100, 100));
    View* const contents_view =
        top_level_widget_->SetContentsView(std::make_unique<View>());
    web_view_ = contents_view->AddChildView(
        std::make_unique<WebView>(browser_context_.get()));
    top_level_widget_->Show();

    focus_manager_ = top_level_widget_->GetFocusManager();
    ASSERT_TRUE(focus_manager_);

    focus_manager_->RegisterAccelerator(
        ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE),
        ui::AcceleratorManager::kNormalPriority, &target_);
  }

  void TearDown() override {
    if (focus_manager_) {
      focus_manager_->UnregisterAccelerators(&target_);
      focus_manager_ = nullptr;
    }
    web_view_ = nullptr;
    top_level_widget_.ExtractAsDangling()->CloseNow();
    WidgetTest::TearDown();
    browser_context_.reset();
    scoped_web_contents_creator_.reset();
    rvh_enabler_.reset();
  }

  input::NativeWebKeyboardEvent CreateReturnKeyDownEvent() {
    input::NativeWebKeyboardEvent event(
        blink::WebInputEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.windows_key_code = ui::VKEY_RETURN;
    return event;
  }

 protected:
  content::TestContentBrowserClient test_browser_client_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_enabler_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::unique_ptr<WebView::ScopedWebContentsCreatorForTesting>
      scoped_web_contents_creator_;
  raw_ptr<Widget> top_level_widget_ = nullptr;
  raw_ptr<WebView> web_view_ = nullptr;
  raw_ptr<FocusManager> focus_manager_ = nullptr;
  ui::TestAcceleratorTarget target_;
  UnhandledKeyboardEventHandler handler_;
};

// Verifies that renderer-unhandled Enter events do not fire accelerators when
// focus is on a button that consumes key events (e.g., Cancel button).
TEST_F(UnhandledKeyboardEventHandlerTest,
       ReturnSkipsAcceleratorWhenButtonFocused) {
  auto* button = top_level_widget_->GetContentsView()->AddChildView(
      std::make_unique<TestButtonThatSkipsDefaultKeyEventProcessing>());
  focus_manager_->SetFocusedView(button);
  EXPECT_EQ(button, focus_manager_->GetFocusedView());

  EXPECT_TRUE(button->SkipDefaultKeyEventProcessing(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RETURN, ui::EF_NONE)));

  EXPECT_TRUE(
      handler_.HandleKeyboardEvent(CreateReturnKeyDownEvent(), focus_manager_));
  EXPECT_EQ(0, target_.accelerator_count());
}

// Verifies that renderer-unhandled Enter events fire accelerators when a
// WebView is focused, since its renderer already had a chance to consume the
// key.
TEST_F(UnhandledKeyboardEventHandlerTest,
       ReturnFiresAcceleratorWhenWebViewFocused) {
  focus_manager_->SetFocusedView(web_view_);
  EXPECT_EQ(web_view_, focus_manager_->GetFocusedView());

  EXPECT_TRUE(
      handler_.HandleKeyboardEvent(CreateReturnKeyDownEvent(), focus_manager_));
  EXPECT_EQ(1, target_.accelerator_count());
}

// Verifies that renderer-unhandled Enter events fire accelerators when focus
// is on a view that does not consume default key events.
TEST_F(UnhandledKeyboardEventHandlerTest,
       ReturnFiresAcceleratorWhenNormalViewFocused) {
  View* normal_view = top_level_widget_->GetContentsView()->AddChildView(
      std::make_unique<View>());
  normal_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  focus_manager_->SetFocusedView(normal_view);
  EXPECT_EQ(normal_view, focus_manager_->GetFocusedView());

  EXPECT_FALSE(normal_view->SkipDefaultKeyEventProcessing(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RETURN, ui::EF_NONE)));

  EXPECT_TRUE(
      handler_.HandleKeyboardEvent(CreateReturnKeyDownEvent(), focus_manager_));
  EXPECT_EQ(1, target_.accelerator_count());
}

}  // namespace views
