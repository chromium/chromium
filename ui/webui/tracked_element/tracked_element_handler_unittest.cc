// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/tracked_element_handler.h"

#include <memory>

#include "base/test/bind.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace ui {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementIdentifier1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementIdentifier2);
constexpr gfx::RectF kElementBounds{10, 20, 30, 40};
constexpr gfx::RectF kElementBounds2{15, 25, 35, 45};

}  // namespace

class TrackedElementHandlerTest : public views::test::WidgetTest {
 public:
  TrackedElementHandlerTest()
      : views::test::WidgetTest(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {}
  ~TrackedElementHandlerTest() override = default;

  void SetUp() override {
    content::SetBrowserClientForTesting(&test_browser_client_);
    rvh_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    views::test::WidgetTest::SetUp();

    browser_context_ = std::make_unique<content::TestBrowserContext>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        browser_context_.get(), nullptr);

    mojo::PendingRemote<tracked_element::mojom::TrackedElementHandler> remote;
    handler_ = std::make_unique<TrackedElementHandler>(
        web_contents_.get(), remote.InitWithNewPipeAndPassReceiver(),
        // When there is a consistent way of assigning contexts, use that.
        ui::ElementContext::CreateFakeContextForTesting(web_contents_.get()),
        std::vector<ui::ElementIdentifier>{kTestElementIdentifier1,
                                           kTestElementIdentifier2});
    tracked_element_handler_remote_.Bind(std::move(remote));
  }

  void TearDown() override {
    tracked_element_handler_remote_.reset();
    handler_.reset();
    web_contents_.reset();
    browser_context_.reset();
    rvh_enabler_.reset();
    views::test::WidgetTest::TearDown();
  }

 protected:
  tracked_element::mojom::TrackedElementHandler* handler_remote() {
    return tracked_element_handler_remote_.get();
  }

  TrackedElementHandler* handler() { return handler_.get(); }

  content::ContentBrowserClient test_browser_client_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_enabler_;
  std::unique_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<TrackedElementHandler> handler_;
  mojo::Remote<tracked_element::mojom::TrackedElementHandler>
      tracked_element_handler_remote_;
};

TEST_F(TrackedElementHandlerTest, StartsWithNoElement) {
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier1));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier2));
}

TEST_F(TrackedElementHandlerTest, ElementCreatedOnEvent) {
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTestElementIdentifier1);
  EXPECT_TRUE(element);
  EXPECT_TRUE(element->IsA<TrackedElementWebUI>());
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier2));

  // Verify that we don't leave elements dangling if the handler is destroyed.
  handler_.reset();
  tracked_element_handler_remote_.FlushForTesting();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier1));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier2));
}

TEST_F(TrackedElementHandlerTest, ElementHiddenOnEvent) {
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier1));

  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), false, gfx::RectF());
  tracked_element_handler_remote_.FlushForTesting();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier1));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier2));
}

TEST_F(TrackedElementHandlerTest, ElementActivatedOnEvent) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, activated);
  const std::string name = kTestElementIdentifier1.GetName();
  handler_remote()->TrackedElementVisibilityChanged(name, true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kTestElementIdentifier1);
  ASSERT_TRUE(element);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          element->identifier(), element->context(), activated.Get());
  EXPECT_CALL_IN_SCOPE(activated, Run(element), {
    handler_remote()->TrackedElementActivated(name);
    tracked_element_handler_remote_.FlushForTesting();
  });
}

TEST_F(TrackedElementHandlerTest, ElementCustomEventOnEvent) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEvent);
  kCustomEvent.GetName();  // Register it.
  const std::string event_name = kCustomEvent.GetName();
  const std::string element_name = kTestElementIdentifier1.GetName();
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, custom_event);
  handler_remote()->TrackedElementVisibilityChanged(element_name, true,
                                                    kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kTestElementIdentifier1);
  ASSERT_TRUE(element);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kCustomEvent, element->context(), custom_event.Get());
  EXPECT_CALL_IN_SCOPE(custom_event, Run(element), {
    handler_remote()->TrackedElementCustomEvent(element_name, event_name);
    tracked_element_handler_remote_.FlushForTesting();
  });
}

