// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/tracked_element_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_highlighter.h"
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

class TestTrackedElementManager
    : public tracked_element::mojom::TrackedElementManager {
 public:
  explicit TestTrackedElementManager(
      mojo::PendingReceiver<tracked_element::mojom::TrackedElementManager>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  void OnElementHighlightChanged(const std::string& native_identifier,
                                 bool highlighted) override {
    highlight_events_.emplace_back(native_identifier, highlighted);
  }

  void ClickElement(const std::string& native_identifier,
                    ClickElementCallback callback) override {
    interaction_events_.push_back("Click:" + native_identifier);
    std::move(callback).Run(true);
  }

  void FocusElement(const std::string& native_identifier,
                    FocusElementCallback callback) override {
    interaction_events_.push_back("Focus:" + native_identifier);
    std::move(callback).Run(true);
  }

  void SelectTab(const std::string& native_identifier,
                 uint32_t index,
                 SelectTabCallback callback) override {
    interaction_events_.push_back(base::StringPrintf(
        "SelectTab:%s:%u", native_identifier.c_str(), index));
    std::move(callback).Run(true);
  }

  void SelectDropdownItem(const std::string& native_identifier,
                          uint32_t index,
                          SelectDropdownItemCallback callback) override {
    interaction_events_.push_back(base::StringPrintf(
        "SelectDropdownItem:%s:%u", native_identifier.c_str(), index));
    std::move(callback).Run(true);
  }

  void EnterText(const std::string& native_identifier,
                 const std::u16string& text,
                 tracked_element::mojom::TextEntryMode mode,
                 EnterTextCallback callback) override {
    interaction_events_.push_back(base::StringPrintf(
        "EnterText:%s:%s:%d", native_identifier.c_str(),
        base::UTF16ToUTF8(text).c_str(), static_cast<int>(mode)));
    std::move(callback).Run(true);
  }

  void Confirm(const std::string& native_identifier,
               ConfirmCallback callback) override {
    interaction_events_.push_back("Confirm:" + native_identifier);
    std::move(callback).Run(true);
  }

  std::vector<std::pair<std::string, bool>> TakeHighlightEvents() {
    return std::move(highlight_events_);
  }

  std::vector<std::string> TakeInteractionEvents() {
    return std::move(interaction_events_);
  }

 private:
  mojo::Receiver<tracked_element::mojom::TrackedElementManager> receiver_{this};
  std::vector<std::pair<std::string, bool>> highlight_events_;
  std::vector<std::string> interaction_events_;
};

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

TEST_F(TrackedElementHandlerTest, CanHighlight) {
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier2.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();
  auto* const element1 =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTestElementIdentifier1);
  ASSERT_TRUE(element1);
  auto* const element2 =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTestElementIdentifier2);
  ASSERT_TRUE(element2);
  EXPECT_FALSE(
      ui::ElementHighlighter::GetElementHighlighter()->CanBeHighlighted(
          element1));
  EXPECT_FALSE(
      ui::ElementHighlighter::GetElementHighlighter()->CanBeHighlighted(
          element2));

  handler_remote()->TrackedElementCanHighlightChanged(
      kTestElementIdentifier1.GetName(), true);
  handler_remote()->TrackedElementCanHighlightChanged(
      kTestElementIdentifier2.GetName(), false);
  tracked_element_handler_remote_.FlushForTesting();

  EXPECT_TRUE(ui::ElementHighlighter::GetElementHighlighter()->CanBeHighlighted(
      element1));
  EXPECT_FALSE(
      ui::ElementHighlighter::GetElementHighlighter()->CanBeHighlighted(
          element2));
}

TEST_F(TrackedElementHandlerTest, Highlight) {
  mojo::Remote<tracked_element::mojom::TrackedElementManager> manager_remote;

  TestTrackedElementManager manager(
      manager_remote.BindNewPipeAndPassReceiver());

  handler_remote()->SetManager(manager_remote.Unbind());
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier2.GetName(), true, kElementBounds);
  handler_remote()->TrackedElementCanHighlightChanged(
      kTestElementIdentifier1.GetName(), true);
  handler_remote()->TrackedElementCanHighlightChanged(
      kTestElementIdentifier2.GetName(), false);
  tracked_element_handler_remote_.FlushForTesting();

  auto* element1 =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTestElementIdentifier1);
  ASSERT_TRUE(element1);
  auto* element2 =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTestElementIdentifier2);
  ASSERT_TRUE(element2);

  // Add 2 highlights, release 1, should just have one highlight event.
  auto hl1 =
      ui::ElementHighlighter::GetElementHighlighter()->AddHighlight(element1);
  ASSERT_TRUE(hl1);
  auto hl2 =
      ui::ElementHighlighter::GetElementHighlighter()->AddHighlight(element1);
  ASSERT_TRUE(hl2);
  hl1.reset();
  handler_->FlushManagerRemoteForTesting();
  EXPECT_THAT(
      manager.TakeHighlightEvents(),
      testing::ElementsAre(std::pair(kTestElementIdentifier1.GetName(), true)));

  // Release the remaining highlight on element 1, and try to acquire one
  // on element 2. The latter should return null since it's not highlightable.
  hl2.reset();
  auto hl3 =
      ui::ElementHighlighter::GetElementHighlighter()->AddHighlight(element2);
  EXPECT_FALSE(hl3);
  handler_->FlushManagerRemoteForTesting();
  EXPECT_THAT(manager.TakeHighlightEvents(),
              testing::ElementsAre(
                  std::pair(kTestElementIdentifier1.GetName(), false)));

  // Now enable highlighting for element2 as well.
  handler_remote()->TrackedElementCanHighlightChanged(
      kTestElementIdentifier2.GetName(), true);
  tracked_element_handler_remote_.FlushForTesting();

  // Grab and release HL on it.
  auto hl4 =
      ui::ElementHighlighter::GetElementHighlighter()->AddHighlight(element2);
  ASSERT_TRUE(hl4);
  hl4.reset();
  handler_->FlushManagerRemoteForTesting();
  EXPECT_THAT(manager.TakeHighlightEvents(),
              testing::ElementsAre(
                  std::pair(kTestElementIdentifier2.GetName(), true),
                  std::pair(kTestElementIdentifier2.GetName(), false)));
}