TEST_F(TrackedElementHandlerTest,
       ElementBoundsChangedEventFiredOnBoundsChange) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, bounds_changed);
  const std::string element_name = kTestElementIdentifier1.GetName();

  // Make element visible with initial bounds.
  handler_remote()->TrackedElementVisibilityChanged(element_name, true,
                                                    kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kTestElementIdentifier1);
  ASSERT_TRUE(element);

  // Subscribe to bounds changed event.
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kElementBoundsChangedEvent, element->context(), bounds_changed.Get());

  // Change bounds - should trigger the event.
  EXPECT_CALL_IN_SCOPE(bounds_changed, Run(element), {
    handler_remote()->TrackedElementVisibilityChanged(element_name, true,
                                                      kElementBounds2);
    tracked_element_handler_remote_.FlushForTesting();
  });
}

TEST_F(TrackedElementHandlerTest,
       ElementBoundsChangedEventNotFiredWhenBoundsUnchanged) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, bounds_changed);
  const std::string element_name = kTestElementIdentifier1.GetName();

  // Make element visible with initial bounds.
  handler_remote()->TrackedElementVisibilityChanged(element_name, true,
                                                    kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kTestElementIdentifier1);
  ASSERT_TRUE(element);

  // Subscribe to bounds changed event.
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kElementBoundsChangedEvent, element->context(), bounds_changed.Get());

  // Send same bounds - should NOT trigger the event.
  EXPECT_CALL(bounds_changed, Run).Times(0);
  handler_remote()->TrackedElementVisibilityChanged(element_name, true,
                                                    kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();
}

TEST_F(TrackedElementHandlerTest, ElementBoundsChangedEventNotFiredWhenHidden) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, bounds_changed);
  const std::string element_name = kTestElementIdentifier1.GetName();

  // Make element visible with initial bounds.
  handler_remote()->TrackedElementVisibilityChanged(element_name, true,
                                                    kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kTestElementIdentifier1);
  ASSERT_TRUE(element);

  // Subscribe to bounds changed event.
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kElementBoundsChangedEvent, element->context(), bounds_changed.Get());

  // Hide element - should NOT trigger bounds changed event.
  EXPECT_CALL(bounds_changed, Run).Times(0);
  handler_remote()->TrackedElementVisibilityChanged(element_name, false,
                                                    gfx::RectF());
  tracked_element_handler_remote_.FlushForTesting();
}

TEST_F(TrackedElementHandlerTest, MultipleIdentifiers) {
  // Show two elements.
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier2.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier1));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier2));

  // Hide one element.
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), false, gfx::RectF());
  tracked_element_handler_remote_.FlushForTesting();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier1));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier2));

  // Hide the other element.
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier2.GetName(), false, gfx::RectF());
  tracked_element_handler_remote_.FlushForTesting();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier1));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier2));

  // Re-show an element.
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier1));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTestElementIdentifier2));
}

TEST_F(TrackedElementHandlerTest, DestroyHandlerCleansUpElement) {
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTestElementIdentifier1);
  ASSERT_TRUE(element);
  const ui::ElementContext context = element->context();
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementIdentifier1, context));
  handler_.reset();
  tracked_element_handler_remote_.FlushForTesting();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kTestElementIdentifier1, context));
}

TEST_F(TrackedElementHandlerTest, GetNativeView) {
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTestElementIdentifier1);
  ASSERT_TRUE(element);
  EXPECT_TRUE(element->IsA<TrackedElementWebUI>());

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::CLIENT_OWNS_WIDGET;
  widget->Init(std::move(params));
  widget->Show();

  auto* webview = widget->SetClientContentsView(
      std::make_unique<views::WebView>(browser_context_.get()));
  webview->SetWebContents(web_contents_.get());

  // The element should return the native view of the widget.
  EXPECT_EQ(widget->GetNativeView(), element->GetNativeView());

  webview->SetWebContents(nullptr);
  widget->CloseNow();
}

// TODO(crbug.com/40243115): add tests for element screen bounds. This requires
// an update to the TestWebContents API to fake
// WebContents::GetContainerBounds().

}  // namespace ui