TEST_F(TrackedElementHandlerTest, Interaction) {
  mojo::Remote<tracked_element::mojom::TrackedElementManager> manager_remote;
  TestTrackedElementManager manager(
      manager_remote.BindNewPipeAndPassReceiver());

  handler_remote()->SetManager(manager_remote.Unbind());
  tracked_element_handler_remote_.FlushForTesting();

  const std::string name = kTestElementIdentifier1.GetName();

  EXPECT_TRUE(handler()->ClickElement(name));
  EXPECT_THAT(manager.TakeInteractionEvents(),
              testing::ElementsAre("Click:" + name));

  EXPECT_TRUE(handler()->FocusElement(name));
  EXPECT_THAT(manager.TakeInteractionEvents(),
              testing::ElementsAre("Focus:" + name));

  EXPECT_TRUE(handler()->SelectTab(name, 2));
  EXPECT_THAT(manager.TakeInteractionEvents(),
              testing::ElementsAre("SelectTab:" + name + ":2"));

  EXPECT_TRUE(handler()->SelectDropdownItem(name, 1));
  EXPECT_THAT(manager.TakeInteractionEvents(),
              testing::ElementsAre("SelectDropdownItem:" + name + ":1"));

  EXPECT_TRUE(handler()->EnterText(
      name, u"hello", tracked_element::mojom::TextEntryMode::kAppend));
  EXPECT_THAT(manager.TakeInteractionEvents(),
              testing::ElementsAre("EnterText:" + name + ":hello:2"));

  EXPECT_TRUE(handler()->Confirm(name));
  EXPECT_THAT(manager.TakeInteractionEvents(),
              testing::ElementsAre("Confirm:" + name));
}

TEST_F(TrackedElementHandlerTest,
       WebContentsVisibilityChangesElementVisibility) {
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kTestElementIdentifier1);
  ASSERT_TRUE(element);
  const ui::ElementContext context = element->context();
  EXPECT_TRUE(tracker->IsElementVisible(kTestElementIdentifier1, context));

  // Hide WebContents.
  handler()->OnVisibilityChanged(content::Visibility::HIDDEN);
  EXPECT_FALSE(tracker->IsElementVisible(kTestElementIdentifier1, context));

  // Show WebContents.
  handler()->OnVisibilityChanged(content::Visibility::VISIBLE);
  EXPECT_TRUE(tracker->IsElementVisible(kTestElementIdentifier1, context));
}

TEST_F(TrackedElementHandlerTest, DestroyHandlerHidesElement) {
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kTestElementIdentifier1);
  ASSERT_TRUE(element);
  const ui::ElementContext context = element->context();
  EXPECT_TRUE(tracker->IsElementVisible(kTestElementIdentifier1, context));

  // Destroy handler (simulates WebContents being destroyed).
  handler_.reset();
  EXPECT_FALSE(tracker->IsElementVisible(kTestElementIdentifier1, context));
}

TEST_F(TrackedElementHandlerTest, VisibilityLockPreventsHiding) {
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kTestElementIdentifier1);
  ASSERT_TRUE(element);
  const ui::ElementContext context = element->context();
  EXPECT_TRUE(tracker->IsElementVisible(kTestElementIdentifier1, context));

  // Acquire lock.
  auto lock = handler()->LockVisible(kTestElementIdentifier1.GetName());
  ASSERT_TRUE(lock);

  // Hide WebContents.
  handler()->OnVisibilityChanged(content::Visibility::HIDDEN);
  // Element should still be visible because of the lock.
  EXPECT_TRUE(tracker->IsElementVisible(kTestElementIdentifier1, context));

  // Release lock.
  lock.reset();
  // Now it should be hidden.
  EXPECT_FALSE(tracker->IsElementVisible(kTestElementIdentifier1, context));
}

TEST_F(TrackedElementHandlerTest, MultipleVisibilityLocks) {
  handler_remote()->TrackedElementVisibilityChanged(
      kTestElementIdentifier1.GetName(), true, kElementBounds);
  tracked_element_handler_remote_.FlushForTesting();

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kTestElementIdentifier1);
  ASSERT_TRUE(element);
  const ui::ElementContext context = element->context();

  auto lock1 = handler()->LockVisible(kTestElementIdentifier1.GetName());
  auto lock2 = handler()->LockVisible(kTestElementIdentifier1.GetName());

  handler()->OnVisibilityChanged(content::Visibility::HIDDEN);
  EXPECT_TRUE(tracker->IsElementVisible(kTestElementIdentifier1, context));

  lock1.reset();
  EXPECT_TRUE(tracker->IsElementVisible(kTestElementIdentifier1, context));

  lock2.reset();
  EXPECT_FALSE(tracker->IsElementVisible(kTestElementIdentifier1, context));
}

// TODO(crbug.com/40243115): add tests for element screen bounds. This requires
// an update to the TestWebContents API to fake
// WebContents::GetContainerBounds().

}  // namespace ui
